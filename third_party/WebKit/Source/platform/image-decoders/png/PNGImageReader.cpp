/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * Portions are Copyright (C) 2001 mozilla.org
 *
 * Other contributors:
 *   Stuart Parmenter <stuart@mozilla.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "platform/image-decoders/png/PNGImageReader.h"

#include <memory>
#include "platform/image-decoders/FastSharedBufferReader.h"
#include "platform/image-decoders/SegmentReader.h"
#include "platform/image-decoders/png/PNGImageDecoder.h"
#include "wtf/PtrUtil.h"
#include "zlib.h"

namespace {

inline blink::PNGImageDecoder* imageDecoder(png_structp png) {
  return static_cast<blink::PNGImageDecoder*>(png_get_progressive_ptr(png));
}

void PNGAPI pngHeaderAvailable(png_structp png, png_infop) {
  imageDecoder(png)->headerAvailable();
}

void PNGAPI pngRowAvailable(png_structp png,
                            png_bytep row,
                            png_uint_32 rowIndex,
                            int state) {
  imageDecoder(png)->rowAvailable(row, rowIndex, state);
}

void PNGAPI pngFrameComplete(png_structp png, png_infop) {
  imageDecoder(png)->frameComplete();
}

void PNGAPI pngFailed(png_structp png, png_const_charp) {
  longjmp(JMPBUF(png), 1);
}

}  // namespace

namespace blink {

PNGImageReader::PNGImageReader(PNGImageDecoder* decoder, size_t initialOffset)
    : m_width(0),
      m_height(0),
      m_decoder(decoder),
      m_initialOffset(initialOffset),
      m_readOffset(initialOffset),
      m_progressiveDecodeOffset(0),
      m_idatOffset(0),
      m_idatIsPartOfAnimation(false),
      m_expectIdats(true),
      m_isAnimated(false),
      m_parsedSignature(false),
      m_parsedIHDR(false),
      m_parseCompleted(false),
      m_reportedFrameCount(0),
      m_nextSequenceNumber(0),
      m_fctlNeedsDatChunk(false),
      m_ignoreAnimation(false) {
  m_png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, pngFailed, 0);
  m_info = png_create_info_struct(m_png);
  png_set_progressive_read_fn(m_png, m_decoder, nullptr, pngRowAvailable,
                              pngFrameComplete);
}

PNGImageReader::~PNGImageReader() {
  png_destroy_read_struct(m_png ? &m_png : 0, m_info ? &m_info : 0, 0);
  DCHECK(!m_png && !m_info);
}

// This method reads from the FastSharedBufferReader, starting at offset,
// and returns |length| bytes in the form of a pointer to a const png_byte*.
// This function is used to make it easy to access data from the reader in a
// png friendly way, and pass it to libpng for decoding.
//
// Pre-conditions before using this:
// - |reader|.size() >= |readOffset| + |length|
// - |buffer|.size() >= |length|
// - |length| <= |kBufferSize|
//
// The reason for the last two precondition is that currently the png signature
// plus IHDR chunk (8B + 25B = 33B) is the largest chunk that is read using this
// method. If the data is not consecutive, it is stored in |buffer|, which must
// have the size of (at least) |length|, but there's no need for it to be larger
// than |kBufferSize|.
static constexpr size_t kBufferSize = 33;
const png_byte* readAsConstPngBytep(const FastSharedBufferReader& reader,
                                    size_t readOffset,
                                    size_t length,
                                    char* buffer) {
  DCHECK(length <= kBufferSize);
  return reinterpret_cast<const png_byte*>(
      reader.getConsecutiveData(readOffset, length, buffer));
}

bool PNGImageReader::shouldDecodeWithNewPNG(size_t index) const {
  if (!m_png)
    return true;
  const bool firstFrameDecodeInProgress = m_progressiveDecodeOffset;
  const bool frameSizeMatchesIHDR =
      m_frameInfo[index].frameRect == IntRect(0, 0, m_width, m_height);
  if (index)
    return firstFrameDecodeInProgress || !frameSizeMatchesIHDR;
  return !firstFrameDecodeInProgress && !frameSizeMatchesIHDR;
}

