#ifndef NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_DEBUG_BLOB_H_
#define NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_DEBUG_BLOB_H_

#include <deque>

// Class for working with raw binary data.

typedef unsigned char byte;

namespace debug {
class Blob {
 public:
  Blob();
  Blob(const Blob& other);
  Blob(const void* buff, size_t buff_sz);
  Blob(const char* buff);
  virtual ~Blob();
  Blob& operator = (const Blob& other);
  bool operator == (const Blob& other) const;
  
  size_t Size() const;
  byte operator[] (size_t position) const;
  byte Front() const;  // Returns first byte.
  byte Back() const;  // Returns last byte.
  byte PopFront();  // Delete first byte.
  byte PopBack();  // Delete last byte.
  void PushFront(byte c);  // Insert byte at beginning.
  void PushBack(byte c);  // Add byte at the end.
  void Append(const Blob& other);
  void Clear();

  std::string ToString() const;
  std::string ToHexString(bool remove_leading_zeroes=true) const;
  bool LoadFromHexString(const std::string& hex_str);
  bool LoadFromHexString(const Blob& hex_str);
  void Reverse();
  bool Compare(const Blob& blob, size_t to_length = -1) const;
  bool HasPrefix(const std::string& prefix) const;

  int ToInt() const;
  void Split(const char* delimiters, std::deque<Blob>* tokens);

  static bool HexCharToInt(byte c, unsigned int* result);

 protected:
  std::deque<byte> value_;
};

class BlobUniTest {
 public:
  BlobUniTest();
  int Run(std::string* error);  // returns 0 if success, error code if failed.
   
};

}  // namespace debug
#endif  // NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_DEBUG_BLOB_H_