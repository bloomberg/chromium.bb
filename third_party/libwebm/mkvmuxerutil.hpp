//
// Copyright (c) 2016, Alliance for Open Media. All rights reserved
//
// This source code is subject to the terms of the BSD 2 Clause License and
// the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
// was not distributed with this source code in the LICENSE file, you can
// obtain it at www.aomedia.org/license/software. If the Alliance for Open
// Media Patent License 1.0 was not distributed with this source code in the
// PATENTS file, you can obtain it at www.aomedia.org/license/patent.
//


#ifndef MKVMUXERUTIL_HPP
#define MKVMUXERUTIL_HPP

#include "mkvmuxer.hpp"
#include "mkvmuxertypes.hpp"

namespace mkvmuxer {

class IMkvWriter;

const uint64 kEbmlUnknownValue = 0x01FFFFFFFFFFFFFFULL;
const int64 kMaxBlockTimecode = 0x07FFFLL;

// Writes out |value| in Big Endian order. Returns 0 on success.
int32 SerializeInt(IMkvWriter* writer, int64 value, int32 size);

// Returns the size in bytes of the element.
int32 GetUIntSize(uint64 value);
int32 GetIntSize(int64 value);
int32 GetCodedUIntSize(uint64 value);
uint64 EbmlMasterElementSize(uint64 type, uint64 value);
uint64 EbmlElementSize(uint64 type, int64 value);
uint64 EbmlElementSize(uint64 type, uint64 value);
uint64 EbmlElementSize(uint64 type, float value);
uint64 EbmlElementSize(uint64 type, const char* value);
uint64 EbmlElementSize(uint64 type, const uint8* value, uint64 size);
uint64 EbmlDateElementSize(uint64 type);

// Creates an EBML coded number from |value| and writes it out. The size of
// the coded number is determined by the value of |value|. |value| must not
// be in a coded form. Returns 0 on success.
int32 WriteUInt(IMkvWriter* writer, uint64 value);

// Creates an EBML coded number from |value| and writes it out. The size of
// the coded number is determined by the value of |size|. |value| must not
// be in a coded form. Returns 0 on success.
int32 WriteUIntSize(IMkvWriter* writer, uint64 value, int32 size);

// Output an Mkv master element. Returns true if the element was written.
bool WriteEbmlMasterElement(IMkvWriter* writer, uint64 value, uint64 size);

// Outputs an Mkv ID, calls |IMkvWriter::ElementStartNotify|, and passes the
// ID to |SerializeInt|. Returns 0 on success.
int32 WriteID(IMkvWriter* writer, uint64 type);

// Output an Mkv non-master element. Returns true if the element was written.
bool WriteEbmlElement(IMkvWriter* writer, uint64 type, uint64 value);
bool WriteEbmlElement(IMkvWriter* writer, uint64 type, int64 value);
bool WriteEbmlElement(IMkvWriter* writer, uint64 type, float value);
bool WriteEbmlElement(IMkvWriter* writer, uint64 type, const char* value);
bool WriteEbmlElement(IMkvWriter* writer, uint64 type, const uint8* value,
                      uint64 size);
bool WriteEbmlDateElement(IMkvWriter* writer, uint64 type, int64 value);

// Output a Mkv Frame. It decides the correct element to write (Block vs
// SimpleBlock) based on the parameters of the Frame.
uint64 WriteFrame(IMkvWriter* writer, const Frame* const frame,
                  Cluster* cluster);

// Output a void element. |size| must be the entire size in bytes that will be
// void. The function will calculate the size of the void header and subtract
// it from |size|.
uint64 WriteVoidElement(IMkvWriter* writer, uint64 size);

// Returns the version number of the muxer in |major|, |minor|, |build|,
// and |revision|.
void GetVersion(int32* major, int32* minor, int32* build, int32* revision);

// Returns a random number to be used for UID, using |seed| to seed
// the random-number generator (see POSIX rand_r() for semantics).
uint64 MakeUID(unsigned int* seed);

}  // end namespace mkvmuxer

#endif  // MKVMUXERUTIL_HPP
