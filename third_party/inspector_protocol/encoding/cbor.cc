// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cbor.h"

#include <cassert>
#include <limits>
#include "json_parser_handler.h"

namespace inspector_protocol {
namespace {
using cbor_internals::MajorType;

// Indicates the number of bits the "initial byte" needs to be shifted to the
// right after applying |kMajorTypeMask| to produce the major type in the
// lowermost bits.
static constexpr uint8_t kMajorTypeBitShift = 5u;
// Mask selecting the low-order 5 bits of the "initial byte", which is where
// the additional information is encoded.
static constexpr uint8_t kAdditionalInformationMask = 0x1f;
// Mask selecting the high-order 3 bits of the "initial byte", which indicates
// the major type of the encoded value.
static constexpr uint8_t kMajorTypeMask = 0xe0;
// Indicates the integer is in the following byte.
static constexpr uint8_t kAdditionalInformation1Byte = 24u;
// Indicates the integer is in the next 2 bytes.
static constexpr uint8_t kAdditionalInformation2Bytes = 25u;
// Indicates the integer is in the next 4 bytes.
static constexpr uint8_t kAdditionalInformation4Bytes = 26u;
// Indicates the integer is in the next 8 bytes.
static constexpr uint8_t kAdditionalInformation8Bytes = 27u;

// Encodes the initial byte, consisting of the |type| in the first 3 bits
// followed by 5 bits of |additional_info|.
constexpr uint8_t EncodeInitialByte(MajorType type, uint8_t additional_info) {
  return (uint8_t(type) << kMajorTypeBitShift) |
         (additional_info & kAdditionalInformationMask);
}

// See RFC 7049 Section 2.3, Table 2.
static constexpr uint8_t kEncodedTrue =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 21);
static constexpr uint8_t kEncodedFalse =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 20);
static constexpr uint8_t kEncodedNull =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 22);
static constexpr uint8_t kInitialByteForDouble =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 27);

// See RFC 7049 Section 2.2.1, indefinite length arrays / maps have additional
// info = 31.
static constexpr uint8_t kInitialByteIndefiniteLengthArray =
    EncodeInitialByte(MajorType::ARRAY, 31);
static constexpr uint8_t kInitialByteIndefiniteLengthMap =
    EncodeInitialByte(MajorType::MAP, 31);
// See RFC 7049 Section 2.3, Table 1; this is used for finishing indefinite
// length maps / arrays.
static constexpr uint8_t kStopByte =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 31);

// See RFC 7049 Table 3 and Section 2.4.4.2. This is used as a prefix for
// arbitrary binary data encoded as BYTE_STRING.
static constexpr uint8_t kExpectedConversionToBase64Tag =
    EncodeInitialByte(MajorType::TAG, 22);

// When parsing CBOR, we limit recursion depth for objects and arrays
// to this constant.
static constexpr int kStackLimit = 1000;

// Writes the bytes for |v| to |out|, starting with the most significant byte.
// See also: https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html
template <typename T>
void WriteBytesMostSignificantByteFirst(T v, std::vector<uint8_t>* out) {
  for (int shift_bytes = sizeof(T) - 1; shift_bytes >= 0; --shift_bytes)
    out->push_back(0xff & v >> (shift_bytes * 8));
}
}  // namespace

namespace cbor_internals {
// Writes the start of a token with |type|. The |value| may indicate the size,
// or it may be the payload if the value is an unsigned integer.
void WriteTokenStart(MajorType type, uint64_t value,
                     std::vector<uint8_t>* encoded) {
  if (value < 24) {
    // Values 0-23 are encoded directly into the additional info of the
    // initial byte.
    encoded->push_back(EncodeInitialByte(type, /*additiona_info=*/value));
    return;
  }
  if (value <= std::numeric_limits<uint8_t>::max()) {
    // Values 24-255 are encoded with one initial byte, followed by the value.
    encoded->push_back(EncodeInitialByte(type, kAdditionalInformation1Byte));
    encoded->push_back(value);
    return;
  }
  if (value <= std::numeric_limits<uint16_t>::max()) {
    // Values 256-65535: 1 initial byte + 2 bytes payload.
    encoded->push_back(EncodeInitialByte(type, kAdditionalInformation2Bytes));
    WriteBytesMostSignificantByteFirst<uint16_t>(value, encoded);
    return;
  }
  if (value <= std::numeric_limits<uint32_t>::max()) {
    // 32 bit uint: 1 initial byte + 4 bytes payload.
    encoded->push_back(EncodeInitialByte(type, kAdditionalInformation4Bytes));
    WriteBytesMostSignificantByteFirst<uint32_t>(value, encoded);
    return;
  }
  // 64 bit uint: 1 initial byte + 8 bytes payload.
  encoded->push_back(EncodeInitialByte(type, kAdditionalInformation8Bytes));
  WriteBytesMostSignificantByteFirst<uint64_t>(value, encoded);
}
}  // namespace cbor_internals

