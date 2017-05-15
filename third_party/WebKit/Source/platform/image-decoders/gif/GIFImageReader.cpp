/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Saari <saari@netscape.com>
 *   Apple Computer
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
The Graphics Interchange Format(c) is the copyright property of CompuServe
Incorporated. Only CompuServe Incorporated is authorized to define, redefine,
enhance, alter, modify or change in any way the definition of the format.

CompuServe Incorporated hereby grants a limited, non-exclusive, royalty-free
license for the use of the Graphics Interchange Format(sm) in computer
software; computer software utilizing GIF(sm) must acknowledge ownership of the
Graphics Interchange Format and its Service Mark by CompuServe Incorporated, in
User and Technical Documentation. Computer software utilizing GIF, which is
distributed or may be distributed without User or Technical Documentation must
display to the screen or printer a message acknowledging ownership of the
Graphics Interchange Format and the Service Mark by CompuServe Incorporated; in
this case, the acknowledgement may be displayed in an opening screen or leading
banner, or a closing screen or trailing banner. A message such as the following
may be used:

    "The Graphics Interchange Format(c) is the Copyright property of
    CompuServe Incorporated. GIF(sm) is a Service Mark property of
    CompuServe Incorporated."

For further information, please contact :

    CompuServe Incorporated
    Graphics Technology Department
    5000 Arlington Center Boulevard
    Columbus, Ohio  43220
    U. S. A.

CompuServe Incorporated maintains a mailing list with all those individuals and
organizations who wish to receive copies of this document when it is corrected
or revised. This service is offered free of charge; please provide us with your
mailing address.
*/

#include "platform/image-decoders/gif/GIFImageReader.h"

#include <string.h>
#include "platform/wtf/PtrUtil.h"

using blink::GIFImageDecoder;

// GETN(n, s) requests at least 'n' bytes available from 'q', at start of state
// 's'.
//
// Note: the hold will never need to be bigger than 256 bytes, as each GIF block
// (except colormaps) can never be bigger than 256 bytes. Colormaps are directly
// copied in the resp. global_colormap or dynamically allocated local_colormap,
// so a fixed buffer in GIFImageReader is good enough. This buffer is only
// needed to copy left-over data from one GifWrite call to the next.
#define GETN(n, s)          \
  do {                      \
    m_bytesToConsume = (n); \
    m_state = (s);          \
  } while (0)

// Get a 16-bit value stored in little-endian format.
#define GETINT16(p) ((p)[1] << 8 | (p)[0])

// Send the data to the display front-end.
bool GIFLZWContext::outputRow(GIFRow::const_iterator rowBegin) {
  int drowStart = irow;
  int drowEnd = irow;

  // Haeberli-inspired hack for interlaced GIFs: Replicate lines while
  // displaying to diminish the "venetian-blind" effect as the image is
  // loaded. Adjust pixel vertical positions to avoid the appearance of the
  // image crawling up the screen as successive passes are drawn.
  if (m_frameContext->progressiveDisplay() && m_frameContext->interlaced() &&
      ipass < 4) {
    unsigned rowDup = 0;
    unsigned rowShift = 0;

    switch (ipass) {
      case 1:
        rowDup = 7;
        rowShift = 3;
        break;
      case 2:
        rowDup = 3;
        rowShift = 1;
        break;
      case 3:
        rowDup = 1;
        rowShift = 0;
        break;
      default:
        break;
    }

    drowStart -= rowShift;
    drowEnd = drowStart + rowDup;

    // Extend if bottom edge isn't covered because of the shift upward.
    if (((m_frameContext->height() - 1) - drowEnd) <= rowShift)
      drowEnd = m_frameContext->height() - 1;

    // Clamp first and last rows to upper and lower edge of image.
    if (drowStart < 0)
      drowStart = 0;

    if ((unsigned)drowEnd >= m_frameContext->height())
      drowEnd = m_frameContext->height() - 1;
  }

  // Protect against too much image data.
  if ((unsigned)drowStart >= m_frameContext->height())
    return true;

  // CALLBACK: Let the client know we have decoded a row.
  if (!m_client->HaveDecodedRow(m_frameContext->frameId(), rowBegin,
                                m_frameContext->width(), drowStart,
                                drowEnd - drowStart + 1,
                                m_frameContext->progressiveDisplay() &&
                                    m_frameContext->interlaced() && ipass > 1))
    return false;

  if (!m_frameContext->interlaced())
    irow++;
  else {
    do {
      switch (ipass) {
        case 1:
          irow += 8;
          if (irow >= m_frameContext->height()) {
            ipass++;
            irow = 4;
          }
          break;

        case 2:
          irow += 8;
          if (irow >= m_frameContext->height()) {
            ipass++;
            irow = 2;
          }
          break;

        case 3:
          irow += 4;
          if (irow >= m_frameContext->height()) {
            ipass++;
            irow = 1;
          }
          break;

        case 4:
          irow += 2;
          if (irow >= m_frameContext->height()) {
            ipass++;
            irow = 0;
          }
          break;

        default:
          break;
      }
    } while (irow > (m_frameContext->height() - 1));
  }
  return true;
}

