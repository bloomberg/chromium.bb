// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cbor.h"

#include <cassert>
#include <limits>
#include "json_parser_handler.h"

namespace inspector_protocol {
using namespace cbor;

namespace {

// See RFC 7049 Section 2.3, Table 2.
static constexpr uint8_t kEncodedTrue =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 21);
static constexpr uint8_t kEncodedFalse =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 20);
static constexpr uint8_t kEncodedNull =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 22);
static constexpr uint8_t kInitialByteForDouble =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 27);

}  // namespace

uint8_t EncodeTrue() { return kEncodedTrue; }
uint8_t EncodeFalse() { return kEncodedFalse; }
uint8_t EncodeNull() { return kEncodedNull; }

uint8_t EncodeIndefiniteLengthArrayStart() {
  return kInitialByteIndefiniteLengthArray;
}

uint8_t EncodeIndefiniteLengthMapStart() {
  return kInitialByteIndefiniteLengthMap;
}

uint8_t EncodeStop() { return kStopByte; }

namespace {
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
    out->push_back(0xff & (v >> (shift_bytes * 8)));
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
    encoded->push_back(EncodeInitialByte(type, /*additional_info=*/value));
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
    WriteBytesMostSignificantByteFirst<uint32_t>(static_cast<uint32_t>(value),
                                                 encoded);
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
  assert(static_cast<std::size_t>(in.size()) >= sizeof(T));
  T result = 0;
  for (std::size_t shift_bytes = 0; shift_bytes < sizeof(T); ++shift_bytes)
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
    if (static_cast<std::size_t>(bytes.size()) < 1 + sizeof(uint16_t))
      return -1;
    *value = ReadBytesMostSignificantByteFirst<uint16_t>(bytes.subspan(1));
    return 3;
  }
  if (additional_information == kAdditionalInformation4Bytes) {
    // 32 bit uint: 1 initial byte + 4 bytes payload.
    if (static_cast<std::size_t>(bytes.size()) < 1 + sizeof(uint32_t))
      return -1;
    *value = ReadBytesMostSignificantByteFirst<uint32_t>(bytes.subspan(1));
    return 5;
  }
  if (additional_information == kAdditionalInformation8Bytes) {
    // 64 bit uint: 1 initial byte + 8 bytes payload.
    if (static_cast<std::size_t>(bytes.size()) < 1 + sizeof(uint64_t))
      return -1;
    *value = ReadBytesMostSignificantByteFirst<uint64_t>(bytes.subspan(1));
    return 9;
  }
  return -1;
}
}  // namespace cbor_internals

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

void EncodeFromLatin1(span<uint8_t> latin1, std::vector<uint8_t>* out) {
  for (std::ptrdiff_t ii = 0; ii < latin1.size(); ++ii) {
    if (latin1[ii] <= 127)
      continue;
    // If there's at least one non-ASCII char, convert to UTF8.
    std::vector<uint8_t> utf8(latin1.begin(), latin1.begin() + ii);
    for (; ii < latin1.size(); ++ii) {
      if (latin1[ii] <= 127) {
        utf8.push_back(latin1[ii]);
      } else {
        // 0xC0 means it's a UTF8 sequence with 2 bytes.
        utf8.push_back((latin1[ii] >> 6) | 0xc0);
        utf8.push_back((latin1[ii] | 0x80) & 0xbf);
      }
    }
    EncodeString8(span<uint8_t>(utf8.data(), utf8.size()), out);
    return;
  }
  EncodeString8(latin1, out);
}

void EncodeFromUTF16(span<uint16_t> utf16, std::vector<uint8_t>* out) {
  // If there's at least one non-ASCII char, encode as STRING16 (UTF16).
  for (uint16_t ch : utf16) {
    if (ch <= 127)
      continue;
    EncodeString16(utf16, out);
    return;
  }
  // It's all US-ASCII, strip out every second byte and encode as UTF8.
  WriteTokenStart(MajorType::STRING, static_cast<uint64_t>(utf16.size()), out);
  out->insert(out->end(), utf16.begin(), utf16.end());
}

