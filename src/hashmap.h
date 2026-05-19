#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>


class HashMapError : std::runtime_error {
	std::string message;

public:

	explicit HashMapError(std::string m) : std::runtime_error("") {
		message = "Something occuried in hashmap: " + m;
	}

	const char* what() const noexcept override {
		return message.c_str();
	}
};

template<typename K, typename V>
class LockFreeHashMap {

	struct Node {
        K key;
        V value;
        std::atomic<Node*> next;
        std::atomic<bool> deleted; 

        Node(const K& k, const V& v) : key(k), value(v), next(nullptr), deleted(false) {}
    };

    std::vector<std::atomic<Node*>> buckets;
    size_t capacity;
    std::atomic<size_t> entry_count{0};
    std::hash<K> hasher;

    size_t bucket_index(const K& key) const {
    	return hasher(key) % capacity;
    }

    Node* find_node(const K& key) const {
        size_t index = bucket_index(key);
        Node* current = buckets[index].load(std::memory_order_acquire);
        while (current != nullptr) {
            if (!current->deleted.load(std::memory_order_acquire) && current->key == key) {
                return current;
            }
            current = current->next.load(std::memory_order_acquire);
        }
        return nullptr;
}

public:

    class iterator {
    private:

        const LockFreeHashMap& map;
        Node* current_node;
        size_t current_bucket;

        void find_first_node() {
            for (size_t i = 0; i < map.buckets.size(); i++) {
                Node* node = map.buckets[i].load(std::memory_order_acquire);
                while (node != nullptr) {
                    if (!node->deleted.load(std::memory_order_acquire)) {
                        current_bucket = i;
                        current_node = node;
                        return;
                    }
                    node = node->next.load(std::memory_order_acquire);
                }
            }
            current_bucket = map.buckets.size();
            current_node = nullptr;
        }

        void find_next_node() {
            if (current_node == nullptr) {
                return;
            }
            Node* next = current_node->next.load(std::memory_order_acquire);
            while (next != nullptr) { 
                if (!next->deleted.load(std::memory_order_acquire)) {
                    current_node = next;
                    return;
                }
                next = next->next.load(std::memory_order_acquire);
            }
            for (size_t i = current_bucket + 1; i < map.buckets.size(); ++i) {
                Node* node = map.buckets[i].load(std::memory_order_acquire);
                while (node != nullptr) {
                    if (!node->deleted.load(std::memory_order_acquire)) {
                        current_bucket = i;
                        current_node = node;
                        return;
                    }
                    node = node->next.load(std::memory_order_acquire);
                }
            }
            current_bucket = map.buckets.size();
            current_node = nullptr;
        }



    public:

        iterator(const LockFreeHashMap& hashmap, bool start) : map(hashmap), current_bucket(0), current_node(nullptr) {
            if (start) {
                find_first_node();
            } else {
                current_bucket = map.buckets.size();  
            }
        }

        std::pair<const K&, V&> operator*() const {
            return {current_node->key, current_node->value};
        }

        Node* operator->() const {
            return current_node; 
        }

        iterator& operator++() {
            find_next_node();
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            find_next_node();
            return tmp;
        }

        bool operator==(const iterator& other) const {
            return current_node == other.current_node;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    };

    iterator begin() const {
        return iterator(*this, true); 
    }
    
    iterator end()   const {
        return iterator(*this, false); 
    }
    

	LockFreeHashMap(size_t initial_capacity = 16) : buckets(initial_capacity), capacity(initial_capacity) {
        for (auto& bucket : buckets) {
            bucket.store(nullptr, std::memory_order_relaxed);
        }
    }

    ~LockFreeHashMap() {
        for (auto& bucket : buckets) {
            Node* current = bucket.load(std::memory_order_relaxed);
            while (current != nullptr) {
                Node* next = current->next.load(std::memory_order_relaxed);
                delete current;
                current = next;
            }
        }
    }

    bool contains(const K& key) const {
        return find_node(key) != nullptr;
    }

    bool insert(const K& key, const V& value) {
        Node* existing = find_node(key);
        if (existing) {
            existing->value = value;
            return false;
        }
        size_t index = bucket_index(key);
        Node* new_node = new Node(key, value);
        while (true) {
            Node* head = buckets[index].load(std::memory_order_acquire);
            new_node->next.store(head, std::memory_order_relaxed);
            if (buckets[index].compare_exchange_weak(
                    head, new_node,
                    std::memory_order_release,
                    std::memory_order_acquire)) {
                entry_count.fetch_add(1, std::memory_order_release);
                return true;
            }
        }
    }

    V& at(const K& key) {
        Node* node = find_node(key);
        if (!node) {
            throw HashMapError("No key in map");
        }
        return node->value;
    }

    const V& at(const K& key) const {
        Node* node = find_node(key);
        if (!node) {
            throw HashMapError("No key in map");
        }
        return node->value;
    }

    bool get(const K& key, V& value) const {
        size_t index = bucket_index(key);
        Node* current = buckets[index].load(std::memory_order_acquire);
        while (current != nullptr) {
            if (!current->deleted.load(std::memory_order_acquire) && current->key == key) {
                value = current->value;
                return true;
            }
            current = current->next.load(std::memory_order_acquire);
        }
        return false;
    }

    bool remove(const K& key) {
        size_t index = bucket_index(key);
        Node* current = buckets[index].load(std::memory_order_acquire);
        while (current != nullptr) {
            if (!current->deleted.load(std::memory_order_acquire) && current->key == key) {
                bool expected = false;
                if (current->deleted.compare_exchange_strong(
                        expected, true,
                        std::memory_order_release,
                        std::memory_order_relaxed)) {
                    entry_count.fetch_sub(1, std::memory_order_release);
                    return true;
                }
            }
            current = current->next.load(std::memory_order_acquire);
        }
        return false;
    }

    size_t size() const {
        return entry_count.load(std::memory_order_acquire);
    }	
};