#include "weld.h"
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace weld {
bool MappedFile::map(const char* path) {
  int fd = ::open(path, O_RDONLY);

  bool result = [fd, this]() {
    struct stat st;
    if (fd == -1)
      return false;
    if (fstat(fd, &st) == -1)
      return false;
    if (st.st_size <= 0)
      return false;

    void* new_ptr =
        mmap(nullptr, st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (new_ptr == MAP_FAILED)
      return false;
    this->ptr_ = static_cast<u8*>(new_ptr);
    this->size_ = st.st_size;
    return true;
  }();

  close(fd);
  return result;
}

void MappedFile::unmap() {
  if (size_ == 0 || !ptr_)
    return;
  munmap(ptr_, size_);
  ptr_ = nullptr;
}
} // namespace weld