void EncodeBinary(span<uint8_t> in, std::vector<uint8_t>* out) {
  out->push_back(kExpectedConversionToBase64Tag);
  uint64_t byte_length = static_cast<uint64_t>(in.size_bytes());
  WriteTokenStart(MajorType::BYTE_STRING, byte_length, out);
  out->insert(out->end(), in.begin(), in.end());
}

// A double is encoded with a specific initial byte
// (kInitialByteForDouble) plus the 64 bits of payload for its value.
constexpr std::ptrdiff_t kEncodedDoubleSize = 1 + sizeof(uint64_t);

// An envelope is encoded with a specific initial byte
// (kInitialByteForEnvelope), plus the start byte for a BYTE_STRING with a 32
// bit wide length, plus a 32 bit length for that string.
constexpr std::ptrdiff_t kEncodedEnvelopeHeaderSize = 1 + 1 + sizeof(uint32_t);

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

void EnvelopeEncoder::EncodeStart(std::vector<uint8_t>* out) {
  assert(byte_size_pos_ == 0);
  out->push_back(kInitialByteForEnvelope);
  out->push_back(kInitialByteFor32BitLengthByteString);
  byte_size_pos_ = out->size();
  out->resize(out->size() + sizeof(uint32_t));
}

bool EnvelopeEncoder::EncodeStop(std::vector<uint8_t>* out) {
  assert(byte_size_pos_ != 0);
  // The byte size is the size of the payload, that is, all the
  // bytes that were written past the byte size position itself.
  uint64_t byte_size = out->size() - (byte_size_pos_ + sizeof(uint32_t));
  // We store exactly 4 bytes, so at most INT32MAX, with most significant
  // byte first.
  if (byte_size > std::numeric_limits<uint32_t>::max()) return false;
  for (int shift_bytes = sizeof(uint32_t) - 1; shift_bytes >= 0;
       --shift_bytes) {
    (*out)[byte_size_pos_++] = 0xff & (byte_size >> (shift_bytes * 8));
  }
  return true;
}

namespace {
class JSONToCBOREncoder : public JSONParserHandler {
 public:
  JSONToCBOREncoder(std::vector<uint8_t>* out, Status* status)
      : out_(out), status_(status) {
    *status_ = Status();
  }

  void HandleObjectBegin() override {
    envelopes_.emplace_back();
    envelopes_.back().EncodeStart(out_);
    out_->push_back(kInitialByteIndefiniteLengthMap);
  }

  void HandleObjectEnd() override {
    out_->push_back(kStopByte);
    assert(!envelopes_.empty());
    envelopes_.back().EncodeStop(out_);
    envelopes_.pop_back();
  }

  void HandleArrayBegin() override {
    envelopes_.emplace_back();
    envelopes_.back().EncodeStart(out_);
    out_->push_back(kInitialByteIndefiniteLengthArray);
  }

  void HandleArrayEnd() override {
    out_->push_back(kStopByte);
    assert(!envelopes_.empty());
    envelopes_.back().EncodeStop(out_);
    envelopes_.pop_back();
  }

  void HandleString8(span<uint8_t> chars) override {
    EncodeString8(chars, out_);
  }

