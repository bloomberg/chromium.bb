#include "debug_blob.h"
#include <algorithm>
#include <string>
#include <ctype.h>

namespace {
char GetHexDigit(unsigned int value, int digit_position) {
//  const char* kDigitStrings = "0123456789ABCDEF";
  const char* kDigitStrings = "0123456789abcdef";
  unsigned int digit = (value >> (4 * digit_position)) & 0xF;
  return kDigitStrings[digit];
}
}  // namespace

namespace debug {
Blob::Blob() {
}

Blob::Blob(const Blob& other) {
  value_ = other.value_;
}

Blob::Blob(const void* buff, size_t buff_sz) {
  const byte* char_buff = static_cast<const byte*>(buff);
  for (size_t i = 0; i < buff_sz; i++ )
    PushBack(*char_buff++);
}

Blob::Blob(const char* buff) {
  if (NULL != buff) {
    for (size_t i = 0; i < strlen(buff); i++ )
      PushBack(buff[i]);
  }
}

Blob::~Blob() {
}

Blob& Blob::operator = (const Blob& other) {
  value_ = other.value_;
  return *this;
}

bool Blob::operator == (const Blob& other) const {
  return value_ == other.value_;
}

size_t Blob::Size() const {
  return value_.size();
}

byte Blob::operator[] (size_t position) const {
  return value_[position];
}

byte Blob::Front() const {
  return value_.front();
}

byte Blob::Back() const {
  return value_.back();
}

byte Blob::PopFront() {
  byte c = value_.front();
  value_.pop_front();
  return c;
}

byte Blob::PopBack() {
  byte c = value_.back();
  value_.pop_back();
  return c;
}

void Blob::PushFront(byte c) {
  value_.push_front(c);
}

void Blob::PushBack(byte c) {
  value_.push_back(c);
}

void Blob::Clear() {
  value_.clear();
}

int Blob::ToInt() const {
  int result = 0;
  char* ptr = reinterpret_cast<char*>(&result);
  size_t bytes_to_copy = std::min(sizeof(result), Size());
  for (size_t i = 0; i < bytes_to_copy; i++)
    ptr[i] = value_[i];
  return result;
}

std::string Blob::ToString() const {
  std::string result;
  for (size_t i = 0; i < Size(); i++)
    result.append(1, value_[i]);
  return result;
}

std::string Blob::ToHexString(bool remove_leading_zeroes) const {
  std::string result;
  size_t num = Size();
  for (size_t i = 0; i < num; i++) {
    char c = GetHexDigit(value_[i], 1);
    result.append(1, c);
    c = GetHexDigit(value_[i], 0);
    result.append(1, c);
  }

  if (remove_leading_zeroes) {
    while (result[0] == '0')
      result.erase(0, 1);
  }
  return result;
}

bool Blob::LoadFromHexString(const std::string& hex_str) {
  Blob tmp(hex_str.c_str());
  return LoadFromHexString(tmp);
}

bool Blob::LoadFromHexString(const Blob& hex_str) {
  Clear();
  size_t num = hex_str.Size();
  for (size_t i = num; i > 0; i -= 2) {
    if (isspace(hex_str[i - 1])) {
      i++;
    } else if (i >= 2) {
      unsigned int c1 = 0;
      unsigned int c2 = 0;
      if (!HexCharToInt(hex_str[i - 1], &c1)) return false;
      if (!HexCharToInt(hex_str[i - 2], &c2)) return false;
      unsigned int c = c1 + (c2 << 4);
      PushFront(c);
    } else {
      unsigned int c = 0;
      if (!HexCharToInt(hex_str[i - 1], &c)) return false;
      PushFront(c);
      break;
    }
  }
  return true;
}

void Blob::Append(const Blob& blob) {
  for (size_t i = 0; i < blob.Size(); i++)
    PushBack(blob[i]);
}

bool Blob::Compare(const Blob& blob, size_t to_length) const {
  if (-1 == to_length) {
    if (Size() != blob.Size())
      return false;
    to_length = Size();
  }
  for (size_t i = 0; i < to_length; i++)
    if (value_[i] != blob[i])
      return false;
  return true;
}

bool Blob::HasPrefix(const std::string& prefix) const {
  size_t num = prefix.size();
  if (Size() < num)
    return false;
  for (size_t i = 0; i < num; i++)
    if (value_[i] != prefix[i])
      return false;
  return true;
}

void Blob::Reverse() {
  std::reverse(value_.begin(), value_.end());
}

void Blob::Split(const char* delimiters, std::deque<Blob>* tokens) {
  Blob token;
  std::deque<byte>::const_iterator it = value_.begin();
  while (value_.end() != it) {
    byte c = *it++;
    if (strchr(delimiters, c) != 0) {
      tokens->push_back(token);
      token.Clear();
    } else {
      token.PushBack(c);
    }
  }
  if (0 != token.Size())
    tokens->push_back(token);
}

bool Blob::HexCharToInt(unsigned char c, unsigned int* result) {
  if (('0' <= c) && ('9' >= c)) {
    *result = c - '0';
  }
  else if (('A' <= c) && ('F' >= c)) {
    *result = c - 'A' + 10;
  }
  else if (('a' <= c) && ('f' >= c)) {
    *result = c - 'a' + 10;
  }
  else {
    return false;
  }
  return true;
}

//--------------------------------------------------------------------------------------------//
#define my_assert(x) do{ if(!(x)) {*error="Failed: "#x; return __LINE__;}} while(false)

BlobUniTest::BlobUniTest() {
}

int BlobUniTest::Run(std::string* error) {
  if (true) {
    unsigned char buff[] = {0x3, 0xAF, 0xF0};
    Blob blob(buff, sizeof(buff));
    std::string str = blob.ToHexString(true);
    my_assert(str == "3aff0");

    str = blob.ToHexString(false);
    my_assert(str == "03aff0");

    my_assert(blob[0] == 0x3);
    my_assert(blob[1] == 0xAF);
    my_assert(blob[2] == 0xF0);
    my_assert(blob.Size() == 3);

    Blob blob2(blob);
    my_assert(blob2 == blob);
    my_assert(blob2[0] == 0x3);
    my_assert(blob2[1] == 0xAF);
    my_assert(blob2[2] == 0xF0);
    my_assert(blob2.Size() == 3);

    Blob blob3;
    blob3 = blob;
    my_assert(blob3 == blob);
    my_assert(blob3[0] == 0x3);
    my_assert(blob3[1] == 0xAF);
    my_assert(blob3[2] == 0xF0);
    my_assert(blob3.Size() == 3);

    Blob blob4;
    blob4.LoadFromHexString(std::string("3aff0"));
    my_assert(blob4 == blob);

    Blob blob5;
    blob5.LoadFromHexString(std::string("03aff0"));
    my_assert(blob5 == blob);

    Blob blob6(std::string("03aff0").c_str());
    Blob blob7;
    blob7.LoadFromHexString(blob6);
    my_assert(blob7 == blob);

  }

  if (true) {
    unsigned char buff[] = {0, 0x3, 0xAF, 0xF0};
    Blob blob(buff, sizeof(buff));
    std::string str = blob.ToHexString(true);
    my_assert(str == "3aff0");

    str = blob.ToHexString(false);
    my_assert(str == "0003aff0");

    my_assert(blob[0] == 0);
    my_assert(blob[1] == 0x3);
    my_assert(blob[2] == 0xAF);
    my_assert(blob[3] == 0xF0);
    my_assert(blob.Size() == 4);
  }

  return 0;
} // returns 0 if success, error code if failed.

}  // namespace debug