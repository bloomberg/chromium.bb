// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_CBOR_H_
#define INSPECTOR_PROTOCOL_ENCODING_CBOR_H_

#include <cstdint>
#include <memory>
#include <vector>
#include "cbor_internals.h"
#include "json_parser_handler.h"
#include "span.h"
#include "status.h"

namespace inspector_protocol {
// The binary encoding for the inspector protocol follows the CBOR specification
// (RFC 7049). Additional constraints:
// - Only indefinite length maps and arrays are supported.
// - At the top level, a message must be an indefinite length map.
// - For scalars, we support only the int32_t range, encoded as
//   UNSIGNED/NEGATIVE (major types 0 / 1).
// - UTF16 strings, including with unbalanced surrogate pairs, are encoded
//   as CBOR BYTE_STRING (major type 2). For such strings, the number of
//   bytes encoded must be even.
// - UTF8 strings (major type 3) may only have ASCII characters
//   (7 bit US-ASCII).
// - Arbitrary byte arrays, in the inspector protocol called 'binary',
//   are encoded as BYTE_STRING (major type 2), prefixed with a byte
//   indicating base64 when rendered as JSON.

// Encodes |value| as |UNSIGNED| (major type 0) iff >= 0, or |NEGATIVE|
// (major type 1) iff < 0.
void EncodeInt32(int32_t value, std::vector<uint8_t>* out);

// Encodes a UTF16 string as a BYTE_STRING (major type 2). Each utf16
// character in |in| is emitted with most significant byte first,
// appending to |out|.
void EncodeString16(span<uint16_t> in, std::vector<uint8_t>* out);

// Encodes a UTF8 string |in| as STRING (major type 3).
void EncodeString8(span<uint8_t> in, std::vector<uint8_t>* out);

// Encodes arbitrary binary data in |in| as a BYTE_STRING (major type 2) with
// definitive length, prefixed with tag 22 indicating expected conversion to
// base64 (see RFC 7049, Table 3 and Section 2.4.4.2).
void EncodeBinary(span<uint8_t> in, std::vector<uint8_t>* out);

// Encodes / decodes a double as Major type 7 (SIMPLE_VALUE),
// with additional info = 27, followed by 8 bytes in big endian.
void EncodeDouble(double value, std::vector<uint8_t>* out);

// This can be used to convert from JSON to CBOR, by passing the
// return value to the routines in json_parser.h.  The handler will encode into
// |out|, and iff an error occurs it will set |status| to an error and clear
// |out|. Otherwise, |status.ok()| will be |true|.
std::unique_ptr<JsonParserHandler> NewJsonToCBOREncoder(
    std::vector<uint8_t>* out, Status* status);

// Parses a CBOR encoded message from |bytes|, sending JSON events to
// |json_out|. If an error occurs, sends |out->HandleError|, and parsing stops.
// The client is responsible for discarding the already received information in
// that case.
void ParseCBOR(span<uint8_t> bytes, JsonParserHandler* json_out);

// Tags for the tokens within a CBOR message that CBORStream understands.
// Note that this is not the same terminology as the CBOR spec (RFC 7049),
// but rather, our adaptation. For instance, we lump unsigned and signed
// major type into INT32 here (and disallow values outside the int32_t range).
enum class CBORTokenTag {
  // Encountered an error in the structure of the message. Consult
  // status() for details.
  ERROR,
  // Booleans and NULL.
  TRUE_VALUE,
  FALSE_VALUE,
  NULL_VALUE,
  // An int32_t (signed 32 bit integer).
  INT32,
  // A double (64 bit floating point).
  DOUBLE,
  // A UTF8 string.
  STRING8,
  // A UTF16 string.
  STRING16,
  // A binary string.
  BINARY,
  // Starts an indefinite length map; after the map start we expect
  // alternating keys and values, followed by STOP.
  MAP_START,
  // Starts an indefinite length array; after the array start we
  // expect values, followed by STOP.
  ARRAY_START,
  // Ends a map or an array.
  STOP,
  // We've reached the end there is nothing else to read.
  DONE,
};

// CBORTokenizer segments a CBOR message, presenting the tokens therein as
// numbers, strings, etc. This is not a complete CBOR parser, but makes it much
// easier to implement one (e.g. ParseCBOR, above). It can also be used to parse
// messages partially.
class CBORTokenizer {
 public:
  explicit CBORTokenizer(span<uint8_t> bytes);
  ~CBORTokenizer();

  // Identifies the current token that we're looking at,
  // or ERROR (in which ase ::Status() has details)
  // or DONE (if we're past the last token).
  CBORTokenTag TokenTag() const;

  // Advances to the next token.
  void Next();

  // If TokenTag() is CBORTokenTag::ERROR, then Status().error describes
  // the error more precisely; otherwise it'll be set to Error::OK.
  // In either case, Status().pos is the current position.
  inspector_protocol::Status Status() const;

  // The following methods retrieve the token values. They can only
  // be called if TokenTag() matches.

  // To be called only if ::TokenTag() == CBORTokenTag::INT32.
  int32_t GetInt32() const;

  // To be called only if ::TokenTag() == CBORTokenTag::DOUBLE.
  double GetDouble() const;

  // To be called only if ::TokenTag() == CBORTokenTag::STRING8.
  span<uint8_t> GetString8() const;

  // Wire representation for STRING16 is low byte first (little endian).
  // To be called only if ::TokenTag() == CBORTokenTag::STRING16.
  span<uint8_t> GetString16WireRep() const;

  // To be called only if ::TokenTag() == CBORTokenTag::BINARY.
  span<uint8_t> GetBinary() const;

 private:
  void ReadNextToken();
  void SetToken(CBORTokenTag token, int64_t token_byte_length);
  void SetError(Error error);

  span<uint8_t> bytes_;
  CBORTokenTag token_tag_;
  inspector_protocol::Status status_;
  int64_t token_byte_length_;
  cbor_internals::MajorType token_start_type_;
  uint64_t token_start_internal_value_;
};
}  // namespace inspector_protocol
#endif  // INSPECTOR_PROTOCOL_ENCODING_CBOR_H_