// Return false on a fatal error.
bool PNGImageReader::decode(SegmentReader& data, size_t index) {
  if (index >= m_frameInfo.size())
    return true;

  const FastSharedBufferReader reader(&data);

  if (!m_isAnimated) {
    if (setjmp(JMPBUF(m_png)))
      return false;
    DCHECK_EQ(0u, index);
    m_progressiveDecodeOffset += processData(
        reader, m_frameInfo[0].startOffset + m_progressiveDecodeOffset, 0);
    return true;
  }

  DCHECK(m_isAnimated);

  const bool decodeWithNewPNG = shouldDecodeWithNewPNG(index);
  if (decodeWithNewPNG) {
    clearDecodeState(0);
    m_png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, pngFailed, 0);
    m_info = png_create_info_struct(m_png);
    png_set_progressive_read_fn(m_png, m_decoder, pngHeaderAvailable,
                                pngRowAvailable, pngFrameComplete);
  }

  if (setjmp(JMPBUF(m_png)))
    return false;

  if (decodeWithNewPNG)
    startFrameDecoding(reader, index);

  if (!index && (!firstFrameFullyReceived() || m_progressiveDecodeOffset)) {
    const bool decodedEntireFrame = progressivelyDecodeFirstFrame(reader);
    if (!decodedEntireFrame)
      return true;
    m_progressiveDecodeOffset = 0;
  } else {
    decodeFrame(reader, index);
  }

  static png_byte IEND[12] = {0, 0, 0, 0, 'I', 'E', 'N', 'D', 174, 66, 96, 130};
  png_process_data(m_png, m_info, IEND, 12);
  png_destroy_read_struct(&m_png, &m_info, 0);
  DCHECK(!m_png && !m_info);

  return true;
}