namespace {
// Extracts sizeof(T) bytes from |in| to extract a value of type T
// (e.g. uint64_t, uint32_t, ...), most significant byte first.
// See also: https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html
template <typename T>
T ReadBytesMostSignificantByteFirst(span<uint8_t> in) {
  assert(size_t(in.size()) >= sizeof(T));
  T result = 0;
  for (size_t shift_bytes = 0; shift_bytes < sizeof(T); ++shift_bytes)
    result |= T(in[sizeof(T) - 1 - shift_bytes]) << (shift_bytes * 8);
  return result;
}
}  // namespace

namespace cbor_internals {
int8_t ReadTokenStart(span<uint8_t> bytes, MajorType* type, uint64_t* value) {
  if (bytes.empty()) return -1;
  uint8_t initial_byte = bytes[0];
  *type = MajorType((initial_byte & kMajorTypeMask) >> kMajorTypeBitShift);

  uint8_t additional_information = initial_byte & kAdditionalInformationMask;
  if (additional_information < 24) {
    // Values 0-23 are encoded directly into the additional info of the
    // initial byte.
    *value = additional_information;
    return 1;
  }
  if (additional_information == kAdditionalInformation1Byte) {
    // Values 24-255 are encoded with one initial byte, followed by the value.
    if (bytes.size() < 2) return -1;
    *value = ReadBytesMostSignificantByteFirst<uint8_t>(bytes.subspan(1));
    return 2;
  }
  if (additional_information == kAdditionalInformation2Bytes) {
    // Values 256-65535: 1 initial byte + 2 bytes payload.
    if (static_cast<size_t>(bytes.size()) < 1 + sizeof(uint16_t)) return -1;
    *value = ReadBytesMostSignificantByteFirst<uint16_t>(bytes.subspan(1));
    return 3;
  }
  if (additional_information == kAdditionalInformation4Bytes) {
    // 32 bit uint: 1 initial byte + 4 bytes payload.
    if (static_cast<size_t>(bytes.size()) < 1 + sizeof(uint32_t)) return -1;
    *value = ReadBytesMostSignificantByteFirst<uint32_t>(bytes.subspan(1));
    return 5;
  }
  if (additional_information == kAdditionalInformation8Bytes) {
    // 64 bit uint: 1 initial byte + 8 bytes payload.
    if (static_cast<size_t>(bytes.size()) < 1 + sizeof(uint64_t)) return -1;
    *value = ReadBytesMostSignificantByteFirst<uint64_t>(bytes.subspan(1));
    return 9;
  }
  return -1;
}
}  // namespace cbor_internals

using cbor_internals::MajorType;
using cbor_internals::WriteTokenStart;
using cbor_internals::ReadTokenStart;

void EncodeInt32(int32_t value, std::vector<uint8_t>* out) {
  if (value >= 0) {
    WriteTokenStart(MajorType::UNSIGNED, value, out);
  } else {
    uint64_t representation = static_cast<uint64_t>(-(value + 1));
    WriteTokenStart(MajorType::NEGATIVE, representation, out);
  }
}

void EncodeString16(span<uint16_t> in, std::vector<uint8_t>* out) {
  uint64_t byte_length = static_cast<uint64_t>(in.size_bytes());
  WriteTokenStart(MajorType::BYTE_STRING, byte_length, out);
  // When emitting UTF16 characters, we always write the least significant byte
  // first; this is because it's the native representation for X86.
  // TODO(johannes): Implement a more efficient thing here later, e.g.
  // casting *iff* the machine has this byte order.
  // The wire format for UTF16 chars will probably remain the same
  // (least significant byte first) since this way we can have
  // golden files, unittests, etc. that port easily and universally.
  // See also:
  // https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html
  for (const uint16_t two_bytes : in) {
    out->push_back(two_bytes);
    out->push_back(two_bytes >> 8);
  }
}

