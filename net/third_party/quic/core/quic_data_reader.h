// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_DATA_READER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_DATA_READER_H_

#include <cstddef>
#include <cstdint>

#include "base/macros.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_endian.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace net {

// Used for reading QUIC data. Though there isn't really anything terribly
// QUIC-specific here, it's a helper class that's useful when doing QUIC
// framing.
//
// To use, simply construct a QuicDataReader using the underlying buffer that
// you'd like to read fields from, then call one of the Read*() methods to
// actually do some reading.
//
// This class keeps an internal iterator to keep track of what's already been
// read and each successive Read*() call automatically increments said iterator
// on success. On failure, internal state of the QuicDataReader should not be
// trusted and it is up to the caller to throw away the failed instance and
// handle the error as appropriate. None of the Read*() methods should ever be
// called after failure, as they will also fail immediately.
class QUIC_EXPORT_PRIVATE QuicDataReader {
 public:
  // Caller must provide an underlying buffer to work on.
  QuicDataReader(const char* data, const size_t len, Endianness endianness);

  // Empty destructor.
  ~QuicDataReader() {}

  // Reads an 8/16/32/64-bit unsigned integer into the given output
  // parameter. Forwards the internal iterator on success. Returns true on
  // success, false otherwise.
  bool ReadUInt8(uint8_t* result);
  bool ReadUInt16(uint16_t* result);
  bool ReadUInt32(uint32_t* result);
  bool ReadUInt64(uint64_t* result);

  // Set |result| to 0, then read |num_bytes| bytes in the correct byte order
  // into least significant bytes of |result|.
  bool ReadBytesToUInt64(size_t num_bytes, uint64_t* result);

  // Reads a 16-bit unsigned float into the given output parameter.
  // Forwards the internal iterator on success.
  // Returns true on success, false otherwise.
  bool ReadUFloat16(uint64_t* result);

  // Reads a string prefixed with 16-bit length into the given output parameter.
  //
  // NOTE: Does not copy but rather references strings in the underlying buffer.
  // This should be kept in mind when handling memory management!
  //
  // Forwards the internal iterator on success.
  // Returns true on success, false otherwise.
  bool ReadStringPiece16(QuicStringPiece* result);

  // Reads a given number of bytes into the given buffer. The buffer
  // must be of adequate size.
  // Forwards the internal iterator on success.
  // Returns true on success, false otherwise.
  bool ReadStringPiece(QuicStringPiece* result, size_t len);

  // Reads connection ID represented as 64-bit unsigned integer into the given
  // output parameter.
  // Forwards the internal iterator on success.
  // Returns true on success, false otherwise.
  // TODO(fayang): Remove this method and use ReadUInt64() once deprecating
  // quic_restart_flag_quic_rw_cid_in_big_endian and QuicDataReader has a mode
  // indicating reading in little/big endian.
  bool ReadConnectionId(uint64_t* connection_id);

  // Returns the remaining payload as a QuicStringPiece.
  // Reads tag represented as 32-bit unsigned integer into given output
  // parameter. Tags are in big endian on the wire (e.g., CHLO is
  // 'C','H','L','O') and are read in byte order, so tags in memory are in big
  // endian.
  bool ReadTag(uint32_t* tag);

  // Returns the remaining payload as a QuicStringPiece.
  //
  // NOTE: Does not copy but rather references strings in the underlying buffer.
  // This should be kept in mind when handling memory management!
  //
  // Forwards the internal iterator.
  QuicStringPiece ReadRemainingPayload();

  // Returns the remaining payload as a QuicStringPiece.
  //
  // NOTE: Does not copy but rather references strings in the underlying buffer.
  // This should be kept in mind when handling memory management!
  //
  // DOES NOT forward the internal iterator.
  QuicStringPiece PeekRemainingPayload();

  // Reads a given number of bytes into the given buffer. The buffer
  // must be of adequate size.
  // Forwards the internal iterator on success.
  // Returns true on success, false otherwise.
  bool ReadBytes(void* result, size_t size);

  // Returns true if the entirety of the underlying buffer has been read via
  // Read*() calls.
  bool IsDoneReading() const;

  // Returns the number of bytes remaining to be read.
  size_t BytesRemaining() const;

  // Returns the next byte that to be read. Must not be called when there are no
  // bytes to be read.
  //
  // DOES NOT forward the internal iterator.
  uint8_t PeekByte() const;

  void set_endianness(Endianness endianness) { endianness_ = endianness; }

  // Read an IETF-encoded Variable Length Integer and place the result
  // in |*result|.
  // Returns true if it works, false if not. The only error is that
  // there is not enough in the buffer to read the number.
  // If there is an error, |*result| is not altered.
  // Numbers are encoded per the rules in draft-ietf-quic-transport-10.txt
  // and that the integers in the range 0 ... (2^62)-1.
  bool ReadVarInt62(uint64_t* result);

  // Convenience method that reads a StreamId.
  // Atempts to read a Stream ID into |result| using ReadVarInt62 and
  // returns false if there is a read error or if the value is
  // greater than (2^32)-1.
  bool ReadVarIntStreamId(QuicStreamId* result);

 private:
  // Returns true if the underlying buffer has enough room to read the given
  // amount of bytes.
  bool CanRead(size_t bytes) const;

  // To be called when a read fails for any reason.
  void OnFailure();

  // TODO(fkastenholz, b/73004262) change buffer_, et al, to be uint8_t, not
  // char. The data buffer that we're reading from.
  const char* data_;

  // The length of the data buffer that we're reading from.
  const size_t len_;

  // The location of the next read from our data buffer.
  size_t pos_;

  // The endianness to read integers and floating numbers.
  Endianness endianness_;

  DISALLOW_COPY_AND_ASSIGN(QuicDataReader);
};

}  // namespace net

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_DATA_READER_H_