// Performs Lempel-Ziv-Welch decoding. Returns whether decoding was successful.
// If successful, the block will have been completely consumed and/or
// rowsRemaining will be 0.
bool GIFLZWContext::doLZW(const unsigned char* block, size_t bytesInBlock) {
  const size_t width = m_frameContext->width();

  if (rowIter == rowBuffer.end())
    return true;

  for (const unsigned char* ch = block; bytesInBlock-- > 0; ch++) {
    // Feed the next byte into the decoder's 32-bit input buffer.
    datum += ((int)*ch) << bits;
    bits += 8;

    // Check for underflow of decoder's 32-bit input buffer.
    while (bits >= codesize) {
      // Get the leading variable-length symbol from the data stream.
      int code = datum & codemask;
      datum >>= codesize;
      bits -= codesize;

      // Reset the dictionary to its original state, if requested.
      if (code == clearCode) {
        codesize = m_frameContext->dataSize() + 1;
        codemask = (1 << codesize) - 1;
        avail = clearCode + 2;
        oldcode = -1;
        continue;
      }

      // Check for explicit end-of-stream code.
      if (code == (clearCode + 1)) {
        // end-of-stream should only appear after all image data.
        if (!rowsRemaining)
          return true;
        return false;
      }

      const int tempCode = code;
      unsigned short codeLength = 0;
      if (code < avail) {
        // This is a pre-existing code, so we already know what it
        // encodes.
        codeLength = suffixLength[code];
        rowIter += codeLength;
      } else if (code == avail && oldcode != -1) {
        // This is a new code just being added to the dictionary.
        // It must encode the contents of the previous code, plus
        // the first character of the previous code again.
        codeLength = suffixLength[oldcode] + 1;
        rowIter += codeLength;
        *--rowIter = firstchar;
        code = oldcode;
      } else {
        // This is an invalid code. The dictionary is just initialized
        // and the code is incomplete. We don't know how to handle
        // this case.
        return false;
      }

      while (code >= clearCode) {
        *--rowIter = suffix[code];
        code = prefix[code];
      }

      *--rowIter = firstchar = suffix[code];

      // Define a new codeword in the dictionary as long as we've read
      // more than one value from the stream.
      if (avail < MAX_DICTIONARY_ENTRIES && oldcode != -1) {
        prefix[avail] = oldcode;
        suffix[avail] = firstchar;
        suffixLength[avail] = suffixLength[oldcode] + 1;
        ++avail;

        // If we've used up all the codewords of a given length
        // increase the length of codewords by one bit, but don't
        // exceed the specified maximum codeword size.
        if (!(avail & codemask) && avail < MAX_DICTIONARY_ENTRIES) {
          ++codesize;
          codemask += avail;
        }
      }
      oldcode = tempCode;
      rowIter += codeLength;

      // Output as many rows as possible.
      GIFRow::iterator rowBegin = rowBuffer.begin();
      for (; rowBegin + width <= rowIter; rowBegin += width) {
        if (!outputRow(rowBegin))
          return false;
        rowsRemaining--;
        if (!rowsRemaining)
          return true;
      }

      if (rowBegin != rowBuffer.begin()) {
        // Move the remaining bytes to the beginning of the buffer.
        const size_t bytesToCopy = rowIter - rowBegin;
        memcpy(rowBuffer.begin(), rowBegin, bytesToCopy);
        rowIter = rowBuffer.begin() + bytesToCopy;
      }
    }
  }
  return true;
}

