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

    bool insert(const K& key, const V& value) {
        size_t index = bucket_index(key);
        Node* new_node = new Node(key, value);
        while (true) {
            Node* head = buckets[index].load(std::memory_order_acquire);
            new_node->next.store(head, std::memory_order_relaxed);
            if (buckets[index].compare_exchange_weak(
                    head, new_node,
                    std::memory_order_release,
                    std::memory_order_acquire)) {
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
                    return true;
                }
            }
            current = current->next.load(std::memory_order_acquire);
        }
        return false;
    }

    size_t size() const {
        return capacity;
    }	
};