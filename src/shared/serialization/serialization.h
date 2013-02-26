/* -*- c++ -*- */
/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_SERIALIZATION_SERIALIZATION_H_
#define NATIVE_CLIENT_SRC_SHARED_SERIALIZATION_SERIALIZATION_H_

#if defined(__native_client__) || NACL_LINUX
# define NACL_HAS_IEEE_754
// Make sure fp is not dead code and is tested.  DO NOT USE the fp
// interface until we have a portability version of ieee754.h!
#endif

#if defined(NACL_HAS_IEEE_754)
# include <ieee754.h>
#endif

#include <vector>
#include <string>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_compiler_annotations.h"
#include "native_client/src/shared/platform/nacl_check.h"

// SerializationBuffer enables serializing basic types and vectors

namespace nacl {

class SerializationBuffer;

template<typename T> class SerializationTraits;

enum {
  kIllegalTag = -1,

  kUint8 = 0,
  kInt8 = 1,
  kUint16 = 2,
  kInt16 = 3,
  kUint32 = 4,
  kInt32 = 5,
  kUint64 = 6,
  kInt64 = 7,

#if defined(NACL_HAS_IEEE_754)
  kFloat = 8,
  kDouble = 9,
  kLongDouble = 10,
#endif

  kCString = 11,
  kString = 12,

  kRecursiveVector = 31,
  kVectorOffset = 32
};

class SerializationBuffer {
 public:
  SerializationBuffer();

  // This initializes the Serialization buffer from |data_buffer|
  // containing |nbytes| of data.  A copy of the data is made rather
  // than transferring ownership, which is suboptimal.
  SerializationBuffer(uint8_t const *data_buffer, size_t nbytes);

  template<typename T> bool Serialize(T basic) NACL_WUR;

  template<typename T> bool Serialize(std::vector<T> const& v) NACL_WUR;

  bool Serialize(char const *cstr) NACL_WUR;
  bool Serialize(char const *cstr, size_t char_count) NACL_WUR;

  bool Serialize(std::string str) NACL_WUR;

  int ReadTag() {
    if (bytes_unread() < kTagBytes) {
      return kIllegalTag;
    }
    return buffer_[read_ix_++];
  }

  template<typename T> bool Deserialize(T *basic) NACL_WUR;

  template<typename T> bool Deserialize(std::vector<T> *v) NACL_WUR;

  // This function deserializes into the provided buffer at |cstr|.
  // The parameter *buffer_size is an in-out parameter, initially
  // containing the available space at |cstr|.  If there are decoding
  // errors, this function returns false.  If it returns true, the
  // caller should check *buffer_size -- if there were insufficient
  // space, the read position is unchanged and *buffer_size is updated
  // to reflect the amount of space that is required; otherwise
  // *buffer_size is updated to reflect the actual number of bytes
  // written to |cstr|.
  bool Deserialize(char *cstr, size_t *buffer_size) NACL_WUR;
  // caller provides buffer

  // This method deserializes a NUL-terminated C-style string.  The
  // caller receives ownnership of the memory allocated via new[] and
  // is responsible for delete[]ing it to release the storage.
  bool Deserialize(char **cstr_out) NACL_WUR;

  bool Deserialize(std::string *str) NACL_WUR;

  size_t num_bytes() const {
    return in_use_;
  }

  uint8_t const *data() const {
    // return buffer_.data();  // C++11 only, not available on windows
    return &buffer_[0];
  }

  void rewind() {
    read_ix_ = 0;
  }

  void reset() {
    in_use_ = 0;
    buffer_.clear();
    nbytes_ = 0;
    read_ix_ = 0;
  }

  static const size_t kTagBytes = 1;

 protected:
  template<typename T> void AddTag();

  template<typename T> bool CheckTag();

  void AddUint8(uint8_t value);
  void AddUint16(uint16_t value);
  void AddUint32(uint32_t value);
  void AddUint64(uint64_t value);
#if defined(NACL_HAS_IEEE_754)
  void AddFloat(float value);
  void AddDouble(double value);
  void AddLongDouble(long double value);
#endif

  bool GetUint8(uint8_t *val);
  bool GetUint16(uint16_t *val);
  bool GetUint32(uint32_t *val);
  bool GetUint64(uint64_t *val);
#if defined(NACL_HAS_IEEE_754)
  bool GetFloat(float *value);
  bool GetDouble(double *value);
  bool GetLongDouble(long double *value);
#endif

  template<typename T> void AddVal(T value) {
    int T_must_be_integral_type[static_cast<T>(1)];
    UNREFERENCED_PARAMETER(T_must_be_integral_type);
    if (sizeof(T) == 1) {
      AddUint8(static_cast<uint8_t>(value));
    } else if (sizeof(T) == 2) {
      AddUint16(static_cast<uint16_t>(value));
    } else if (sizeof(T) == 4) {
      AddUint32(static_cast<uint32_t>(value));
    } else if (sizeof(T) == 8) {
      AddUint64(static_cast<uint64_t>(value));
    }
  }