void GIFColorMap::buildTable(blink::FastSharedBufferReader* reader) {
  if (!m_isDefined || !m_table.IsEmpty())
    return;

  CHECK_LE(m_position + m_colors * BYTES_PER_COLORMAP_ENTRY, reader->size());
  DCHECK_LE(m_colors, MAX_COLORS);
  char buffer[MAX_COLORS * BYTES_PER_COLORMAP_ENTRY];
  const unsigned char* srcColormap =
      reinterpret_cast<const unsigned char*>(reader->GetConsecutiveData(
          m_position, m_colors * BYTES_PER_COLORMAP_ENTRY, buffer));
  m_table.resize(m_colors);
  for (Table::iterator iter = m_table.begin(); iter != m_table.end(); ++iter) {
    *iter = SkPackARGB32NoCheck(255, srcColormap[0], srcColormap[1],
                                srcColormap[2]);
    srcColormap += BYTES_PER_COLORMAP_ENTRY;
  }
}

// Decodes this frame. |frameDecoded| will be set to true if the entire frame is
// decoded. Returns true if decoding progressed further than before without
// error, or there is insufficient new data to decode further. Otherwise, a
// decoding error occurred; returns false in this case.
bool GIFFrameContext::decode(blink::FastSharedBufferReader* reader,
                             blink::GIFImageDecoder* client,
                             bool* frameDecoded) {
  m_localColorMap.buildTable(reader);

  *frameDecoded = false;
  if (!m_lzwContext) {
    // Wait for more data to properly initialize GIFLZWContext.
    if (!isDataSizeDefined() || !isHeaderDefined())
      return true;

    m_lzwContext = WTF::MakeUnique<GIFLZWContext>(client, this);
    if (!m_lzwContext->prepareToDecode()) {
      m_lzwContext.reset();
      return false;
    }

    m_currentLzwBlock = 0;
  }

  // Some bad GIFs have extra blocks beyond the last row, which we don't want to
  // decode.
  while (m_currentLzwBlock < m_lzwBlocks.size() &&
         m_lzwContext->hasRemainingRows()) {
    size_t blockPosition = m_lzwBlocks[m_currentLzwBlock].blockPosition;
    size_t blockSize = m_lzwBlocks[m_currentLzwBlock].blockSize;
    if (blockPosition + blockSize > reader->size())
      return false;

    while (blockSize) {
      const char* segment = 0;
      size_t segmentLength = reader->GetSomeData(segment, blockPosition);
      size_t decodeSize = std::min(segmentLength, blockSize);
      if (!m_lzwContext->doLZW(reinterpret_cast<const unsigned char*>(segment),
                               decodeSize))
        return false;
      blockPosition += decodeSize;
      blockSize -= decodeSize;
    }
    ++m_currentLzwBlock;
  }

  // If this frame is data complete then the previous loop must have completely
  // decoded all LZW blocks.
  // There will be no more decoding for this frame so it's time to cleanup.
  if (isComplete()) {
    *frameDecoded = true;
    m_lzwContext.reset();
  }
  return true;
}

// Decodes a frame using GIFFrameContext:decode(). Returns true if decoding has
// progressed, or false if an error has occurred.
bool GIFImageReader::decode(size_t frameIndex) {
  blink::FastSharedBufferReader reader(m_data);
  m_globalColorMap.buildTable(&reader);

  bool frameDecoded = false;
  GIFFrameContext* currentFrame = m_frames[frameIndex].get();

  return currentFrame->decode(&reader, m_client, &frameDecoded) &&
         (!frameDecoded || m_client->FrameComplete(frameIndex));
}

bool GIFImageReader::parse(GIFImageDecoder::GIFParseQuery query) {
  if (m_bytesRead >= m_data->size()) {
    // This data has already been parsed. For example, in deferred
    // decoding, a DecodingImageGenerator with more data may have already
    // used this same ImageDecoder to decode. This can happen if two
    // SkImages created by a DeferredImageDecoder are drawn/prerolled
    // out of order (with respect to how much data they had at creation
    // time).
    return !m_client->Failed();
  }

  return parseData(m_bytesRead, m_data->size() - m_bytesRead, query);
}

