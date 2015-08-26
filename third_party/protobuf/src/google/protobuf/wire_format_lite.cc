// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/wire_format_lite_inl.h>

#include <stack>
#include <string>
#include <vector>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/coded_stream_inl.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

namespace google {
namespace protobuf {
namespace internal {

#ifndef _MSC_VER    // MSVC doesn't like definitions of inline constants, GCC
                    // requires them.
const int WireFormatLite::kMessageSetItemStartTag;
const int WireFormatLite::kMessageSetItemEndTag;
const int WireFormatLite::kMessageSetTypeIdTag;
const int WireFormatLite::kMessageSetMessageTag;

#endif

// ===================================================================

bool FieldSkipper::SkipField(
    io::CodedInputStream* input, uint32 tag) {
  return WireFormatLite::SkipField(input, tag, unknown_fields_);
}

bool FieldSkipper::SkipMessage(io::CodedInputStream* input) {
  return WireFormatLite::SkipMessage(input, unknown_fields_);
}

void FieldSkipper::SkipUnknownEnum(
    int field_number, int value) {
  unknown_fields_->AddVarint(field_number, value);
}

// ===================================================================

// IBM xlC requires prefixing constants with WireFormatLite::
const int WireFormatLite::kMessageSetItemTagsSize =
  io::CodedOutputStream::StaticVarintSize32<
      WireFormatLite::kMessageSetItemStartTag>::value +
  io::CodedOutputStream::StaticVarintSize32<
      WireFormatLite::kMessageSetItemEndTag>::value +
  io::CodedOutputStream::StaticVarintSize32<
      WireFormatLite::kMessageSetTypeIdTag>::value +
  io::CodedOutputStream::StaticVarintSize32<
      WireFormatLite::kMessageSetMessageTag>::value;

const WireFormatLite::CppType
WireFormatLite::kFieldTypeToCppTypeMap[MAX_FIELD_TYPE + 1] = {
  static_cast<CppType>(0),  // 0 is reserved for errors

  CPPTYPE_DOUBLE,   // TYPE_DOUBLE
  CPPTYPE_FLOAT,    // TYPE_FLOAT
  CPPTYPE_INT64,    // TYPE_INT64
  CPPTYPE_UINT64,   // TYPE_UINT64
  CPPTYPE_INT32,    // TYPE_INT32
  CPPTYPE_UINT64,   // TYPE_FIXED64
  CPPTYPE_UINT32,   // TYPE_FIXED32
  CPPTYPE_BOOL,     // TYPE_BOOL
  CPPTYPE_STRING,   // TYPE_STRING
  CPPTYPE_MESSAGE,  // TYPE_GROUP
  CPPTYPE_MESSAGE,  // TYPE_MESSAGE
  CPPTYPE_STRING,   // TYPE_BYTES
  CPPTYPE_UINT32,   // TYPE_UINT32
  CPPTYPE_ENUM,     // TYPE_ENUM
  CPPTYPE_INT32,    // TYPE_SFIXED32
  CPPTYPE_INT64,    // TYPE_SFIXED64
  CPPTYPE_INT32,    // TYPE_SINT32
  CPPTYPE_INT64,    // TYPE_SINT64
};

const WireFormatLite::WireType
WireFormatLite::kWireTypeForFieldType[MAX_FIELD_TYPE + 1] = {
  static_cast<WireFormatLite::WireType>(-1),  // invalid
  WireFormatLite::WIRETYPE_FIXED64,           // TYPE_DOUBLE
  WireFormatLite::WIRETYPE_FIXED32,           // TYPE_FLOAT
  WireFormatLite::WIRETYPE_VARINT,            // TYPE_INT64
  WireFormatLite::WIRETYPE_VARINT,            // TYPE_UINT64
  WireFormatLite::WIRETYPE_VARINT,            // TYPE_INT32
  WireFormatLite::WIRETYPE_FIXED64,           // TYPE_FIXED64
  WireFormatLite::WIRETYPE_FIXED32,           // TYPE_FIXED32
  WireFormatLite::WIRETYPE_VARINT,            // TYPE_BOOL
  WireFormatLite::WIRETYPE_LENGTH_DELIMITED,  // TYPE_STRING
  WireFormatLite::WIRETYPE_START_GROUP,       // TYPE_GROUP
  WireFormatLite::WIRETYPE_LENGTH_DELIMITED,  // TYPE_MESSAGE
  WireFormatLite::WIRETYPE_LENGTH_DELIMITED,  // TYPE_BYTES
  WireFormatLite::WIRETYPE_VARINT,            // TYPE_UINT32
  WireFormatLite::WIRETYPE_VARINT,            // TYPE_ENUM
  WireFormatLite::WIRETYPE_FIXED32,           // TYPE_SFIXED32
  WireFormatLite::WIRETYPE_FIXED64,           // TYPE_SFIXED64
  WireFormatLite::WIRETYPE_VARINT,            // TYPE_SINT32
  WireFormatLite::WIRETYPE_VARINT,            // TYPE_SINT64
};

bool WireFormatLite::SkipField(io::CodedInputStream* input, uint32 tag) {
  return SkipField(input, tag, static_cast<UnknownFieldSet*>(NULL));
}

bool WireFormatLite::SkipField(io::CodedInputStream* input, uint32 tag,
                               UnknownFieldSet* unknown_fields) {
  int number = WireFormatLite::GetTagFieldNumber(tag);

  switch (WireFormatLite::GetTagWireType(tag)) {
    case WireFormatLite::WIRETYPE_VARINT: {
      uint64 value;
      if (!input->ReadVarint64(&value)) return false;
      if (unknown_fields != NULL) unknown_fields->AddVarint(number, value);
      return true;
    }
    case WireFormatLite::WIRETYPE_FIXED64: {
      uint64 value;
      if (!input->ReadLittleEndian64(&value)) return false;
      if (unknown_fields != NULL) unknown_fields->AddFixed64(number, value);
      return true;
    }
    case WireFormatLite::WIRETYPE_LENGTH_DELIMITED: {
      uint32 length;
      if (!input->ReadVarint32(&length)) return false;
      if (unknown_fields == NULL) {
        if (!input->Skip(length)) return false;
      } else {
        if (!input->ReadString(unknown_fields->AddLengthDelimited(number),
                               length)) {
          return false;
        }
      }
      return true;
    }
    case WireFormatLite::WIRETYPE_START_GROUP: {
      if (!input->IncrementRecursionDepth()) return false;
      if (!SkipMessage(input, (unknown_fields == NULL) ?
                              NULL : unknown_fields->AddGroup(number))) {
        return false;
      }
      input->DecrementRecursionDepth();
      // Check that the ending tag matched the starting tag.
      if (!input->LastTagWas(WireFormatLite::MakeTag(
          WireFormatLite::GetTagFieldNumber(tag),
          WireFormatLite::WIRETYPE_END_GROUP))) {
        return false;
      }
      return true;
    }
    case WireFormatLite::WIRETYPE_END_GROUP: {
      return false;
    }
    case WireFormatLite::WIRETYPE_FIXED32: {
      uint32 value;
      if (!input->ReadLittleEndian32(&value)) return false;
      if (unknown_fields != NULL) unknown_fields->AddFixed32(number, value);
      return true;
    }
    default: {
      return false;
    }
  }
}

bool WireFormatLite::SkipField(
    io::CodedInputStream* input, uint32 tag, io::CodedOutputStream* output) {
  switch (WireFormatLite::GetTagWireType(tag)) {
    case WireFormatLite::WIRETYPE_VARINT: {
      uint64 value;
      if (!input->ReadVarint64(&value)) return false;
      output->WriteVarint32(tag);
      output->WriteVarint64(value);
      return true;
    }
    case WireFormatLite::WIRETYPE_FIXED64: {
      uint64 value;
      if (!input->ReadLittleEndian64(&value)) return false;
      output->WriteVarint32(tag);
      output->WriteLittleEndian64(value);
      return true;
    }
    case WireFormatLite::WIRETYPE_LENGTH_DELIMITED: {
      uint32 length;
      if (!input->ReadVarint32(&length)) return false;
      output->WriteVarint32(tag);
      output->WriteVarint32(length);
      // TODO(mkilavuz): Provide API to prevent extra string copying.
      string temp;
      if (!input->ReadString(&temp, length)) return false;
      output->WriteString(temp);
      return true;
    }
    case WireFormatLite::WIRETYPE_START_GROUP: {
      output->WriteVarint32(tag);
      if (!input->IncrementRecursionDepth()) return false;
      if (!SkipMessage(input, output)) return false;
      input->DecrementRecursionDepth();
      // Check that the ending tag matched the starting tag.
      if (!input->LastTagWas(WireFormatLite::MakeTag(
          WireFormatLite::GetTagFieldNumber(tag),
          WireFormatLite::WIRETYPE_END_GROUP))) {
        return false;
      }
      return true;
    }
    case WireFormatLite::WIRETYPE_END_GROUP: {
      return false;
    }
    case WireFormatLite::WIRETYPE_FIXED32: {
      uint32 value;
      if (!input->ReadLittleEndian32(&value)) return false;
      output->WriteVarint32(tag);
      output->WriteLittleEndian32(value);
      return true;
    }
    default: {
      return false;
    }
  }
}

bool WireFormatLite::SkipMessage(io::CodedInputStream* input,
                                 UnknownFieldSet* unknown_fields) {
  while (true) {
    uint32 tag = input->ReadTag();
    if (tag == 0) {
      // End of input.  This is a valid place to end, so return true.
      return true;
    }

    WireFormatLite::WireType wire_type = WireFormatLite::GetTagWireType(tag);

    if (wire_type == WireFormatLite::WIRETYPE_END_GROUP) {
      // Must be the end of the message.
      return true;
    }

    if (!SkipField(input, tag, unknown_fields)) return false;
  }
}

bool WireFormatLite::SkipMessage(io::CodedInputStream* input,
    io::CodedOutputStream* output) {
  while (true) {
    uint32 tag = input->ReadTag();
    if (tag == 0) {
      // End of input.  This is a valid place to end, so return true.
      return true;
    }

    WireFormatLite::WireType wire_type = WireFormatLite::GetTagWireType(tag);

    if (wire_type == WireFormatLite::WIRETYPE_END_GROUP) {
      output->WriteVarint32(tag);
      // Must be the end of the message.
      return true;
    }

    if (!SkipField(input, tag, output)) return false;
  }
}

bool WireFormatLite::ReadPackedEnumPreserveUnknowns(
    io::CodedInputStream* input,
    uint32 field_number,
    bool (*is_valid)(int),
    UnknownFieldSet* unknown_fields,
    RepeatedField<int>* values) {
  uint32 length;
  if (!input->ReadVarint32(&length)) return false;
  io::CodedInputStream::Limit limit = input->PushLimit(length);
  while (input->BytesUntilLimit() > 0) {
    int value;
    if (!google::protobuf::internal::WireFormatLite::ReadPrimitive<
        int, WireFormatLite::TYPE_ENUM>(input, &value)) {
      return false;
    }
    if (is_valid == NULL || is_valid(value)) {
      values->Add(value);
    } else {
      unknown_fields->AddVarint(field_number, value);
    }
  }
  input->PopLimit(limit);
  return true;
}


void WireFormatLite::SerializeUnknownFields(
    const UnknownFieldSet& unknown_fields,
    io::CodedOutputStream* output) {
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const UnknownField& field = unknown_fields.field(i);
    switch (field.type()) {
      case UnknownField::TYPE_VARINT:
        output->WriteVarint32(WireFormatLite::MakeTag(field.number(),
            WireFormatLite::WIRETYPE_VARINT));
        output->WriteVarint64(field.varint());
        break;
      case UnknownField::TYPE_FIXED32:
        output->WriteVarint32(WireFormatLite::MakeTag(field.number(),
            WireFormatLite::WIRETYPE_FIXED32));
        output->WriteLittleEndian32(field.fixed32());
        break;
      case UnknownField::TYPE_FIXED64:
        output->WriteVarint32(WireFormatLite::MakeTag(field.number(),
            WireFormatLite::WIRETYPE_FIXED64));
        output->WriteLittleEndian64(field.fixed64());
        break;
      case UnknownField::TYPE_LENGTH_DELIMITED:
        output->WriteVarint32(WireFormatLite::MakeTag(field.number(),
            WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
        output->WriteVarint32(field.length_delimited().size());
        output->WriteRawMaybeAliased(field.length_delimited().data(),
                                     field.length_delimited().size());
        break;
      case UnknownField::TYPE_GROUP:
        output->WriteVarint32(WireFormatLite::MakeTag(field.number(),
            WireFormatLite::WIRETYPE_START_GROUP));
        SerializeUnknownFields(field.group(), output);
        output->WriteVarint32(WireFormatLite::MakeTag(field.number(),
            WireFormatLite::WIRETYPE_END_GROUP));
        break;
    }
  }
}

uint8* WireFormatLite::SerializeUnknownFieldsToArray(
    const UnknownFieldSet& unknown_fields,
    uint8* target) {
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const UnknownField& field = unknown_fields.field(i);

    switch (field.type()) {
      case UnknownField::TYPE_VARINT:
        target = WireFormatLite::WriteInt64ToArray(
            field.number(), field.varint(), target);
        break;
      case UnknownField::TYPE_FIXED32:
        target = WireFormatLite::WriteFixed32ToArray(
            field.number(), field.fixed32(), target);
        break;
      case UnknownField::TYPE_FIXED64:
        target = WireFormatLite::WriteFixed64ToArray(
            field.number(), field.fixed64(), target);
        break;
      case UnknownField::TYPE_LENGTH_DELIMITED:
        target = WireFormatLite::WriteBytesToArray(
            field.number(), field.length_delimited(), target);
        break;
      case UnknownField::TYPE_GROUP:
        target = WireFormatLite::WriteTagToArray(
            field.number(), WireFormatLite::WIRETYPE_START_GROUP, target);
        target = SerializeUnknownFieldsToArray(field.group(), target);
        target = WireFormatLite::WriteTagToArray(
            field.number(), WireFormatLite::WIRETYPE_END_GROUP, target);
        break;
    }
  }
  return target;
}

void WireFormatLite::SerializeUnknownMessageSetItems(
    const UnknownFieldSet& unknown_fields,
    io::CodedOutputStream* output) {
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const UnknownField& field = unknown_fields.field(i);
    // The only unknown fields that are allowed to exist in a MessageSet are
    // messages, which are length-delimited.
    if (field.type() == UnknownField::TYPE_LENGTH_DELIMITED) {
      // Start group.
      output->WriteVarint32(WireFormatLite::kMessageSetItemStartTag);

      // Write type ID.
      output->WriteVarint32(WireFormatLite::kMessageSetTypeIdTag);
      output->WriteVarint32(field.number());

      // Write message.
      output->WriteVarint32(WireFormatLite::kMessageSetMessageTag);
      field.SerializeLengthDelimitedNoTag(output);

      // End group.
      output->WriteVarint32(WireFormatLite::kMessageSetItemEndTag);
    }
  }
}

uint8* WireFormatLite::SerializeUnknownMessageSetItemsToArray(
    const UnknownFieldSet& unknown_fields,
    uint8* target) {
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const UnknownField& field = unknown_fields.field(i);

    // The only unknown fields that are allowed to exist in a MessageSet are
    // messages, which are length-delimited.
    if (field.type() == UnknownField::TYPE_LENGTH_DELIMITED) {
      // Start group.
      target = io::CodedOutputStream::WriteTagToArray(
          WireFormatLite::kMessageSetItemStartTag, target);

      // Write type ID.
      target = io::CodedOutputStream::WriteTagToArray(
          WireFormatLite::kMessageSetTypeIdTag, target);
      target = io::CodedOutputStream::WriteVarint32ToArray(
          field.number(), target);

      // Write message.
      target = io::CodedOutputStream::WriteTagToArray(
          WireFormatLite::kMessageSetMessageTag, target);
      target = field.SerializeLengthDelimitedNoTagToArray(target);

      // End group.
      target = io::CodedOutputStream::WriteTagToArray(
          WireFormatLite::kMessageSetItemEndTag, target);
    }
  }

