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

#include "perfetto/protozero/proto_decoder.h"

#include <string.h>

#include "perfetto/base/logging.h"
#include "perfetto/protozero/proto_utils.h"

namespace protozero {

using namespace proto_utils;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BYTE_SWAP_TO_LE32(x) (x)
#define BYTE_SWAP_TO_LE64(x) (x)
#else
#error Unimplemented for big endian archs.
#endif

ProtoDecoder::Field ProtoDecoder::ReadField() {
  Field field{};

  // The first byte of a proto field is structured as follows:
  // The least 3 significant bits determine the field type.
  // The most 5 significant bits determine the field id. If MSB == 1, the
  // field id continues on the next bytes following the VarInt encoding.
  const uint8_t kFieldTypeNumBits = 3;
  const uint64_t kFieldTypeMask = (1 << kFieldTypeNumBits) - 1;  // 0000 0111;

  const uint8_t* end = buffer_ + length_;
  const uint8_t* pos = current_position_;
  PERFETTO_DCHECK(pos >= buffer_);
  PERFETTO_DCHECK(pos <= end);

  // If we've already hit the end, just return an invalid field.
  if (pos >= end) {
    return field;
  }

  uint64_t raw_field_id = 0;
  if (PERFETTO_LIKELY(*pos < 0x80)) {
    raw_field_id = *(pos++);  // Fastpath for fields with ID < 32.
  } else {
    pos = ParseVarInt(pos, end, &raw_field_id);
  }

  uint32_t field_id = static_cast<uint32_t>(raw_field_id >> kFieldTypeNumBits);
  if (field_id == 0 || pos >= end) {
    return field;
  }

  field.type = static_cast<ProtoWireType>(raw_field_id & kFieldTypeMask);

  const uint8_t* new_pos = nullptr;
  uint64_t field_intvalue = 0;
  switch (field.type) {
    case ProtoWireType::kVarInt: {
      new_pos = ParseVarInt(pos, end, &field.int_value);

      // new_pos not being greater than pos means ParseVarInt could not fully
      // parse the number. This is because we are out of space in the buffer.
      // Set the id to zero and return but don't update the offset so a future
      // read can read this field.
      if (new_pos == pos) {
        return field;
      }
      pos = new_pos;
      break;
    }
    case ProtoWireType::kLengthDelimited: {
      new_pos = ParseVarInt(pos, end, &field_intvalue);

      // new_pos not being greater than pos means ParseVarInt could not fully
      // parse the number. This is because we are out of space in the buffer.
      // Alternatively, we may not have space to fully read the length
      // delimited field. Set the id to zero and return but don't update the
      // offset so a future read can read this field.
      // It is safe to static_cast end - new_pos as new_pos will be <= end after
      // ParseVarInt.
      if (new_pos == pos ||
          field_intvalue > static_cast<uint64_t>(end - new_pos)) {
        return field;
      }

      // If the message is larger than 256 MiB silently skip it.
      if (field_intvalue >= proto_utils::kMaxMessageLength) {
        current_position_ = new_pos + field_intvalue;
        return ReadField();
      }

      pos = new_pos;
      field.length_limited.data = pos;
      field.length_limited.length = static_cast<size_t>(field_intvalue);
      pos += field_intvalue;
      break;
    }
    case ProtoWireType::kFixed64: {
      if (pos + sizeof(uint64_t) > end) {
        return field;
      }
      memcpy(&field_intvalue, pos, sizeof(uint64_t));
      field.int_value = BYTE_SWAP_TO_LE64(field_intvalue);
      pos += sizeof(uint64_t);
      break;
    }
    case ProtoWireType::kFixed32: {
      if (pos + sizeof(uint32_t) > end) {
        return field;
      }
      uint32_t tmp;
      memcpy(&tmp, pos, sizeof(uint32_t));
      field.int_value = BYTE_SWAP_TO_LE32(tmp);
      pos += sizeof(uint32_t);
      break;
    }
  }
  // Set the field id to make the returned value valid and update the current
  // position in the buffer.
  field.id = field_id;
  current_position_ = pos;
  return field;
}

}  // namespace protozero