// Parse incoming GIF data stream into internal data structures.
// Return true if parsing has progressed or there is not enough data.
// Return false if a fatal error is encountered.
bool GIFImageReader::parseData(size_t dataPosition,
                               size_t len,
                               GIFImageDecoder::GIFParseQuery query) {
  if (!len) {
    // No new data has come in since the last call, just ignore this call.
    return true;
  }

  if (len < m_bytesToConsume)
    return true;

  blink::FastSharedBufferReader reader(m_data);

  // A read buffer of 16 bytes is enough to accomodate all possible reads for
  // parsing.
  char readBuffer[16];

  // Read as many components from |m_data| as possible. At the beginning of each
  // iteration, |dataPosition| is advanced by m_bytesToConsume to point to the
  // next component. |len| is decremented accordingly.
  while (len >= m_bytesToConsume) {
    const size_t currentComponentPosition = dataPosition;

    // Mark the current component as consumed. Note that currentComponent will
    // remain pointed at this component until the next loop iteration.
    dataPosition += m_bytesToConsume;
    len -= m_bytesToConsume;

    switch (m_state) {
      case GIFLZW:
        DCHECK(!m_frames.IsEmpty());
        // m_bytesToConsume is the current component size because it hasn't been
        // updated.
        m_frames.back()->addLzwBlock(currentComponentPosition,
                                     m_bytesToConsume);
        GETN(1, GIFSubBlock);
        break;

      case GIFLZWStart: {
        DCHECK(!m_frames.IsEmpty());
        m_frames.back()->setDataSize(static_cast<unsigned char>(
            reader.GetOneByte(currentComponentPosition)));
        GETN(1, GIFSubBlock);
        break;
      }

      case GIFType: {
        const char* currentComponent =
            reader.GetConsecutiveData(currentComponentPosition, 6, readBuffer);

        // All GIF files begin with "GIF87a" or "GIF89a".
        if (!memcmp(currentComponent, "GIF89a", 6))
          m_version = 89;
        else if (!memcmp(currentComponent, "GIF87a", 6))
          m_version = 87;
        else
          return false;
        GETN(7, GIFGlobalHeader);
        break;
      }

      case GIFGlobalHeader: {
        const unsigned char* currentComponent =
            reinterpret_cast<const unsigned char*>(reader.GetConsecutiveData(
                currentComponentPosition, 5, readBuffer));

        // This is the height and width of the "screen" or frame into which
        // images are rendered. The individual images can be smaller than
        // the screen size and located with an origin anywhere within the
        // screen.
        // Note that we don't inform the client of the size yet, as it might
        // change after we read the first frame's image header.
        m_screenWidth = GETINT16(currentComponent);
        m_screenHeight = GETINT16(currentComponent + 2);

        const size_t globalColorMapColors = 2 << (currentComponent[4] & 0x07);

        if ((currentComponent[4] & 0x80) &&
            globalColorMapColors > 0) { /* global map */
          m_globalColorMap.setTablePositionAndSize(dataPosition,
                                                   globalColorMapColors);
          GETN(BYTES_PER_COLORMAP_ENTRY * globalColorMapColors,
               GIFGlobalColormap);
          break;
        }

        GETN(1, GIFImageStart);
        break;
      }

      case GIFGlobalColormap: {
        m_globalColorMap.setDefined();
        GETN(1, GIFImageStart);
        break;
      }

      case GIFImageStart: {
        const char currentComponent =
            reader.GetOneByte(currentComponentPosition);

        if (currentComponent == '!') {  // extension.
          GETN(2, GIFExtension);
          break;
        }

        if (currentComponent == ',') {  // image separator.
          GETN(9, GIFImageHeader);
          break;
        }

        // If we get anything other than ',' (image separator), '!'
        // (extension), or ';' (trailer), there is extraneous data
        // between blocks. The GIF87a spec tells us to keep reading
        // until we find an image separator, but GIF89a says such
        // a file is corrupt. We follow Mozilla's implementation and
        // proceed as if the file were correctly terminated, so the
        // GIF will display.
        GETN(0, GIFDone);
        break;
      }

      case GIFExtension: {
        const unsigned char* currentComponent =
            reinterpret_cast<const unsigned char*>(reader.GetConsecutiveData(
                currentComponentPosition, 2, readBuffer));

        size_t bytesInBlock = currentComponent[1];
        GIFState exceptionState = GIFSkipBlock;

        switch (*currentComponent) {
          case 0xf9:
            exceptionState = GIFControlExtension;
            // The GIF spec mandates that the GIFControlExtension header block
            // length is 4 bytes, and the parser for this block reads 4 bytes,
            // so we must enforce that the buffer contains at least this many
            // bytes. If the GIF specifies a different length, we allow that, so
            // long as it's larger; the additional data will simply be ignored.
            bytesInBlock = std::max(bytesInBlock, static_cast<size_t>(4));
            break;

          // The GIF spec also specifies the lengths of the following two
          // extensions' headers (as 12 and 11 bytes, respectively). Because we
          // ignore the plain text extension entirely and sanity-check the
          // actual length of the application extension header before reading
          // it, we allow GIFs to deviate from these values in either direction.
          // This is important for real-world compatibility, as GIFs in the wild
          // exist with application extension headers that are both shorter and
          // longer than 11 bytes.
          case 0x01:
            // ignoring plain text extension
            break;

          case 0xff:
            exceptionState = GIFApplicationExtension;
            break;

          case 0xfe:
            exceptionState = GIFConsumeComment;
            break;
        }

        if (bytesInBlock)
          GETN(bytesInBlock, exceptionState);
        else
          GETN(1, GIFImageStart);
        break;
      }

      case GIFConsumeBlock: {
        const unsigned char currentComponent = static_cast<unsigned char>(
            reader.GetOneByte(currentComponentPosition));
        if (!currentComponent)
          GETN(1, GIFImageStart);
        else
          GETN(currentComponent, GIFSkipBlock);
        break;
      }

      case GIFSkipBlock: {
        GETN(1, GIFConsumeBlock);
        break;
      }

      case GIFControlExtension: {
        const unsigned char* currentComponent =
            reinterpret_cast<const unsigned char*>(reader.GetConsecutiveData(
                currentComponentPosition, 4, readBuffer));

        addFrameIfNecessary();
        GIFFrameContext* currentFrame = m_frames.back().get();
        if (*currentComponent & 0x1)
          currentFrame->setTransparentPixel(currentComponent[3]);

        // We ignore the "user input" bit.

        // NOTE: This relies on the values in the FrameDisposalMethod enum
        // matching those in the GIF spec!
        int disposalMethod = ((*currentComponent) >> 2) & 0x7;
        if (disposalMethod < 4) {
          currentFrame->setDisposalMethod(
              static_cast<blink::ImageFrame::DisposalMethod>(disposalMethod));
        } else if (disposalMethod == 4) {
          // Some specs say that disposal method 3 is "overwrite previous",
          // others that setting the third bit of the field (i.e. method 4) is.
          // We map both to the same value.
          currentFrame->setDisposalMethod(
              blink::ImageFrame::kDisposeOverwritePrevious);
        }
        currentFrame->setDelayTime(GETINT16(currentComponent + 1) * 10);
        GETN(1, GIFConsumeBlock);
        break;
      }

      case GIFCommentExtension: {
        const unsigned char currentComponent = static_cast<unsigned char>(
            reader.GetOneByte(currentComponentPosition));
        if (currentComponent)
          GETN(currentComponent, GIFConsumeComment);
        else
          GETN(1, GIFImageStart);
        break;
      }

      case GIFConsumeComment: {
        GETN(1, GIFCommentExtension);
        break;
      }

      case GIFApplicationExtension: {
        // Check for netscape application extension.
        if (m_bytesToConsume == 11) {
          const unsigned char* currentComponent =
              reinterpret_cast<const unsigned char*>(reader.GetConsecutiveData(
                  currentComponentPosition, 11, readBuffer));

          if (!memcmp(currentComponent, "NETSCAPE2.0", 11) ||
              !memcmp(currentComponent, "ANIMEXTS1.0", 11))
            GETN(1, GIFNetscapeExtensionBlock);
        }

        if (m_state != GIFNetscapeExtensionBlock)
          GETN(1, GIFConsumeBlock);
        break;
      }

      // Netscape-specific GIF extension: animation looping.
      case GIFNetscapeExtensionBlock: {
        const int currentComponent = static_cast<unsigned char>(
            reader.GetOneByte(currentComponentPosition));
        // GIFConsumeNetscapeExtension always reads 3 bytes from the stream; we
        // should at least wait for this amount.
        if (currentComponent)
          GETN(std::max(3, currentComponent), GIFConsumeNetscapeExtension);
        else
          GETN(1, GIFImageStart);
        break;
      }

      // Parse netscape-specific application extensions
      case GIFConsumeNetscapeExtension: {
        const unsigned char* currentComponent =
            reinterpret_cast<const unsigned char*>(reader.GetConsecutiveData(
                currentComponentPosition, 3, readBuffer));

        int netscapeExtension = currentComponent[0] & 7;

        // Loop entire animation specified # of times. Only read the loop count
        // during the first iteration.
        if (netscapeExtension == 1) {
          m_loopCount = GETINT16(currentComponent + 1);

          // Zero loop count is infinite animation loop request.
          if (!m_loopCount)
            m_loopCount = blink::kAnimationLoopInfinite;

          GETN(1, GIFNetscapeExtensionBlock);
        } else if (netscapeExtension == 2) {
          // Wait for specified # of bytes to enter buffer.

          // Don't do this, this extension doesn't exist (isn't used at all)
          // and doesn't do anything, as our streaming/buffering takes care of
          // it all. See http://semmix.pl/color/exgraf/eeg24.htm .
          GETN(1, GIFNetscapeExtensionBlock);
        } else {
          // 0,3-7 are yet to be defined netscape extension codes
          return false;
        }
        break;
      }

      case GIFImageHeader: {
        unsigned height, width, xOffset, yOffset;
        const unsigned char* currentComponent =
            reinterpret_cast<const unsigned char*>(reader.GetConsecutiveData(
                currentComponentPosition, 9, readBuffer));

        /* Get image offsets, with respect to the screen origin */
        xOffset = GETINT16(currentComponent);
        yOffset = GETINT16(currentComponent + 2);

        /* Get image width and height. */
        width = GETINT16(currentComponent + 4);
        height = GETINT16(currentComponent + 6);

        // Some GIF files have frames that don't fit in the specified
        // overall image size. For the first frame, we can simply enlarge
        // the image size to allow the frame to be visible.  We can't do
        // this on subsequent frames because the rest of the decoding
        // infrastructure assumes the image size won't change as we
        // continue decoding, so any subsequent frames that are even
        // larger will be cropped.
        // Luckily, handling just the first frame is sufficient to deal
        // with most cases, e.g. ones where the image size is erroneously
        // set to zero, since usually the first frame completely fills
        // the image.
        if (currentFrameIsFirstFrame()) {
          m_screenHeight = std::max(m_screenHeight, yOffset + height);
          m_screenWidth = std::max(m_screenWidth, xOffset + width);
        }

        // Inform the client of the final size.
        if (!m_sentSizeToClient && m_client &&
            !m_client->SetSize(m_screenWidth, m_screenHeight))
          return false;
        m_sentSizeToClient = true;

        if (query == GIFImageDecoder::kGIFSizeQuery) {
          // The decoder needs to stop. Hand back the number of bytes we
          // consumed from the buffer minus 9 (the amount we consumed to read
          // the header).
          setRemainingBytes(len + 9);
          GETN(9, GIFImageHeader);
          return true;
        }

        addFrameIfNecessary();
        GIFFrameContext* currentFrame = m_frames.back().get();

        currentFrame->setHeaderDefined();

        // Work around more broken GIF files that have zero image width or
        // height.
        if (!height || !width) {
          height = m_screenHeight;
          width = m_screenWidth;
          if (!height || !width)
            return false;
        }
        currentFrame->setRect(xOffset, yOffset, width, height);
        currentFrame->setInterlaced(currentComponent[8] & 0x40);

        // Overlaying interlaced, transparent GIFs over
        // existing image data using the Haeberli display hack
        // requires saving the underlying image in order to
        // avoid jaggies at the transparency edges. We are
        // unprepared to deal with that, so don't display such
        // images progressively. Which means only the first
        // frame can be progressively displayed.
        // FIXME: It is possible that a non-transparent frame
        // can be interlaced and progressively displayed.
        currentFrame->setProgressiveDisplay(currentFrameIsFirstFrame());

        const bool isLocalColormapDefined = currentComponent[8] & 0x80;
        if (isLocalColormapDefined) {
          // The three low-order bits of currentComponent[8] specify the bits
          // per pixel.
          const size_t numColors = 2 << (currentComponent[8] & 0x7);
          currentFrame->localColorMap().setTablePositionAndSize(dataPosition,
                                                                numColors);
          GETN(BYTES_PER_COLORMAP_ENTRY * numColors, GIFImageColormap);
          break;
        }

        GETN(1, GIFLZWStart);
        break;
      }

      case GIFImageColormap: {
        DCHECK(!m_frames.IsEmpty());
        m_frames.back()->localColorMap().setDefined();
        GETN(1, GIFLZWStart);
        break;
      }

      case GIFSubBlock: {
        const size_t bytesInBlock = static_cast<unsigned char>(
            reader.GetOneByte(currentComponentPosition));
        if (bytesInBlock)
          GETN(bytesInBlock, GIFLZW);
        else {
          // Finished parsing one frame; Process next frame.
          DCHECK(!m_frames.IsEmpty());
          // Note that some broken GIF files do not have enough LZW blocks to
          // fully decode all rows; we treat this case as "frame complete".
          m_frames.back()->setComplete();
          GETN(1, GIFImageStart);
        }
        break;
      }

      case GIFDone: {
        m_parseCompleted = true;
        return true;
      }

      default:
        // We shouldn't ever get here.
        return false;
        break;
    }
  }

  setRemainingBytes(len);
  return true;
}