  return target;
}

int WireFormatLite::ComputeUnknownFieldsSize(
    const UnknownFieldSet& unknown_fields) {
  int size = 0;
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const UnknownField& field = unknown_fields.field(i);

    switch (field.type()) {
      case UnknownField::TYPE_VARINT:
        size += io::CodedOutputStream::VarintSize32(
            WireFormatLite::MakeTag(field.number(),
            WireFormatLite::WIRETYPE_VARINT));
        size += io::CodedOutputStream::VarintSize64(field.varint());
        break;
      case UnknownField::TYPE_FIXED32:
        size += io::CodedOutputStream::VarintSize32(
            WireFormatLite::MakeTag(field.number(),
            WireFormatLite::WIRETYPE_FIXED32));
        size += sizeof(int32);
        break;
      case UnknownField::TYPE_FIXED64:
        size += io::CodedOutputStream::VarintSize32(
            WireFormatLite::MakeTag(field.number(),
            WireFormatLite::WIRETYPE_FIXED64));
        size += sizeof(int64);
        break;
      case UnknownField::TYPE_LENGTH_DELIMITED:
        size += io::CodedOutputStream::VarintSize32(
            WireFormatLite::MakeTag(field.number(),
            WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
        size += io::CodedOutputStream::VarintSize32(
            field.length_delimited().size());
        size += field.length_delimited().size();
        break;
      case UnknownField::TYPE_GROUP:
        size += io::CodedOutputStream::VarintSize32(
            WireFormatLite::MakeTag(field.number(),
            WireFormatLite::WIRETYPE_START_GROUP));
        size += ComputeUnknownFieldsSize(field.group());
        size += io::CodedOutputStream::VarintSize32(
            WireFormatLite::MakeTag(field.number(),
            WireFormatLite::WIRETYPE_END_GROUP));
        break;
    }
  }

  return size;
}

int WireFormatLite::ComputeUnknownMessageSetItemsSize(
    const UnknownFieldSet& unknown_fields) {
  int size = 0;
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const UnknownField& field = unknown_fields.field(i);

    // The only unknown fields that are allowed to exist in a MessageSet are
    // messages, which are length-delimited.
    if (field.type() == UnknownField::TYPE_LENGTH_DELIMITED) {
      size += WireFormatLite::kMessageSetItemTagsSize;
      size += io::CodedOutputStream::VarintSize32(field.number());

      int field_size = field.GetLengthDelimitedSize();
      size += io::CodedOutputStream::VarintSize32(field_size);
      size += field_size;
    }
  }

  return size;
}

bool CodedOutputStreamFieldSkipper::SkipField(
    io::CodedInputStream* input, uint32 tag) {
  return WireFormatLite::SkipField(input, tag, unknown_fields_);
}

bool CodedOutputStreamFieldSkipper::SkipMessage(io::CodedInputStream* input) {
  return WireFormatLite::SkipMessage(input, unknown_fields_);
}

void CodedOutputStreamFieldSkipper::SkipUnknownEnum(
    int field_number, int value) {
  unknown_fields_->WriteVarint32(field_number);
  unknown_fields_->WriteVarint64(value);
}

bool WireFormatLite::ReadPackedEnumNoInline(io::CodedInputStream* input,
                                            bool (*is_valid)(int),
                                            RepeatedField<int>* values) {
  uint32 length;
  if (!input->ReadVarint32(&length)) return false;
  io::CodedInputStream::Limit limit = input->PushLimit(length);
  while (input->BytesUntilLimit() > 0) {
    int value;
    if (!google::protobuf::internal::WireFormatLite::ReadPrimitive<
        int, WireFormatLite::TYPE_ENUM>(input, &value)) {
      return false;
    }
    if (is_valid == NULL || is_valid(value)) {
      values->Add(value);
    }
  }
  input->PopLimit(limit);
  return true;
}

bool WireFormatLite::ReadPackedEnumPreserveUnknowns(
    io::CodedInputStream* input,
    int field_number,
    bool (*is_valid)(int),
    io::CodedOutputStream* unknown_fields_stream,
    RepeatedField<int>* values) {
  uint32 length;
  if (!input->ReadVarint32(&length)) return false;
  io::CodedInputStream::Limit limit = input->PushLimit(length);
  while (input->BytesUntilLimit() > 0) {
    int value;
    if (!google::protobuf::internal::WireFormatLite::ReadPrimitive<
        int, WireFormatLite::TYPE_ENUM>(input, &value)) {
      return false;
    }
    if (is_valid == NULL || is_valid(value)) {
      values->Add(value);
    } else {
      uint32 tag = WireFormatLite::MakeTag(field_number,
                                           WireFormatLite::WIRETYPE_VARINT);
      unknown_fields_stream->WriteVarint32(tag);
      unknown_fields_stream->WriteVarint32(value);
    }
  }
  input->PopLimit(limit);
  return true;
}

void WireFormatLite::WriteInt32(int field_number, int32 value,
                                io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteInt32NoTag(value, output);
}
void WireFormatLite::WriteInt64(int field_number, int64 value,
                                io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteInt64NoTag(value, output);
}
void WireFormatLite::WriteUInt32(int field_number, uint32 value,
                                 io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteUInt32NoTag(value, output);
}
void WireFormatLite::WriteUInt64(int field_number, uint64 value,
                                 io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteUInt64NoTag(value, output);
}
void WireFormatLite::WriteSInt32(int field_number, int32 value,
                                 io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteSInt32NoTag(value, output);
}
void WireFormatLite::WriteSInt64(int field_number, int64 value,
                                 io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteSInt64NoTag(value, output);
}
void WireFormatLite::WriteFixed32(int field_number, uint32 value,
                                  io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED32, output);
  WriteFixed32NoTag(value, output);
}
void WireFormatLite::WriteFixed64(int field_number, uint64 value,
                                  io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED64, output);
  WriteFixed64NoTag(value, output);
}
void WireFormatLite::WriteSFixed32(int field_number, int32 value,
                                   io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED32, output);
  WriteSFixed32NoTag(value, output);
}
void WireFormatLite::WriteSFixed64(int field_number, int64 value,
                                   io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED64, output);
  WriteSFixed64NoTag(value, output);
}
void WireFormatLite::WriteFloat(int field_number, float value,
                                io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED32, output);
  WriteFloatNoTag(value, output);
}
void WireFormatLite::WriteDouble(int field_number, double value,
                                 io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED64, output);
  WriteDoubleNoTag(value, output);
}
void WireFormatLite::WriteBool(int field_number, bool value,
                               io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteBoolNoTag(value, output);
}
void WireFormatLite::WriteEnum(int field_number, int value,
                               io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteEnumNoTag(value, output);
}

