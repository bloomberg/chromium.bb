/* -*- c++ -*- */
/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/serialization/serialization.h"

#include <stdio.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"

namespace nacl {

int const kInitialBufferSize = 256;

SerializationBuffer::SerializationBuffer()
    : nbytes_(0)
    , in_use_(0)
    , read_ix_(0) {}


SerializationBuffer::SerializationBuffer(uint8_t const *data_buffer,
                                         size_t nbytes)
    : nbytes_(0)  // EnsureTotalSize will update
    , in_use_(nbytes)
    , read_ix_(0) {
  EnsureTotalSize(nbytes);
  memcpy(&buffer_[0], data_buffer, nbytes);
}

bool SerializationBuffer::Serialize(char const *cstr, size_t char_count) {
  if (char_count > ~(uint32_t) 0) {
    return false;
  }
  AddTag<char *>();
  AddVal(static_cast<uint32_t>(char_count));
  for (size_t ix = 0; ix < char_count; ++ix) {
    AddVal<uint8_t>(cstr[ix]);
  }
  return true;
}

bool SerializationBuffer::Serialize(char const *cstr) {
  size_t len = strlen(cstr) + 1;  // The ASCII character NUL is included
  return Serialize(cstr, len);
}

bool SerializationBuffer::Serialize(std::string str) {
  size_t bytes = str.size();
  if (bytes > ~(uint32_t) 0) {
    return false;
  }
  AddTag<std::string>();
  AddVal(static_cast<uint32_t>(bytes));
  for (size_t ix = 0; ix < bytes; ++ix) {
    AddVal<uint8_t>(str[ix]);
  }
  return true;
}

bool SerializationBuffer::Deserialize(char *cstr, size_t *buffer_size) {
  size_t orig = cur_read_pos();
  if (bytes_unread() < kTagBytes + SerializationTraits<uint32_t>::kBytes) {
    return false;
  }
  if (ReadTag() != SerializationTraits<char *>::kTag) {
    reset_read_pos(orig);
    return false;
  }
  uint32_t char_count;
  if (!GetUint32(&char_count)) {
    reset_read_pos(orig);
    return false;
  }
  if (char_count > *buffer_size) {
    *buffer_size = char_count;
    reset_read_pos(orig);
    return true;  // true means check buffer_size!
  }
  for (size_t ix = 0; ix < char_count; ++ix) {
    uint8_t byte;
    if (!GetVal(&byte)) {
      reset_read_pos(orig);
      return false;  // encoded data is garbled!
    }
    cstr[ix] = byte;
  }
  *buffer_size = char_count;
  return true;
}

bool SerializationBuffer::Deserialize(char **cstr_out) {
  size_t nbytes = 256;
  char *buffer = new char[nbytes];

  size_t used = nbytes;
  if (!Deserialize(buffer, &used)) {
    delete[] buffer;
    return false;
  }
  if (used > nbytes) {
    delete[] buffer;
    buffer = new char[used];
    CHECK(Deserialize(buffer, &used));
  }
  *cstr_out = buffer;
  return true;
}

bool SerializationBuffer::Deserialize(std::string *str) {
  size_t orig = cur_read_pos();
  if (bytes_unread() < kTagBytes + SerializationTraits<uint32_t>::kBytes) {
    return false;
  }
  if (ReadTag() != SerializationTraits<std::string>::kTag) {
    reset_read_pos(orig);
    return false;
  }
  uint32_t bytes;
  if (!GetUint32(&bytes)) {
    reset_read_pos(orig);
    return false;
  }
  for (size_t ix = 0; ix < bytes; ++ix) {
    uint8_t b;
    if (!GetUint8(&b)) {
      reset_read_pos(orig);
      return false;
    }
    str->push_back(b);
  }
  return true;
}

void SerializationBuffer::AddUint8(uint8_t value) {
  EnsureAvailableSpace(sizeof value);
  buffer_[in_use_] = value;
  in_use_ += sizeof value;
}

void SerializationBuffer::AddUint16(uint16_t value) {
  EnsureAvailableSpace(sizeof value);
  buffer_[in_use_ + 0] = static_cast<uint8_t>(value >> 0);
  buffer_[in_use_ + 1] = static_cast<uint8_t>(value >> 8);
  in_use_ += sizeof value;
}

void SerializationBuffer::AddUint32(uint32_t value) {
  EnsureAvailableSpace(sizeof value);
  buffer_[in_use_ + 0] = static_cast<uint8_t>(value >> 0);
  buffer_[in_use_ + 1] = static_cast<uint8_t>(value >> 8);
  buffer_[in_use_ + 2] = static_cast<uint8_t>(value >> 16);
  buffer_[in_use_ + 3] = static_cast<uint8_t>(value >> 24);
  in_use_ += sizeof value;
}

void SerializationBuffer::AddUint64(uint64_t value) {
  EnsureAvailableSpace(sizeof value);
  buffer_[in_use_ + 0] = static_cast<uint8_t>(value >> 0);
  buffer_[in_use_ + 1] = static_cast<uint8_t>(value >> 8);
  buffer_[in_use_ + 2] = static_cast<uint8_t>(value >> 16);
  buffer_[in_use_ + 3] = static_cast<uint8_t>(value >> 24);
  buffer_[in_use_ + 4] = static_cast<uint8_t>(value >> 32);
  buffer_[in_use_ + 5] = static_cast<uint8_t>(value >> 40);
  buffer_[in_use_ + 6] = static_cast<uint8_t>(value >> 48);
  buffer_[in_use_ + 7] = static_cast<uint8_t>(value >> 56);
  in_use_ += sizeof value;
}

bool SerializationBuffer::GetUint8(uint8_t *value) {
  if (bytes_unread() < sizeof *value) {
    return false;
  }
  *value = static_cast<uint8_t>(buffer_[read_ix_]);
  read_ix_ += sizeof *value;
  return true;
}

bool SerializationBuffer::GetUint16(uint16_t *value) {
  if (bytes_unread() < sizeof *value) {
    return false;
  }
  *value = ((static_cast<uint16_t>(buffer_[read_ix_ + 0]) << 0) |
            (static_cast<uint16_t>(buffer_[read_ix_ + 1]) << 8));
  read_ix_ += sizeof *value;
  return true;
}

bool SerializationBuffer::GetUint32(uint32_t *value) {
  if (bytes_unread() < sizeof *value) {
    return false;
  }
  *value = ((static_cast<uint32_t>(buffer_[read_ix_ + 0]) << 0) |
            (static_cast<uint32_t>(buffer_[read_ix_ + 1]) << 8) |
            (static_cast<uint32_t>(buffer_[read_ix_ + 2]) << 16) |
            (static_cast<uint32_t>(buffer_[read_ix_ + 3]) << 24));
  read_ix_ += sizeof *value;
  return true;
}

bool SerializationBuffer::GetUint64(uint64_t *value) {
  if (bytes_unread() < sizeof *value) {
    return false;
  }
  *value = ((static_cast<uint64_t>(buffer_[read_ix_ + 0]) << 0) |
            (static_cast<uint64_t>(buffer_[read_ix_ + 1]) << 8) |
            (static_cast<uint64_t>(buffer_[read_ix_ + 2]) << 16) |
            (static_cast<uint64_t>(buffer_[read_ix_ + 3]) << 24) |
            (static_cast<uint64_t>(buffer_[read_ix_ + 4]) << 32) |
            (static_cast<uint64_t>(buffer_[read_ix_ + 5]) << 40) |
            (static_cast<uint64_t>(buffer_[read_ix_ + 6]) << 48) |
            (static_cast<uint64_t>(buffer_[read_ix_ + 7]) << 56));
  read_ix_ += sizeof *value;
  return true;
}

#if defined(NACL_HAS_IEEE_754)
bool SerializationBuffer::GetFloat(float *value) {
  union ieee754_float v;
  uint32_t encoded = 0;
  if (!GetUint32(&encoded)) {
    return false;
  }
  v.ieee.negative = encoded >> 31;
  v.ieee.exponent = encoded >> 23;
  v.ieee.mantissa = encoded;
  *value = v.f;
  return true;
}

bool SerializationBuffer::GetDouble(double *value) {
  union ieee754_double v;
  uint64_t encoded;
  if (!GetUint64(&encoded)) {
    return false;
  }
  v.ieee.negative = encoded >> 63;
  v.ieee.exponent = encoded >> 52;
  v.ieee.mantissa0 = encoded >> 32;
  v.ieee.mantissa1 = encoded;
  *value = v.d;
  return true;
}

bool SerializationBuffer::GetLongDouble(long double *value) {
  union ieee854_long_double v;
  uint16_t encoded1;
  uint64_t encoded2;
  if (in_use_ < read_ix_ + 10) {
    return false;
  }
  if (!GetUint16(&encoded1) || !GetUint64(&encoded2)) {
    return false;
  }
  v.ieee.negative = (encoded1 >> 15) & 1;
  v.ieee.exponent = encoded1;
  v.ieee.mantissa0 = encoded2 >> 32;
  v.ieee.mantissa1 = encoded2;
  *value = v.d;
  return true;
}
#endif

void SerializationBuffer::EnsureTotalSize(size_t req_size) {
  if (nbytes_ >= req_size) {
    return;
  }
  size_t new_size = (0 == nbytes_) ? kInitialBufferSize : 2 * nbytes_;
  CHECK(new_size > nbytes_);  // no arithmetic overflow
  if (new_size < req_size) {
    new_size = req_size;
  }
  buffer_.resize(new_size);
  nbytes_ = new_size;
}

void SerializationBuffer::EnsureAvailableSpace(size_t req_space) {
  CHECK(nbytes_ >= in_use_);
  CHECK((~(size_t) 0) - in_use_ >= req_space);
  size_t new_size = in_use_ + req_space;
  EnsureTotalSize(new_size);
}

}  // namespace nacl