  void HandleString16(span<uint16_t> chars) override {
    for (uint16_t ch : chars) {
      if (ch >= 0x7f) {
        // If there's at least one non-7bit character, we encode as UTF16.
        EncodeString16(chars, out_);
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

  void HandleDouble(double value) override { EncodeDouble(value, out_); }

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
  std::vector<EnvelopeEncoder> envelopes_;
  Status* status_;
};
}  // namespace

std::unique_ptr<JSONParserHandler> NewJSONToCBOREncoder(
    std::vector<uint8_t>* out, Status* status) {
  return std::unique_ptr<JSONParserHandler>(new JSONToCBOREncoder(out, status));
}

namespace {
// Below are three parsing routines for CBOR, which cover enough
// to roundtrip JSON messages.
bool ParseMap(int32_t stack_depth, CBORTokenizer* tokenizer,
              JSONParserHandler* out);
bool ParseArray(int32_t stack_depth, CBORTokenizer* tokenizer,
                JSONParserHandler* out);
bool ParseValue(int32_t stack_depth, CBORTokenizer* tokenizer,
                JSONParserHandler* out);

void ParseUTF16String(CBORTokenizer* tokenizer, JSONParserHandler* out) {
  std::vector<uint16_t> value;
  span<uint8_t> rep = tokenizer->GetString16WireRep();
  for (std::ptrdiff_t ii = 0; ii < rep.size(); ii += 2)
    value.push_back((rep[ii + 1] << 8) | rep[ii]);
  out->HandleString16(span<uint16_t>(value.data(), value.size()));
  tokenizer->Next();
}

bool ParseUTF8String(CBORTokenizer* tokenizer, JSONParserHandler* out) {
  assert(tokenizer->TokenTag() == CBORTokenTag::STRING8);
  out->HandleString8(tokenizer->GetString8());
  tokenizer->Next();
  return true;
}

bool ParseValue(int32_t stack_depth, CBORTokenizer* tokenizer,
                JSONParserHandler* out) {
  if (stack_depth > kStackLimit) {
    out->HandleError(
        Status{Error::CBOR_STACK_LIMIT_EXCEEDED, tokenizer->Status().pos});
    return false;
  }
  // Skip past the envelope to get to what's inside.
  if (tokenizer->TokenTag() == CBORTokenTag::ENVELOPE)
    tokenizer->EnterEnvelope();
  switch (tokenizer->TokenTag()) {
    case CBORTokenTag::ERROR_VALUE:
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
      return ParseUTF8String(tokenizer, out);
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
                JSONParserHandler* out) {
  assert(tokenizer->TokenTag() == CBORTokenTag::ARRAY_START);
  tokenizer->Next();
  out->HandleArrayBegin();
  while (tokenizer->TokenTag() != CBORTokenTag::STOP) {
    if (tokenizer->TokenTag() == CBORTokenTag::DONE) {
      out->HandleError(
          Status{Error::CBOR_UNEXPECTED_EOF_IN_ARRAY, tokenizer->Status().pos});
      return false;
    }
    if (tokenizer->TokenTag() == CBORTokenTag::ERROR_VALUE) {
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
              JSONParserHandler* out) {
  assert(tokenizer->TokenTag() == CBORTokenTag::MAP_START);
  out->HandleObjectBegin();
  tokenizer->Next();
  while (tokenizer->TokenTag() != CBORTokenTag::STOP) {
    if (tokenizer->TokenTag() == CBORTokenTag::DONE) {
      out->HandleError(
          Status{Error::CBOR_UNEXPECTED_EOF_IN_MAP, tokenizer->Status().pos});
      return false;
    }
    if (tokenizer->TokenTag() == CBORTokenTag::ERROR_VALUE) {
      out->HandleError(tokenizer->Status());
      return false;
    }
    // Parse key.
    if (tokenizer->TokenTag() == CBORTokenTag::STRING8) {
      if (!ParseUTF8String(tokenizer, out))
        return false;
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

void ParseCBOR(span<uint8_t> bytes, JSONParserHandler* json_out) {
  if (bytes.empty()) {
    json_out->HandleError(Status{Error::CBOR_NO_INPUT, 0});
    return;
  }
  if (bytes[0] != kInitialByteForEnvelope) {
    json_out->HandleError(Status{Error::CBOR_INVALID_START_BYTE, 0});
    return;
  }
  CBORTokenizer tokenizer(bytes);
  if (tokenizer.TokenTag() == CBORTokenTag::ERROR_VALUE) {
    json_out->HandleError(tokenizer.Status());
    return;
  }
  // We checked for the envelope start byte above, so the tokenizer
  // must agree here, since it's not an error.
  assert(tokenizer.TokenTag() == CBORTokenTag::ENVELOPE);
  tokenizer.EnterEnvelope();
  if (tokenizer.TokenTag() != CBORTokenTag::MAP_START) {
    json_out->HandleError(
        Status{Error::CBOR_MAP_START_EXPECTED, tokenizer.Status().pos});
    return;
  }
  if (!ParseMap(/*stack_depth=*/1, &tokenizer, json_out)) return;
  if (tokenizer.TokenTag() == CBORTokenTag::DONE) return;
  if (tokenizer.TokenTag() == CBORTokenTag::ERROR_VALUE) {
    json_out->HandleError(tokenizer.Status());
    return;
  }
  json_out->HandleError(
      Status{Error::CBOR_TRAILING_JUNK, tokenizer.Status().pos});
}

CBORTokenizer::CBORTokenizer(span<uint8_t> bytes) : bytes_(bytes) {
  ReadNextToken(/*enter_envelope=*/false);
}
CBORTokenizer::~CBORTokenizer() {}

CBORTokenTag CBORTokenizer::TokenTag() const { return token_tag_; }

void CBORTokenizer::Next() {
  if (token_tag_ == CBORTokenTag::ERROR_VALUE || token_tag_ == CBORTokenTag::DONE)
    return;
  ReadNextToken(/*enter_envelope=*/false);
}

void CBORTokenizer::EnterEnvelope() {
  assert(token_tag_ == CBORTokenTag::ENVELOPE);
  ReadNextToken(/*enter_envelope=*/true);
}

Status CBORTokenizer::Status() const { return status_; }

int32_t CBORTokenizer::GetInt32() const {
  assert(token_tag_ == CBORTokenTag::INT32);
  // The range checks happen in ::ReadNextToken().
  return static_cast<uint32_t>(
      token_start_type_ == MajorType::UNSIGNED
          ? token_start_internal_value_
          : -static_cast<int64_t>(token_start_internal_value_) - 1);
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
  auto length = static_cast<std::ptrdiff_t>(token_start_internal_value_);
  return bytes_.subspan(status_.pos + (token_byte_length_ - length), length);
}

span<uint8_t> CBORTokenizer::GetString16WireRep() const {
  assert(token_tag_ == CBORTokenTag::STRING16);
  auto length = static_cast<std::ptrdiff_t>(token_start_internal_value_);
  return bytes_.subspan(status_.pos + (token_byte_length_ - length), length);
}

span<uint8_t> CBORTokenizer::GetBinary() const {
  assert(token_tag_ == CBORTokenTag::BINARY);
  auto length = static_cast<std::ptrdiff_t>(token_start_internal_value_);
  return bytes_.subspan(status_.pos + (token_byte_length_ - length), length);
}

void CBORTokenizer::ReadNextToken(bool enter_envelope) {
  if (enter_envelope) {
    status_.pos += kEncodedEnvelopeHeaderSize;
  } else {
    status_.pos =
        status_.pos == Status::npos() ? 0 : status_.pos + token_byte_length_;
  }
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
      int8_t bytes_read =
          ReadTokenStart(bytes_.subspan(status_.pos + 1), &token_start_type_,
                         &token_start_internal_value_);
      int64_t token_byte_length = 1 + bytes_read + token_start_internal_value_;
      if (-1 == bytes_read || token_start_type_ != MajorType::BYTE_STRING ||
          status_.pos + token_byte_length > bytes_.size()) {
        SetError(Error::CBOR_INVALID_BINARY);
        return;
      }
      SetToken(CBORTokenTag::BINARY,
               static_cast<std::ptrdiff_t>(token_byte_length));
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
    case kInitialByteForEnvelope: {  // ENVELOPE
      if (status_.pos + kEncodedEnvelopeHeaderSize > bytes_.size()) {
        SetError(Error::CBOR_INVALID_ENVELOPE);
        return;
      }
      // The envelope must be a byte string with 32 bit length.
      if (bytes_[status_.pos + 1] != kInitialByteFor32BitLengthByteString) {
        SetError(Error::CBOR_INVALID_ENVELOPE);
        return;
      }
      // Read the length of the byte string.
      token_start_internal_value_ = ReadBytesMostSignificantByteFirst<uint32_t>(
          bytes_.subspan(status_.pos + 2));
      // Make sure the payload is contained within the message.
      if (token_start_internal_value_ + kEncodedEnvelopeHeaderSize +
              status_.pos >
          static_cast<std::size_t>(bytes_.size())) {
        SetError(Error::CBOR_INVALID_ENVELOPE);
        return;
      }
      auto length = static_cast<std::ptrdiff_t>(token_start_internal_value_);
      SetToken(CBORTokenTag::ENVELOPE,
               kEncodedEnvelopeHeaderSize + length);
      return;
    }
    default: {
      span<uint8_t> remainder =
          bytes_.subspan(status_.pos, bytes_.size() - status_.pos);
      assert(!remainder.empty());
      int8_t token_start_length = ReadTokenStart(remainder, &token_start_type_,
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
        case MajorType::STRING: {  // STRING8.
          if (!success || remainder.size() < static_cast<int64_t>(
                                                 token_start_internal_value_)) {
            SetError(Error::CBOR_INVALID_STRING8);
            return;
          }
          auto length = static_cast<std::ptrdiff_t>(token_start_internal_value_);
          SetToken(CBORTokenTag::STRING8, token_start_length + length);
          return;
        }
        case MajorType::BYTE_STRING: {  // STRING16.
          if (!success ||
              remainder.size() <
                  static_cast<int64_t>(token_start_internal_value_) ||
              // Must be divisible by 2 since UTF16 is 2 bytes per character.
              token_start_internal_value_ & 1) {
            SetError(Error::CBOR_INVALID_STRING16);
            return;
          }
          auto length = static_cast<std::ptrdiff_t>(token_start_internal_value_);
          SetToken(CBORTokenTag::STRING16, token_start_length + length);
          return;
        }
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
                             std::ptrdiff_t token_byte_length) {
  token_tag_ = token_tag;
  token_byte_length_ = token_byte_length;
}

void CBORTokenizer::SetError(Error error) {
  token_tag_ = CBORTokenTag::ERROR_VALUE;
  status_.error = error;
}

#if 0
void DumpCBOR(span<uint8_t> cbor) {
  std::string indent;
  CBORTokenizer tokenizer(cbor);
  while (true) {
    fprintf(stderr, "%s", indent.c_str());
    switch (tokenizer.TokenTag()) {
      case CBORTokenTag::ERROR_VALUE:
        fprintf(stderr, "ERROR {status.error=%d, status.pos=%ld}\n",
               tokenizer.Status().error, tokenizer.Status().pos);
        return;
      case CBORTokenTag::DONE:
        fprintf(stderr, "DONE\n");
        return;
      case CBORTokenTag::TRUE_VALUE:
        fprintf(stderr, "TRUE_VALUE\n");
        break;
      case CBORTokenTag::FALSE_VALUE:
        fprintf(stderr, "FALSE_VALUE\n");
        break;
      case CBORTokenTag::NULL_VALUE:
        fprintf(stderr, "NULL_VALUE\n");
        break;
      case CBORTokenTag::INT32:
        fprintf(stderr, "INT32 [%d]\n", tokenizer.GetInt32());
        break;
      case CBORTokenTag::DOUBLE:
        fprintf(stderr, "DOUBLE [%lf]\n", tokenizer.GetDouble());
        break;
      case CBORTokenTag::STRING8: {
        span<uint8_t> v = tokenizer.GetString8();
        std::string t(v.begin(), v.end());
        fprintf(stderr, "STRING8 [%s]\n", t.c_str());
        break;
      }
      case CBORTokenTag::STRING16: {
        span<uint8_t> v = tokenizer.GetString16WireRep();
        std::string t(v.begin(), v.end());
        fprintf(stderr, "STRING16 [%s]\n", t.c_str());
        break;
      }
      case CBORTokenTag::BINARY: {
        span<uint8_t> v = tokenizer.GetBinary();
        std::string t(v.begin(), v.end());
        fprintf(stderr, "BINARY [%s]\n", t.c_str());
        break;
      }
      case CBORTokenTag::MAP_START:
        fprintf(stderr, "MAP_START\n");
        indent += "  ";
        break;
      case CBORTokenTag::ARRAY_START:
        fprintf(stderr, "ARRAY_START\n");
        indent += "  ";
        break;
      case CBORTokenTag::STOP:
        fprintf(stderr, "STOP\n");
        indent.erase(0, 2);
        break;
      case CBORTokenTag::ENVELOPE:
        fprintf(stderr, "ENVELOPE\n");
        tokenizer.EnterEnvelope();
        continue;
    }
    tokenizer.Next();
  }
}
#endif

}  // namespace inspector_protocol