void WireFormatLite::WriteString(int field_number, const string& value,
                                 io::CodedOutputStream* output) {
  // String is for UTF-8 text only
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  GOOGLE_CHECK(value.size() <= kint32max);
  output->WriteVarint32(value.size());
  output->WriteString(value);
}
void WireFormatLite::WriteStringMaybeAliased(
    int field_number, const string& value,
    io::CodedOutputStream* output) {
  // String is for UTF-8 text only
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  GOOGLE_CHECK(value.size() <= kint32max);
  output->WriteVarint32(value.size());
  output->WriteRawMaybeAliased(value.data(), value.size());
}
void WireFormatLite::WriteBytes(int field_number, const string& value,
                                io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  GOOGLE_CHECK(value.size() <= kint32max);
  output->WriteVarint32(value.size());
  output->WriteString(value);
}
void WireFormatLite::WriteBytesMaybeAliased(
    int field_number, const string& value,
    io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  GOOGLE_CHECK(value.size() <= kint32max);
  output->WriteVarint32(value.size());
  output->WriteRawMaybeAliased(value.data(), value.size());
}


void WireFormatLite::WriteGroup(int field_number,
                                const MessageLite& value,
                                io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_START_GROUP, output);
  value.SerializeWithCachedSizes(output);
  WriteTag(field_number, WIRETYPE_END_GROUP, output);
}

