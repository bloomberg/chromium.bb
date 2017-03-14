/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PNGImageReader_h
#define PNGImageReader_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntRect.h"
#include "platform/image-decoders/ImageFrame.h"
#include "png.h"
#include "wtf/Allocator.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"

#if !defined(PNG_LIBPNG_VER_MAJOR) || !defined(PNG_LIBPNG_VER_MINOR)
#error version error: compile against a versioned libpng.
#endif

#if PNG_LIBPNG_VER_MAJOR > 1 || \
    (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR >= 4)
#define JMPBUF(png_ptr) png_jmpbuf(png_ptr)
#else
#define JMPBUF(png_ptr) png_ptr->jmpbuf
#endif

namespace blink {

class FastSharedBufferReader;
class PNGImageDecoder;
class SegmentReader;

class PLATFORM_EXPORT PNGImageReader final {
  USING_FAST_MALLOC(PNGImageReader);
  WTF_MAKE_NONCOPYABLE(PNGImageReader);

 public:
  PNGImageReader(PNGImageDecoder*, size_t initialOffset);
  ~PNGImageReader();

  struct FrameInfo {
    // The offset where the frame data of this frame starts.
    size_t startOffset;
    // The number of bytes that contain frame data, starting at startOffset.
    size_t byteLength;
    size_t duration;
    IntRect frameRect;
    ImageFrame::DisposalMethod disposalMethod;
    ImageFrame::AlphaBlendSource alphaBlend;
  };

  enum class ParseQuery { Size, MetaData };

  bool parse(SegmentReader&, ParseQuery);

  // Returns false on a fatal error.
  bool decode(SegmentReader&, size_t);
  const FrameInfo& frameInfo(size_t) const;

  // Number of complete frames parsed so far; includes frame 0 even if partial.
  size_t frameCount() const { return m_frameInfo.size(); }

  bool parseCompleted() const { return m_parseCompleted; };

  bool frameIsReceivedAtIndex(size_t index) const {
    if (!index)
      return firstFrameFullyReceived();
    return index < frameCount();
  }

  void clearDecodeState(size_t);

  png_structp pngPtr() const { return m_png; }
  png_infop infoPtr() const { return m_info; }

  png_bytep interlaceBuffer() const { return m_interlaceBuffer.get(); }
  void createInterlaceBuffer(int size) {
    m_interlaceBuffer = wrapArrayUnique(new png_byte[size]);
  }
  void clearInterlaceBuffer() { m_interlaceBuffer.reset(); }

 private:
  png_structp m_png;
  png_infop m_info;
  png_uint_32 m_width;
  png_uint_32 m_height;

  PNGImageDecoder* m_decoder;

  // The offset in the stream where the PNG image starts.
  const size_t m_initialOffset;
  // How many bytes have been read during parsing.
  size_t m_readOffset;
  size_t m_progressiveDecodeOffset;
  size_t m_idatOffset;

  bool m_idatIsPartOfAnimation;
  // All IDAT chunks must precede the first fdAT chunk, and all fdAT chunks
  // should be separated from the IDAT chunks by an fcTL chunk. So this is true
  // until the first fcTL chunk after an IDAT chunk. After that, only fdAT
  // chunks are expected.
  bool m_expectIdats;
  bool m_isAnimated;
  bool m_parsedSignature;
  bool m_parsedIHDR;
  bool m_parseCompleted;
  uint32_t m_reportedFrameCount;
  uint32_t m_nextSequenceNumber;
  // True when an fcTL has been parsed but not its corresponding fdAT or IDAT
  // chunk. Consecutive fcTLs is an error.
  bool m_fctlNeedsDatChunk;
  bool m_ignoreAnimation;

  std::unique_ptr<png_byte[]> m_interlaceBuffer;

  // Value used for the byteLength of a FrameInfo struct to indicate that it is
  // the first frame and its byteLength is not yet known. 1 is a safe value
  // since the byteLength field of a frame is at least 12.
  static constexpr size_t kFirstFrameIndicator = 1;

  // Stores information about a frame until it can be pushed to |m_frameInfo|
  // once all the frame data has been read from the stream.
  FrameInfo m_newFrame;
  Vector<FrameInfo, 1> m_frameInfo;

  size_t processData(const FastSharedBufferReader&,
                     size_t offset,
                     size_t length);
  // Returns false on a fatal error.
  bool parseSize(const FastSharedBufferReader&);
  // Returns false on an error.
  bool parseFrameInfo(const png_byte* data);
  bool shouldDecodeWithNewPNG(size_t) const;
  void startFrameDecoding(const FastSharedBufferReader&, size_t);
  // Returns whether the frame was completely decoded.
  bool progressivelyDecodeFirstFrame(const FastSharedBufferReader&);
  void decodeFrame(const FastSharedBufferReader&, size_t);
  void processFdatChunkAsIdat(png_uint_32 fdatLength);
  // Returns false on a fatal error.
  bool checkSequenceNumber(const png_byte* position);
  bool firstFrameFullyReceived() const {
    return !m_frameInfo.isEmpty() &&
           m_frameInfo[0].byteLength != kFirstFrameIndicator;
  }
};

}  // namespace blink

#endif