void GIFImageReader::setRemainingBytes(size_t remainingBytes) {
  DCHECK_LE(remainingBytes, m_data->size());
  m_bytesRead = m_data->size() - remainingBytes;
}

void GIFImageReader::addFrameIfNecessary() {
  if (m_frames.IsEmpty() || m_frames.back()->isComplete())
    m_frames.push_back(WTF::WrapUnique(new GIFFrameContext(m_frames.size())));
}

// FIXME: Move this method to close to doLZW().
bool GIFLZWContext::prepareToDecode() {
  DCHECK(m_frameContext->isDataSizeDefined());
  DCHECK(m_frameContext->isHeaderDefined());

  // Since we use a codesize of 1 more than the datasize, we need to ensure
  // that our datasize is strictly less than the MAX_DICTIONARY_ENTRY_BITS.
  if (m_frameContext->dataSize() >= MAX_DICTIONARY_ENTRY_BITS)
    return false;
  clearCode = 1 << m_frameContext->dataSize();
  avail = clearCode + 2;
  oldcode = -1;
  codesize = m_frameContext->dataSize() + 1;
  codemask = (1 << codesize) - 1;
  datum = bits = 0;
  ipass = m_frameContext->interlaced() ? 1 : 0;
  irow = 0;

  // We want to know the longest sequence encodable by a dictionary with
  // MAX_DICTIONARY_ENTRIES entries. If we ignore the need to encode the base
  // values themselves at the beginning of the dictionary, as well as the need
  // for a clear code or a termination code, we could use every entry to
  // encode a series of multiple values. If the input value stream looked
  // like "AAAAA..." (a long string of just one value), the first dictionary
  // entry would encode AA, the next AAA, the next AAAA, and so forth. Thus
  // the longest sequence would be MAX_DICTIONARY_ENTRIES + 1 values.
  //
  // However, we have to account for reserved entries. The first |datasize|
  // bits are reserved for the base values, and the next two entries are
  // reserved for the clear code and termination code. In theory a GIF can
  // set the datasize to 0, meaning we have just two reserved entries, making
  // the longest sequence (MAX_DICTIONARY_ENTIRES + 1) - 2 values long. Since
  // each value is a byte, this is also the number of bytes in the longest
  // encodable sequence.
  const size_t maxBytes = MAX_DICTIONARY_ENTRIES - 1;

  // Now allocate the output buffer. We decode directly into this buffer
  // until we have at least one row worth of data, then call outputRow().
  // This means worst case we may have (row width - 1) bytes in the buffer
  // and then decode a sequence |maxBytes| long to append.
  rowBuffer.resize(m_frameContext->width() - 1 + maxBytes);
  rowIter = rowBuffer.begin();
  rowsRemaining = m_frameContext->height();

  // Clearing the whole suffix table lets us be more tolerant of bad data.
  for (int i = 0; i < clearCode; ++i) {
    suffix[i] = i;
    suffixLength[i] = 1;
  }
  return true;
}