void EncodeString8(span<uint8_t> in, std::vector<uint8_t>* out) {
  WriteTokenStart(MajorType::STRING, static_cast<uint64_t>(in.size_bytes()),
                  out);
  out->insert(out->end(), in.begin(), in.end());
}

void EncodeBinary(span<uint8_t> in, std::vector<uint8_t>* out) {
  out->push_back(kExpectedConversionToBase64Tag);
  uint64_t byte_length = static_cast<uint64_t>(in.size_bytes());
  WriteTokenStart(MajorType::BYTE_STRING, byte_length, out);
  out->insert(out->end(), in.begin(), in.end());
}

// A double is encoded with a specific initial byte
// (kInitialByteForDouble) plus the 64 bits of payload for its value.
constexpr int kEncodedDoubleSize = 1 + sizeof(uint64_t);

void EncodeDouble(double value, std::vector<uint8_t>* out) {
  // The additional_info=27 indicates 64 bits for the double follow.
  // See RFC 7049 Section 2.3, Table 1.
  out->push_back(kInitialByteForDouble);
  union {
    double from_double;
    uint64_t to_uint64;
  } reinterpret;
  reinterpret.from_double = value;
  WriteBytesMostSignificantByteFirst<uint64_t>(reinterpret.to_uint64, out);
}

namespace {
class JsonToCBOREncoder : public JsonParserHandler {
 public:
  JsonToCBOREncoder(std::vector<uint8_t>* out, Status* status)
      : out_(out), status_(status) {
    *status_ = Status();
  }

  void HandleObjectBegin() override {
    out_->push_back(kInitialByteIndefiniteLengthMap);
  }

  void HandleObjectEnd() override { out_->push_back(kStopByte); };

  void HandleArrayBegin() override {
    out_->push_back(kInitialByteIndefiniteLengthArray);
  }

  void HandleArrayEnd() override { out_->push_back(kStopByte); };

  void HandleString16(std::vector<uint16_t> chars) override {
    for (uint16_t ch : chars) {
      if (ch >= 0x7f) {
        // If there's at least one non-7bit character, we encode as UTF16.
        EncodeString16(span<uint16_t>(chars.data(), chars.size()), out_);
        return;
      }
    }
    std::vector<uint8_t> sevenbit_chars(chars.begin(), chars.end());
    EncodeString8(span<uint8_t>(sevenbit_chars.data(), sevenbit_chars.size()),
                  out_);
  }

  void HandleBinary(std::vector<uint8_t> bytes) override {
    EncodeBinary(span<uint8_t>(bytes.data(), bytes.size()), out_);
  }

  void HandleDouble(double value) override { EncodeDouble(value, out_); };

  void HandleInt32(int32_t value) override { EncodeInt32(value, out_); }

  void HandleBool(bool value) override {
    // See RFC 7049 Section 2.3, Table 2.
    out_->push_back(value ? kEncodedTrue : kEncodedFalse);
  }

  void HandleNull() override {
    // See RFC 7049 Section 2.3, Table 2.
    out_->push_back(kEncodedNull);
  }

  void HandleError(Status error) override {
    assert(!error.ok());
    *status_ = error;
    out_->clear();
  }

 private:
  std::vector<uint8_t>* out_;
  Status* status_;
};
}  // namespace

std::unique_ptr<JsonParserHandler> NewJsonToCBOREncoder(
    std::vector<uint8_t>* out, Status* status) {
  return std::make_unique<JsonToCBOREncoder>(out, status);
}

