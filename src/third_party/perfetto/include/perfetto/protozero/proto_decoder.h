/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_PROTOZERO_PROTO_DECODER_H_
#define INCLUDE_PERFETTO_PROTOZERO_PROTO_DECODER_H_

#include <stdint.h>
#include <memory>

#include "perfetto/base/logging.h"
#include "perfetto/base/string_view.h"
#include "perfetto/protozero/proto_utils.h"

namespace protozero {

// Reads and decodes protobuf messages from a fixed length buffer. This class
// does not allocate and does no more work than necessary so can be used in
// performance sensitive contexts.
class ProtoDecoder {
 public:
  using StringView = ::perfetto::base::StringView;

  // The field of a protobuf message. |id| == 0 if the tag is not valid (e.g.
  // because the full tag was unable to be read etc.).
  struct Field {
    struct LengthDelimited {
      const uint8_t* data;
      size_t length;
    };

    uint32_t id = 0;
    protozero::proto_utils::FieldType type;
    union {
      uint64_t int_value;
      LengthDelimited length_limited;
    };

    inline uint32_t as_uint32() const {
      PERFETTO_DCHECK(type == proto_utils::FieldType::kFieldTypeVarInt ||
                      type == proto_utils::FieldType::kFieldTypeFixed32);
      return static_cast<uint32_t>(int_value);
    }

    inline StringView as_string() const {
      PERFETTO_DCHECK(type ==
                      proto_utils::FieldType::kFieldTypeLengthDelimited);
      return StringView(reinterpret_cast<const char*>(length_limited.data),
                        length_limited.length);
    }

    inline const uint8_t* data() const {
      PERFETTO_DCHECK(type ==
                      proto_utils::FieldType::kFieldTypeLengthDelimited);
      return length_limited.data;
    }

    inline size_t size() const {
      PERFETTO_DCHECK(type ==
                      proto_utils::FieldType::kFieldTypeLengthDelimited);
      return static_cast<size_t>(length_limited.length);
    }
  };

  // Creates a ProtoDecoder using the given |buffer| with size |length| bytes.
  inline ProtoDecoder(const uint8_t* buffer, uint64_t length)
      : buffer_(buffer), length_(length), current_position_(buffer) {}

  // Reads the next field from the buffer. If the full field cannot be read,
  // the returned struct will have id 0 which is an invalid field id.
  Field ReadField();

  template <int field_id>
  inline bool FindIntField(uint64_t* field_value) {
    bool res = false;
    for (auto f = ReadField(); f.id != 0; f = ReadField()) {
      if (f.id == field_id) {
        *field_value = f.int_value;
        res = true;
        break;
      }
    }
    Reset();
    return res;
  }

  // Returns true if |length_| == |current_position_| - |buffer| and false
  // otherwise.
  inline bool IsEndOfBuffer() {
    PERFETTO_DCHECK(current_position_ >= buffer_);
    return length_ == static_cast<uint64_t>(current_position_ - buffer_);
  }

  // Resets the current position to the start of the buffer.
  inline void Reset() { current_position_ = buffer_; }

  // Resets to the given position (must be within the buffer).
  inline void Reset(const uint8_t* pos) {
    PERFETTO_DCHECK(pos >= buffer_ && pos < buffer_ + length_);
    current_position_ = pos;
  }

  // Return's offset inside the buffer.
  inline uint64_t offset() const {
    return static_cast<uint64_t>(current_position_ - buffer_);
  }

  inline const uint8_t* buffer() const { return buffer_; }
  inline uint64_t length() const { return length_; }

 private:
  const uint8_t* const buffer_;
  const uint64_t length_;  // The outer buffer can be larger than 4GB.
  const uint8_t* current_position_ = nullptr;
};

}  // namespace protozero

#endif  // INCLUDE_PERFETTO_PROTOZERO_PROTO_DECODER_H_
