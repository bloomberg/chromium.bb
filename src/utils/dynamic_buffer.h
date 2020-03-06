#ifndef LIBGAV1_SRC_UTILS_DYNAMIC_BUFFER_H_
#define LIBGAV1_SRC_UTILS_DYNAMIC_BUFFER_H_

#include <memory>

#include "src/utils/memory.h"

namespace libgav1 {

template <typename T>
class DynamicBuffer {
 public:
  T* get() { return buffer_.get(); }

  // Resizes the buffer so that it can hold at least |size| elements. Existing
  // contents will be destroyed when resizing to a larger size.
  //
  // Returns true on success. If Resize() returns false, then subsequent calls
  // to get() will return nullptr.
  bool Resize(size_t size) {
    if (size <= size_) return true;
    buffer_.reset(new (std::nothrow) T[size]);
    if (buffer_ == nullptr) {
      size_ = 0;
      return false;
    }
    size_ = size;
    return true;
  }

 private:
  // Used only when |aligned| is false.
  std::unique_ptr<T[]> buffer_;
  size_t size_ = 0;
};

template <typename T, int alignment>
class AlignedDynamicBuffer {
 public:
  T* get() { return buffer_.get(); }

  // Resizes the buffer so that it can hold at least |size| elements. Existing
  // contents will be destroyed when resizing to a larger size.
  //
  // Returns true on success. If Resize() returns false, then subsequent calls
  // to get() will return nullptr.
  bool Resize(size_t size) {
    if (size <= size_) return true;
    buffer_ = MakeAlignedUniquePtr<T>(alignment, size);
    if (buffer_ == nullptr) {
      size_ = 0;
      return false;
    }
    size_ = size;
    return true;
  }

 private:
  AlignedUniquePtr<T> buffer_;
  size_t size_ = 0;
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_UTILS_DYNAMIC_BUFFER_H_