namespace {
// Below are three parsing routines for CBOR, which cover enough
// to roundtrip JSON messages.
bool ParseMap(int32_t stack_depth, CBORTokenizer* tokenizer,
              JsonParserHandler* out);
bool ParseArray(int32_t stack_depth, CBORTokenizer* tokenizer,
                JsonParserHandler* out);
bool ParseValue(int32_t stack_depth, CBORTokenizer* tokenizer,
                JsonParserHandler* out);

void ParseUTF16String(CBORTokenizer* tokenizer, JsonParserHandler* out) {
  std::vector<uint16_t> value;
  span<uint8_t> rep = tokenizer->GetString16WireRep();
  for (std::ptrdiff_t ii = 0; ii < rep.size(); ii += 2)
    value.push_back((rep[ii + 1] << 8) | rep[ii]);
  out->HandleString16(std::move(value));
  tokenizer->Next();
}

// For now this method only covers US-ASCII. Later, we may allow UTF8.
bool ParseASCIIString(CBORTokenizer* tokenizer, JsonParserHandler* out) {
  assert(tokenizer->TokenTag() == CBORTokenTag::STRING8);
  std::vector<uint16_t> value16;
  for (uint8_t ch : tokenizer->GetString8()) {
    // We only accept us-ascii (7 bit) strings here. Other strings must
    // be encoded with 16 bit (the BYTE_STRING case).
    if (ch >= 0x7f) {
      out->HandleError(
          Status{Error::CBOR_STRING8_MUST_BE_7BIT, tokenizer->Status().pos});
      return false;
    }
    value16.push_back(ch);
  }
  out->HandleString16(std::move(value16));
  tokenizer->Next();
  return true;
}

bool ParseValue(int32_t stack_depth, CBORTokenizer* tokenizer,
                JsonParserHandler* out) {
  if (stack_depth > kStackLimit) {
    out->HandleError(
        Status{Error::CBOR_STACK_LIMIT_EXCEEDED, tokenizer->Status().pos});
    return false;
  }
  switch (tokenizer->TokenTag()) {
    case CBORTokenTag::ERROR:
      out->HandleError(tokenizer->Status());
      return false;
    case CBORTokenTag::DONE:
      out->HandleError(Status{Error::CBOR_UNEXPECTED_EOF_EXPECTED_VALUE,
                              tokenizer->Status().pos});
      return false;
    case CBORTokenTag::TRUE_VALUE:
      out->HandleBool(true);
      tokenizer->Next();
      return true;
    case CBORTokenTag::FALSE_VALUE:
      out->HandleBool(false);
      tokenizer->Next();
      return true;
    case CBORTokenTag::NULL_VALUE:
      out->HandleNull();
      tokenizer->Next();
      return true;
    case CBORTokenTag::INT32:
      out->HandleInt32(tokenizer->GetInt32());
      tokenizer->Next();
      return true;
    case CBORTokenTag::DOUBLE:
      out->HandleDouble(tokenizer->GetDouble());
      tokenizer->Next();
      return true;
    case CBORTokenTag::STRING8:
      return ParseASCIIString(tokenizer, out);
    case CBORTokenTag::STRING16:
      ParseUTF16String(tokenizer, out);
      return true;
    case CBORTokenTag::BINARY: {
      span<uint8_t> binary = tokenizer->GetBinary();
      out->HandleBinary(std::vector<uint8_t>(binary.begin(), binary.end()));
      tokenizer->Next();
      return true;
    }
    case CBORTokenTag::MAP_START:
      return ParseMap(stack_depth + 1, tokenizer, out);
    case CBORTokenTag::ARRAY_START:
      return ParseArray(stack_depth + 1, tokenizer, out);
    default:
      out->HandleError(
          Status{Error::CBOR_UNSUPPORTED_VALUE, tokenizer->Status().pos});
      return false;
  }
}

// |bytes| must start with the indefinite length array byte, so basically,
// ParseArray may only be called after an indefinite length array has been
// detected.
bool ParseArray(int32_t stack_depth, CBORTokenizer* tokenizer,
                JsonParserHandler* out) {
  assert(tokenizer->TokenTag() == CBORTokenTag::ARRAY_START);
  tokenizer->Next();
  out->HandleArrayBegin();
  while (tokenizer->TokenTag() != CBORTokenTag::STOP) {
    if (tokenizer->TokenTag() == CBORTokenTag::DONE) {
      out->HandleError(
          Status{Error::CBOR_UNEXPECTED_EOF_IN_ARRAY, tokenizer->Status().pos});
      return false;
    }
    if (tokenizer->TokenTag() == CBORTokenTag::ERROR) {
      out->HandleError(tokenizer->Status());
      return false;
    }
    // Parse value.
    if (!ParseValue(stack_depth, tokenizer, out)) return false;
  }
  out->HandleArrayEnd();
  tokenizer->Next();
  return true;
}

// |bytes| must start with the indefinite length array byte, so basically,
// ParseArray may only be called after an indefinite length array has been
// detected.
bool ParseMap(int32_t stack_depth, CBORTokenizer* tokenizer,
              JsonParserHandler* out) {
  assert(tokenizer->TokenTag() == CBORTokenTag::MAP_START);
  out->HandleObjectBegin();
  tokenizer->Next();
  while (tokenizer->TokenTag() != CBORTokenTag::STOP) {
    if (tokenizer->TokenTag() == CBORTokenTag::DONE) {
      out->HandleError(
          Status{Error::CBOR_UNEXPECTED_EOF_IN_MAP, tokenizer->Status().pos});
      return false;
    }
    if (tokenizer->TokenTag() == CBORTokenTag::ERROR) {
      out->HandleError(tokenizer->Status());
      return false;
    }
    // Parse key.
    if (tokenizer->TokenTag() == CBORTokenTag::STRING8) {
      if (!ParseASCIIString(tokenizer, out)) return false;
    } else if (tokenizer->TokenTag() == CBORTokenTag::STRING16) {
      ParseUTF16String(tokenizer, out);
    } else {
      out->HandleError(
          Status{Error::CBOR_INVALID_MAP_KEY, tokenizer->Status().pos});
      return false;
    }
    // Parse value.
    if (!ParseValue(stack_depth, tokenizer, out)) return false;
  }
  out->HandleObjectEnd();
  tokenizer->Next();
  return true;
}
}  // namespace