  template<typename T> bool GetVal(T *basic) {
    int T_must_be_integral_type[static_cast<T>(1)];
    UNREFERENCED_PARAMETER(T_must_be_integral_type);
    if (sizeof(T) == 1) {
      uint8_t val;
      return GetUint8(&val) ? ((*basic = static_cast<T>(val)), true) : false;
    } else if (sizeof(T) == 2) {
      uint16_t val;
      return GetUint16(&val) ? ((*basic = static_cast<T>(val)), true) : false;
    } else if (sizeof(T) == 4) {
      uint32_t val;
      return GetUint32(&val) ? ((*basic = static_cast<T>(val)), true) : false;
    } else if (sizeof(T) == 8) {
      uint64_t val;
      return GetUint64(&val) ? ((*basic = static_cast<T>(val)), true) : false;
    }
    return false;
  }

#if defined(NACL_HAS_IEEE_754)
  void AddVal(float value) {
    AddFloat(value);
  }

  bool GetVal(float *value) {
    return GetFloat(value);
  }

  void AddVal(double value) {
    AddDouble(value);
  }

  bool GetVal(double *value) {
    return GetDouble(value);
  }

  void AddVal(long double value) {
    AddLongDouble(value);
  }

  bool GetVal(long double *value) {
    return GetLongDouble(value);
  }
#endif

  template<typename T, bool nested_tagging>
  bool Serialize(std::vector<T> const& v);

  // Template metaprogramming to determine at compile time, based on
  // whether the type T is a container type or not, whether to tag the
  // elements with their own type tag, or to just write the elements
  // sans type tag.  For vector containers of simple types such as
  // int8_t, tagging every byte is excessive overhead.  NB: see the
  // definition below of kTag for vectors.
  template<typename T, bool nested_tagging> class SerializeHelper {
   public:
    static bool DoSerialize(SerializationBuffer *buf,
                            std::vector<T> const& v) {
      size_t orig = buf->cur_write_pos();
      size_t num_elt = v.size();
      if (num_elt > ~(uint32_t) 0) {
        return false;
      }
      buf->AddTag<std::vector<T> >();
      buf->AddVal(static_cast<uint32_t>(num_elt));

      for (size_t ix = 0; ix < v.size(); ++ix) {
        if (!buf->Serialize(v[ix])) {
          buf->reset_write_pos(orig);
          return false;
        }
      }
      return true;
    }
  };

  template<typename T> class SerializeHelper<T, false> {
   public:
    static bool DoSerialize(SerializationBuffer *buf,
                            std::vector<T> const& v) {
      size_t num_elt = v.size();
      if (num_elt > ~(uint32_t) 0) {
        return false;
      }
      buf->AddTag<std::vector<T> >();
      buf->AddVal(static_cast<uint32_t>(num_elt));

      for (size_t ix = 0; ix < v.size(); ++ix) {
        buf->AddVal(v[ix]);
      }
      return true;
    }
  };

  template<typename T, bool b> friend class SerializeHelper;

  template<typename T, bool nested_tagging> class DeserializeHelper {
   public:
    static bool DoDeserialize(SerializationBuffer *buf,
                              std::vector<T> *v) {
      size_t orig = buf->cur_read_pos();
      if (buf->ReadTag() != SerializationTraits<std::vector<T> >::kTag) {
        buf->reset_read_pos(orig);
        return false;
      }
      uint32_t num_elt;
      if (!buf->GetVal(&num_elt)) {
        buf->reset_read_pos(orig);
        return false;
      }
      for (size_t ix = 0; ix < num_elt; ++ix) {
        T val;
        if (!buf->Deserialize(&val)) {
          buf->reset_read_pos(orig);
          return false;
        }
        v->push_back(val);
      }
      return true;
    }
  };

  template<typename T> class DeserializeHelper<T, false> {
   public:
    static bool DoDeserialize(SerializationBuffer *buf,
                              std::vector<T> *v) {
      size_t orig = buf->cur_read_pos();
      if (buf->ReadTag() != SerializationTraits<std::vector<T> >::kTag) {
        buf->reset_read_pos(orig);
        return false;
      }
      uint32_t num_elt;
      if (!buf->GetVal(&num_elt)) {
        buf->reset_read_pos(orig);
        return false;
      }
      for (size_t ix = 0; ix < num_elt; ++ix) {
        T val;
        if (!buf->GetVal(&val)) {
          buf->reset_read_pos(orig);
          return false;
        }
        v->push_back(val);
      }
      return true;
    }
  };

  template<typename T, bool b> friend class DeserializeHelper;

  // TODO(bsy): consider doing something along the lines of
  //
  // template<typename T> Serialize(T stl_container) {
  //   AddTag<T>();  // how?
  //   for (T::const_iterator it = stl_container.begin();
  //        it != stl_container.end();
  //        ++it) {
  //     Serialize(*it);
  //     // Or AddVal, when SerializationTraits<T::value_type>::kNestedTag
  //     // is false.
  //   }
  // }
  //
  // This means that the container type would probably be omitted or a
  // generic stl_container type tag would be used -- or we'd have to
  // enumerate all container types.

