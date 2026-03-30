#include "weld.h"
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace weld {
bool MappedFile::map(const char* path) {
  int fd = ::open(path, O_RDONLY);
  if (fd == -1)
    return false;

  struct stat st;
  if (fstat(fd, &st) == -1)
    return false;

  if (st.st_size > 0) {
    ptr_ = static_cast<u8*>(
        mmap(nullptr, st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0));
    if (ptr_ == MAP_FAILED)
      return false;
  }

  close(fd);
  return true;
}

void MappedFile::unmap() {
  if (size_ == 0 || !ptr_)
    return;
  munmap(ptr_, size_);
  ptr_ = nullptr;
}
} // namespace weld