void ParseCBOR(span<uint8_t> bytes, JsonParserHandler* json_out) {
  CBORTokenizer tokenizer(bytes);
  if (tokenizer.TokenTag() == CBORTokenTag::DONE) {
    json_out->HandleError(Status{Error::CBOR_NO_INPUT, 0});
    return;
  }
  if (tokenizer.TokenTag() != CBORTokenTag::MAP_START) {
    json_out->HandleError(Status{Error::CBOR_INVALID_START_BYTE, 0});
    return;
  }
  if (!ParseMap(/*stack_depth=*/1, &tokenizer, json_out)) return;
  if (tokenizer.TokenTag() == CBORTokenTag::DONE) return;
  if (tokenizer.TokenTag() == CBORTokenTag::ERROR) {
    json_out->HandleError(tokenizer.Status());
    return;
  }
  json_out->HandleError(
      Status{Error::CBOR_TRAILING_JUNK, tokenizer.Status().pos});
}

CBORTokenizer::CBORTokenizer(span<uint8_t> bytes) : bytes_(bytes) {
  ReadNextToken();
}
CBORTokenizer::~CBORTokenizer() {}

CBORTokenTag CBORTokenizer::TokenTag() const { return token_tag_; }

void CBORTokenizer::Next() {
  if (token_tag_ == CBORTokenTag::ERROR || token_tag_ == CBORTokenTag::DONE)
    return;
  ReadNextToken();
}

Status CBORTokenizer::Status() const { return status_; }

int32_t CBORTokenizer::GetInt32() const {
  assert(token_tag_ == CBORTokenTag::INT32);
  // The range checks happen in ::ReadNextToken().
  return token_start_type_ == MajorType::UNSIGNED
             ? token_start_internal_value_
             : -static_cast<int64_t>(token_start_internal_value_) - 1;
}

double CBORTokenizer::GetDouble() const {
  assert(token_tag_ == CBORTokenTag::DOUBLE);
  union {
    uint64_t from_uint64;
    double to_double;
  } reinterpret;
  reinterpret.from_uint64 = ReadBytesMostSignificantByteFirst<uint64_t>(
      bytes_.subspan(status_.pos + 1));
  return reinterpret.to_double;
}

span<uint8_t> CBORTokenizer::GetString8() const {
  assert(token_tag_ == CBORTokenTag::STRING8);
  return bytes_.subspan(
      status_.pos + (token_byte_length_ - token_start_internal_value_),
      token_start_internal_value_);
}

span<uint8_t> CBORTokenizer::GetString16WireRep() const {
  assert(token_tag_ == CBORTokenTag::STRING16);
  return bytes_.subspan(
      status_.pos + (token_byte_length_ - token_start_internal_value_),
      token_start_internal_value_);
}

span<uint8_t> CBORTokenizer::GetBinary() const {
  assert(token_tag_ == CBORTokenTag::BINARY);
  return bytes_.subspan(
      status_.pos + (token_byte_length_ - token_start_internal_value_),
      token_start_internal_value_);
}