 private:
  std::vector<uint8_t> buffer_;
  size_t nbytes_;
  size_t in_use_;
  size_t read_ix_;

  void EnsureTotalSize(size_t req_size);
  void EnsureAvailableSpace(size_t req_space);

  size_t bytes_unread() const {
    return in_use_ - read_ix_;
  }

  size_t cur_read_pos() const {
    return read_ix_;
  }

  void reset_read_pos(size_t pos) {
    read_ix_ = pos;
  }

  size_t cur_write_pos() const {
    return in_use_;
  }

  void reset_write_pos(size_t pos) {
    in_use_ = pos;
  }
};

template<typename T> void SerializationBuffer::AddTag() {
  AddUint8(SerializationTraits<T>::kTag);
}

template<typename T> bool SerializationBuffer::Serialize(T basic) {
  AddTag<T>();
  AddVal(basic);
  return true;
}

template<typename T> bool SerializationBuffer::Serialize(
    std::vector<T> const& v) {
  return SerializeHelper<T, SerializationTraits<T>::kNestedTag>::
      DoSerialize(this, v);
}

template<typename T> bool SerializationBuffer::Deserialize(T *basic) {
  size_t orig = cur_read_pos();
  if (bytes_unread() < kTagBytes + SerializationTraits<T>::kBytes) {
    return false;
  }
  uint8_t tag;
  if ((tag = ReadTag()) != SerializationTraits<T>::kTag) {
    reset_read_pos(orig);
    return false;
  }
  // if BytesAvail >= tag + serialization_size
  (void) GetVal(basic);
  return true;
}

template<typename T> bool SerializationBuffer::Deserialize(
    std::vector<T> *v) {
  return DeserializeHelper<T, SerializationTraits<T>::kNestedTag>::
      DoDeserialize(this, v);
}

template<> class SerializationTraits<uint8_t> {
 public:
  static const int kTag = kUint8;
  static const int kBytes = 1;
  static const bool kNestedTag = false;
};

template<> class SerializationTraits<int8_t> {
 public:
  static const int kTag = kInt8;
  static const int kBytes = 1;
  static const bool kNestedTag = false;
};

template<> class SerializationTraits<uint16_t> {
 public:
  static const int kTag = kUint16;
  static const int kBytes = 2;
  static const bool kNestedTag = false;
};

template<> class SerializationTraits<int16_t> {
 public:
  static const int kTag = kInt16;
  static const int kBytes = 2;
  static const bool kNestedTag = false;
};

template<> class SerializationTraits<uint32_t> {
 public:
  static const int kTag = kUint32;
  static const int kBytes = 4;
  static const bool kNestedTag = false;
};

template<> class SerializationTraits<int32_t> {
 public:
  static const int kTag = kInt32;
  static const int kBytes = 4;
  static const bool kNestedTag = false;
};

template<> class SerializationTraits<uint64_t> {
 public:
  static const int kTag = kUint64;
  static const int kBytes = 8;
  static const bool kNestedTag = false;
};

template<> class SerializationTraits<int64_t> {
 public:
  static const int kTag = kInt64;
  static const int kBytes = 8;
  static const bool kNestedTag = false;
};

#if defined(NACL_HAS_IEEE_754)
template<> class SerializationTraits<float> {
 public:
  static const int kTag = kFloat;
  static const int kBytes = 4;
  static const bool kNestedTag = false;
};

template<> class SerializationTraits<double> {
 public:
  static const int kTag = kDouble;
  static const int kBytes = 8;
  static const bool kNestedTag = false;
};

template<> class SerializationTraits<long double> {
 public:
  static const int kTag = kLongDouble;
  static const int kBytes = 10;
  static const bool kNestedTag = false;
};
#endif

template<> class SerializationTraits<char *> {
 public:
  static const int kTag = kCString;
  static const bool kNestedTag = true;
};

template<> class SerializationTraits<std::string> {
 public:
  static const int kTag = kString;
  static const bool kNestedTag = true;
};

// We want the type tag for vector<T>, when the type T is a basic
// type, to incorporate the type tag for T.  This way, we do not tag
// each vector element (see SerializeHelper etc above), and yet the
// type information is present.  When T is not a basic type (e.g., it
// is a string, a vector<U>, or some other container to be added), we
// don't want to just add the kVectorOffset to the type tag for T,
// since deep nesting of containers could cause the tag to overflow.
// Assuming that the type T nested containers are not empty, paying
// the cost of tagging each element of the vector is not a huge
// overhead.
template<typename T> class SerializationTraits<std::vector<T> > {
 private:
  template<typename S, bool b> class RecursiveOrNotTag {
   public:
    static const int kVectorTag = kRecursiveVector;
  };
  template<typename S> class RecursiveOrNotTag<S, false> {
   public:
    static const int kVectorTag = kVectorOffset + SerializationTraits<S>::kTag;
  };
 public:
  static const int kTag =
      RecursiveOrNotTag<T, SerializationTraits<T>::kNestedTag>::kVectorTag;
  static const bool kNestedTag = true;
};

}  // namespace nacl

#endif