void WireFormatLite::WriteMessage(int field_number,
                                  const MessageLite& value,
                                  io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  const int size = value.GetCachedSize();
  output->WriteVarint32(size);
  value.SerializeWithCachedSizes(output);
}

void WireFormatLite::WriteGroupMaybeToArray(int field_number,
                                            const MessageLite& value,
                                            io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_START_GROUP, output);
  const int size = value.GetCachedSize();
  uint8* target = output->GetDirectBufferForNBytesAndAdvance(size);
  if (target != NULL) {
    uint8* end = value.SerializeWithCachedSizesToArray(target);
    GOOGLE_DCHECK_EQ(end - target, size);
  } else {
    value.SerializeWithCachedSizes(output);
  }
  WriteTag(field_number, WIRETYPE_END_GROUP, output);
}

void WireFormatLite::WriteMessageMaybeToArray(int field_number,
                                              const MessageLite& value,
                                              io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  const int size = value.GetCachedSize();
  output->WriteVarint32(size);
  uint8* target = output->GetDirectBufferForNBytesAndAdvance(size);
  if (target != NULL) {
    uint8* end = value.SerializeWithCachedSizesToArray(target);
    GOOGLE_DCHECK_EQ(end - target, size);
  } else {
    value.SerializeWithCachedSizes(output);
  }
}

static inline bool ReadBytesToString(io::CodedInputStream* input,
                                     string* value) GOOGLE_ATTRIBUTE_ALWAYS_INLINE;
static inline bool ReadBytesToString(io::CodedInputStream* input,
                                     string* value) {
  uint32 length;
  return input->ReadVarint32(&length) &&
      input->InternalReadStringInline(value, length);
}

bool WireFormatLite::ReadBytes(io::CodedInputStream* input, string* value) {
  return ReadBytesToString(input, value);
}

bool WireFormatLite::ReadBytes(io::CodedInputStream* input, string** p) {
  if (*p == &::google::protobuf::internal::GetEmptyStringAlreadyInited()) {
    *p = new ::std::string();
  }
  return ReadBytesToString(input, *p);
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google