void CBORTokenizer::ReadNextToken() {
  status_.pos =
      status_.pos == Status::npos() ? 0 : status_.pos + token_byte_length_;
  status_.error = Error::OK;
  if (status_.pos >= bytes_.size()) {
    token_tag_ = CBORTokenTag::DONE;
    return;
  }
  switch (bytes_[status_.pos]) {
    case kStopByte:
      SetToken(CBORTokenTag::STOP, 1);
      return;
    case kInitialByteIndefiniteLengthMap:
      SetToken(CBORTokenTag::MAP_START, 1);
      return;
    case kInitialByteIndefiniteLengthArray:
      SetToken(CBORTokenTag::ARRAY_START, 1);
      return;
    case kEncodedTrue:
      SetToken(CBORTokenTag::TRUE_VALUE, 1);
      return;
    case kEncodedFalse:
      SetToken(CBORTokenTag::FALSE_VALUE, 1);
      return;
    case kEncodedNull:
      SetToken(CBORTokenTag::NULL_VALUE, 1);
      return;
    case kExpectedConversionToBase64Tag: {  // BINARY
      int64_t bytes_read =
          ReadTokenStart(bytes_.subspan(status_.pos + 1), &token_start_type_,
                         &token_start_internal_value_);
      int64_t token_byte_length = 1 + bytes_read + token_start_internal_value_;
      if (-1 == bytes_read || token_start_type_ != MajorType::BYTE_STRING ||
          status_.pos + token_byte_length > bytes_.size()) {
        SetError(Error::CBOR_INVALID_BINARY);
        return;
      }
      SetToken(CBORTokenTag::BINARY, token_byte_length);
      return;
    }
    case kInitialByteForDouble: {  // DOUBLE
      if (status_.pos + kEncodedDoubleSize > bytes_.size()) {
        SetError(Error::CBOR_INVALID_DOUBLE);
        return;
      }
      SetToken(CBORTokenTag::DOUBLE, kEncodedDoubleSize);
      return;
    }
    default: {
      span<uint8_t> remainder =
          bytes_.subspan(status_.pos, bytes_.size() - status_.pos);
      assert(!remainder.empty());
      int64_t token_start_length = ReadTokenStart(remainder, &token_start_type_,
                                                  &token_start_internal_value_);
      bool success = token_start_length != -1;
      switch (token_start_type_) {
        case MajorType::UNSIGNED:  // INT32.
          if (!success || std::numeric_limits<int32_t>::max() <
                              token_start_internal_value_) {
            SetError(Error::CBOR_INVALID_INT32);
            return;
          }
          SetToken(CBORTokenTag::INT32, token_start_length);
          return;
        case MajorType::NEGATIVE:  // INT32.
          if (!success ||
              std::numeric_limits<int32_t>::min() >
                  -static_cast<int64_t>(token_start_internal_value_) - 1) {
            SetError(Error::CBOR_INVALID_INT32);
            return;
          }
          SetToken(CBORTokenTag::INT32, token_start_length);
          return;
        case MajorType::STRING:  // STRING8.
          if (!success ||
              remainder.size() < int64_t(token_start_internal_value_)) {
            SetError(Error::CBOR_INVALID_STRING8);
            return;
          }
          SetToken(CBORTokenTag::STRING8,
                   token_start_length + token_start_internal_value_);
          return;
        case MajorType::BYTE_STRING:  // STRING16.
          if (!success ||
              remainder.size() < int64_t(token_start_internal_value_) ||
              // Must be divisible by 2 since UTF16 is 2 bytes per character.
              token_start_internal_value_ & 1) {
            SetError(Error::CBOR_INVALID_STRING16);
            return;
          }
          SetToken(CBORTokenTag::STRING16,
                   token_start_length + token_start_internal_value_);
          return;
        case MajorType::ARRAY:
        case MajorType::MAP:
        case MajorType::TAG:
        case MajorType::SIMPLE_VALUE:
          SetError(Error::CBOR_UNSUPPORTED_VALUE);
          return;
      }
    }
  }
}

void CBORTokenizer::SetToken(CBORTokenTag token_tag,
                             int64_t token_byte_length) {
  token_tag_ = token_tag;
  token_byte_length_ = token_byte_length;
}

void CBORTokenizer::SetError(Error error) {
  token_tag_ = CBORTokenTag::ERROR;
  status_.error = error;
}

}  // namespace inspector_protocol