void PNGImageReader::startFrameDecoding(const FastSharedBufferReader& reader,
                                        size_t index) {
  // If the frame is the size of the whole image, just re-process all header
  // data up to the first frame.
  const IntRect& frameRect = m_frameInfo[index].frameRect;
  if (frameRect == IntRect(0, 0, m_width, m_height)) {
    processData(reader, m_initialOffset, m_idatOffset);
    return;
  }

  // Process the IHDR chunk, but change the width and height so it reflects
  // the frame's width and height. ImageDecoder will apply the x,y offset.
  constexpr size_t headerSize = kBufferSize;
  char readBuffer[headerSize];
  const png_byte* chunk =
      readAsConstPngBytep(reader, m_initialOffset, headerSize, readBuffer);
  png_byte* header = reinterpret_cast<png_byte*>(readBuffer);
  if (chunk != header)
    memcpy(header, chunk, headerSize);
  png_save_uint_32(header + 16, frameRect.width());
  png_save_uint_32(header + 20, frameRect.height());
  // IHDR has been modified, so tell libpng to ignore CRC errors.
  png_set_crc_action(m_png, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
  png_process_data(m_png, m_info, header, headerSize);

  // Process the rest of the header chunks.
  processData(reader, m_initialOffset + headerSize, m_idatOffset - headerSize);
}

// Determine if the bytes 4 to 7 of |chunk| indicate that it is a |tag| chunk.
// - The length of |chunk| must be >= 8
// - The length of |tag| must be = 4
static inline bool isChunk(const png_byte* chunk, const char* tag) {
  return memcmp(chunk + 4, tag, 4) == 0;
}

bool PNGImageReader::progressivelyDecodeFirstFrame(
    const FastSharedBufferReader& reader) {
  size_t offset = m_frameInfo[0].startOffset;

  // Loop while there is enough data to do progressive decoding.
  while (reader.size() >= offset + 8) {
    char readBuffer[8];
    // At the beginning of each loop, the offset is at the start of a chunk.
    const png_byte* chunk = readAsConstPngBytep(reader, offset, 8, readBuffer);
    const png_uint_32 length = png_get_uint_32(chunk);
    DCHECK(length <= PNG_UINT_31_MAX);

    // When an fcTL or IEND chunk is encountered, the frame data has ended.
    // Return true, since all frame data is decoded.
    if (isChunk(chunk, "fcTL") || isChunk(chunk, "IEND"))
      return true;

    // If this chunk was already decoded, move on to the next.
    if (m_progressiveDecodeOffset >= offset + length + 12) {
      offset += length + 12;
      continue;
    }

    // Three scenarios are possible here:
    // 1) Some bytes of this chunk were already decoded in a previous call.
    //    Continue from there.
    // 2) This is an fdAT chunk. Convert it to an IDAT chunk to decode.
    // 3) This is any other chunk. Pass it to libpng for processing.
    size_t endOffsetChunk = offset + length + 12;

    if (m_progressiveDecodeOffset >= offset + 8) {
      offset = m_progressiveDecodeOffset;
    } else if (isChunk(chunk, "fdAT")) {
      processFdatChunkAsIdat(length);
      // Skip the sequence number.
      offset += 12;
    } else {
      png_process_data(m_png, m_info, const_cast<png_byte*>(chunk), 8);
      offset += 8;
    }

    size_t bytesLeftInChunk = endOffsetChunk - offset;
    size_t bytesDecoded = processData(reader, offset, bytesLeftInChunk);
    m_progressiveDecodeOffset = offset + bytesDecoded;
    if (bytesDecoded < bytesLeftInChunk)
      return false;
    offset += bytesDecoded;
  }

  return false;
}

void PNGImageReader::processFdatChunkAsIdat(png_uint_32 fdatLength) {
  // An fdAT chunk is build up as follows:
  // - |length| (4B)
  // - fdAT tag (4B)
  // - sequence number (4B)
  // - frame data (|length| - 4B)
  // - CRC (4B)
  // Thus, to reformat this into an IDAT chunk, do the following:
  // - write |length| - 4 as the new length, since the sequence number
  //   must be removed.
  // - change the tag to IDAT.
  // - omit the sequence number from the data part of the chunk.
  png_byte chunkIDAT[] = {0, 0, 0, 0, 'I', 'D', 'A', 'T'};
  png_save_uint_32(chunkIDAT, fdatLength - 4);
  // The CRC is incorrect when applied to the modified fdAT.
  png_set_crc_action(m_png, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
  png_process_data(m_png, m_info, chunkIDAT, 8);
}

void PNGImageReader::decodeFrame(const FastSharedBufferReader& reader,
                                 size_t index) {
  size_t offset = m_frameInfo[index].startOffset;
  size_t endOffset = offset + m_frameInfo[index].byteLength;
  char readBuffer[8];

  while (offset < endOffset) {
    const png_byte* chunk = readAsConstPngBytep(reader, offset, 8, readBuffer);
    const png_uint_32 length = png_get_uint_32(chunk);
    DCHECK(length <= PNG_UINT_31_MAX);

    if (isChunk(chunk, "fdAT")) {
      processFdatChunkAsIdat(length);
      // The frame data and the CRC span |length| bytes, so skip the
      // sequence number and process |length| bytes to decode the frame.
      processData(reader, offset + 12, length);
    } else {
      png_process_data(m_png, m_info, const_cast<png_byte*>(chunk), 8);
      processData(reader, offset + 8, length + 4);
    }

    offset += 12 + length;
  }
}

// Compute the CRC and compare to the stored value.
static bool checkCrc(const FastSharedBufferReader& reader,
                     size_t chunkStart,
                     size_t chunkLength) {
  constexpr size_t kSizeNeededForfcTL = 26 + 4;
  char readBuffer[kSizeNeededForfcTL];
  DCHECK(chunkLength + 4 <= kSizeNeededForfcTL);
  const png_byte* chunk =
      readAsConstPngBytep(reader, chunkStart + 4, chunkLength + 4, readBuffer);

  char crcBuffer[4];
  const png_byte* crcPosition =
      readAsConstPngBytep(reader, chunkStart + 8 + chunkLength, 4, crcBuffer);
  png_uint_32 crc = png_get_uint_32(crcPosition);
  return crc == crc32(crc32(0, Z_NULL, 0), chunk, chunkLength + 4);
}

bool PNGImageReader::checkSequenceNumber(const png_byte* position) {
  png_uint_32 sequence = png_get_uint_32(position);
  if (sequence != m_nextSequenceNumber || sequence > PNG_UINT_31_MAX)
    return false;

  ++m_nextSequenceNumber;
  return true;
}

// Return false if there was a fatal error; true otherwise.
bool PNGImageReader::parse(SegmentReader& data, ParseQuery query) {
  if (m_parseCompleted)
    return true;

  const FastSharedBufferReader reader(&data);

  if (!parseSize(reader))
    return false;

  if (!m_decoder->isDecodedSizeAvailable())
    return true;

  // For non animated images (identified by no acTL chunk before the IDAT),
  // there is no need to continue parsing.
  if (!m_isAnimated) {
    FrameInfo frame;
    frame.startOffset = m_readOffset;
    // This should never be read in this case, but initialize just in case.
    frame.byteLength = kFirstFrameIndicator;
    frame.duration = 0;
    frame.frameRect = IntRect(0, 0, m_width, m_height);
    frame.disposalMethod = ImageFrame::DisposalMethod::DisposeKeep;
    frame.alphaBlend = ImageFrame::AlphaBlendSource::BlendAtopBgcolor;
    DCHECK(m_frameInfo.isEmpty());
    m_frameInfo.push_back(frame);
    m_parseCompleted = true;
    return true;
  }

  if (query == ParseQuery::Size)
    return true;

  DCHECK_EQ(ParseQuery::MetaData, query);
  DCHECK(m_isAnimated);

  // Loop over the data and manually register all frames. Nothing is passed to
  // libpng for processing. A frame is registered on the next fcTL chunk or
  // when the IEND chunk is found. This ensures that only complete frames are
  // reported, unless there is an error in the stream.
  char readBuffer[kBufferSize];
  while (reader.size() >= m_readOffset + 8) {
    const png_byte* chunk =
        readAsConstPngBytep(reader, m_readOffset, 8, readBuffer);
    const size_t length = png_get_uint_32(chunk);
    if (length > PNG_UINT_31_MAX)
      return false;

    const bool IDAT = isChunk(chunk, "IDAT");
    if (IDAT && !m_expectIdats)
      return false;

    const bool fdAT = isChunk(chunk, "fdAT");
    if (fdAT && m_expectIdats)
      return false;

    if (fdAT || (IDAT && m_idatIsPartOfAnimation)) {
      m_fctlNeedsDatChunk = false;
      if (!m_newFrame.startOffset) {
        // Beginning of a new frame's data.
        m_newFrame.startOffset = m_readOffset;

        if (m_frameInfo.isEmpty()) {
          // This is the first frame. Report it immediately so it can be
          // decoded progressively.
          m_newFrame.byteLength = kFirstFrameIndicator;
          m_frameInfo.push_back(m_newFrame);
        }
      }

      if (fdAT) {
        if (reader.size() < m_readOffset + 8 + 4)
          return true;
        const png_byte* sequencePosition =
            readAsConstPngBytep(reader, m_readOffset + 8, 4, readBuffer);
        if (!checkSequenceNumber(sequencePosition))
          return false;
      }

    } else if (isChunk(chunk, "fcTL") || isChunk(chunk, "IEND")) {
      // This marks the end of the previous frame.
      if (m_newFrame.startOffset) {
        m_newFrame.byteLength = m_readOffset - m_newFrame.startOffset;
        if (m_frameInfo[0].byteLength == kFirstFrameIndicator) {
          m_frameInfo[0].byteLength = m_newFrame.byteLength;
        } else {
          m_frameInfo.push_back(m_newFrame);
          if (isChunk(chunk, "fcTL")) {
            if (m_frameInfo.size() >= m_reportedFrameCount)
              return false;
          } else {  // IEND
            if (m_frameInfo.size() != m_reportedFrameCount)
              return false;
          }
        }

        m_newFrame.startOffset = 0;
      }

      if (reader.size() < m_readOffset + 12 + length)
        return true;

      if (isChunk(chunk, "IEND")) {
        m_parseCompleted = true;
        return true;
      }

      if (length != 26 || !checkCrc(reader, m_readOffset, length))
        return false;

      chunk = readAsConstPngBytep(reader, m_readOffset + 8, length, readBuffer);
      if (!parseFrameInfo(chunk))
        return false;

      m_expectIdats = false;
    } else if (isChunk(chunk, "acTL")) {
      // There should only be one acTL chunk, and it should be before the
      // IDAT chunk.
      return false;
    }

    m_readOffset += 12 + length;
  }
  return true;
}

// If |length| == 0, read until the stream ends. Return number of bytes
// processed.
size_t PNGImageReader::processData(const FastSharedBufferReader& reader,
                                   size_t offset,
                                   size_t length) {
  const char* segment;
  size_t totalProcessedBytes = 0;
  while (reader.size() > offset) {
    size_t segmentLength = reader.getSomeData(segment, offset);
    if (length > 0 && segmentLength + totalProcessedBytes > length)
      segmentLength = length - totalProcessedBytes;

    png_process_data(m_png, m_info,
                     reinterpret_cast<png_byte*>(const_cast<char*>(segment)),
                     segmentLength);
    offset += segmentLength;
    totalProcessedBytes += segmentLength;
    if (totalProcessedBytes == length)
      return length;
  }
  return totalProcessedBytes;
}

// Process up to the start of the IDAT with libpng.
// Return false for a fatal error. True otherwise.
bool PNGImageReader::parseSize(const FastSharedBufferReader& reader) {
  if (m_decoder->isDecodedSizeAvailable())
    return true;

  char readBuffer[kBufferSize];

  if (setjmp(JMPBUF(m_png)))
    return false;

  if (!m_parsedSignature) {
    if (reader.size() < m_readOffset + 8)
      return true;

    const png_byte* chunk =
        readAsConstPngBytep(reader, m_readOffset, 8, readBuffer);
    png_process_data(m_png, m_info, const_cast<png_byte*>(chunk), 8);
    m_readOffset += 8;
    m_parsedSignature = true;
    m_newFrame.startOffset = 0;
  }

  // Process APNG chunks manually, pass other chunks to libpng.
  for (png_uint_32 length = 0; reader.size() >= m_readOffset + 8;
       m_readOffset += length + 12) {
    const png_byte* chunk =
        readAsConstPngBytep(reader, m_readOffset, 8, readBuffer);
    length = png_get_uint_32(chunk);

    if (isChunk(chunk, "IDAT")) {
      // Done with header chunks.
      m_idatOffset = m_readOffset;
      m_fctlNeedsDatChunk = false;
      if (m_ignoreAnimation)
        m_isAnimated = false;
      if (!m_isAnimated || 1 == m_reportedFrameCount)
        m_decoder->setRepetitionCount(cAnimationNone);
      if (!m_decoder->setSize(m_width, m_height))
        return false;
      m_decoder->setColorSpace();
      m_decoder->headerAvailable();
      return true;
    }

    // Wait until the entire chunk is available for parsing simplicity.
    if (reader.size() < m_readOffset + length + 12)
      break;

    if (isChunk(chunk, "acTL")) {
      if (m_ignoreAnimation)
        continue;
      if (m_isAnimated || length != 8 || !m_parsedIHDR ||
          !checkCrc(reader, m_readOffset, 8)) {
        m_ignoreAnimation = true;
        continue;
      }
      chunk = readAsConstPngBytep(reader, m_readOffset + 8, length, readBuffer);
      m_reportedFrameCount = png_get_uint_32(chunk);
      if (!m_reportedFrameCount || m_reportedFrameCount > PNG_UINT_31_MAX) {
        m_ignoreAnimation = true;
        continue;
      }
      png_uint_32 repetitionCount = png_get_uint_32(chunk + 4);
      if (repetitionCount > PNG_UINT_31_MAX) {
        m_ignoreAnimation = true;
        continue;
      }
      m_isAnimated = true;
      m_decoder->setRepetitionCount(static_cast<int>(repetitionCount) - 1);
    } else if (isChunk(chunk, "fcTL")) {
      if (m_ignoreAnimation)
        continue;
      if (length != 26 || !m_parsedIHDR ||
          !checkCrc(reader, m_readOffset, 26)) {
        m_ignoreAnimation = true;
        continue;
      }
      chunk = readAsConstPngBytep(reader, m_readOffset + 8, length, readBuffer);
      if (!parseFrameInfo(chunk) ||
          m_newFrame.frameRect != IntRect(0, 0, m_width, m_height)) {
        m_ignoreAnimation = true;
        continue;
      }
      m_idatIsPartOfAnimation = true;
    } else if (isChunk(chunk, "fdAT")) {
      m_ignoreAnimation = true;
    } else {
      png_process_data(m_png, m_info, const_cast<png_byte*>(chunk), 8);
      processData(reader, m_readOffset + 8, length + 4);
      if (isChunk(chunk, "IHDR")) {
        m_parsedIHDR = true;
        m_width = png_get_image_width(m_png, m_info);
        m_height = png_get_image_height(m_png, m_info);
      }
    }
  }

  // Not enough data to call headerAvailable.
  return true;
}

void PNGImageReader::clearDecodeState(size_t index) {
  if (index)
    return;
  png_destroy_read_struct(m_png ? &m_png : nullptr,
                          m_info ? &m_info : nullptr, 0);
  DCHECK(!m_png && !m_info);
  m_progressiveDecodeOffset = 0;
}

const PNGImageReader::FrameInfo& PNGImageReader::frameInfo(size_t index) const {
  DCHECK(index < m_frameInfo.size());
  return m_frameInfo[index];
}

// Extract the fcTL frame control info and store it in m_newFrame. The length
// check on the fcTL data has been done by the calling code.
bool PNGImageReader::parseFrameInfo(const png_byte* data) {
  if (m_fctlNeedsDatChunk)
    return false;

  png_uint_32 frameWidth = png_get_uint_32(data + 4);
  png_uint_32 frameHeight = png_get_uint_32(data + 8);
  png_uint_32 xOffset = png_get_uint_32(data + 12);
  png_uint_32 yOffset = png_get_uint_32(data + 16);
  png_uint_16 delayNumerator = png_get_uint_16(data + 20);
  png_uint_16 delayDenominator = png_get_uint_16(data + 22);

  if (!checkSequenceNumber(data))
    return false;
  if (!frameWidth || !frameHeight)
    return false;
  if (xOffset + frameWidth > m_width || yOffset + frameHeight > m_height)
    return false;

  m_newFrame.frameRect = IntRect(xOffset, yOffset, frameWidth, frameHeight);

  if (delayDenominator)
    m_newFrame.duration = delayNumerator * 1000 / delayDenominator;
  else
    m_newFrame.duration = delayNumerator * 10;

  enum DisposeOperations : png_byte {
    kAPNG_DISPOSE_OP_NONE = 0,
    kAPNG_DISPOSE_OP_BACKGROUND = 1,
    kAPNG_DISPOSE_OP_PREVIOUS = 2,
  };
  const png_byte& disposeOp = data[24];
  switch (disposeOp) {
    case kAPNG_DISPOSE_OP_NONE:
      m_newFrame.disposalMethod = ImageFrame::DisposalMethod::DisposeKeep;
      break;
    case kAPNG_DISPOSE_OP_BACKGROUND:
      m_newFrame.disposalMethod =
          ImageFrame::DisposalMethod::DisposeOverwriteBgcolor;
      break;
    case kAPNG_DISPOSE_OP_PREVIOUS:
      m_newFrame.disposalMethod =
          ImageFrame::DisposalMethod::DisposeOverwritePrevious;
      break;
    default:
      return false;
  }

  enum BlendOperations : png_byte {
    kAPNG_BLEND_OP_SOURCE = 0,
    kAPNG_BLEND_OP_OVER = 1,
  };
  const png_byte& blendOp = data[25];
  switch (blendOp) {
    case kAPNG_BLEND_OP_SOURCE:
      m_newFrame.alphaBlend = ImageFrame::AlphaBlendSource::BlendAtopBgcolor;
      break;
    case kAPNG_BLEND_OP_OVER:
      m_newFrame.alphaBlend =
          ImageFrame::AlphaBlendSource::BlendAtopPreviousFrame;
      break;
    default:
      return false;
  }

  m_fctlNeedsDatChunk = true;
  return true;
}

}  // namespace blink
