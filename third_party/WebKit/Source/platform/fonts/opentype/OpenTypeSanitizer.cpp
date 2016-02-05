/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/opentype/OpenTypeSanitizer.h"

#include "hb.h"
#include "ots-memory-stream.h"
#include "platform/Histogram.h"
#include "platform/SharedBuffer.h"
#include "platform/TraceEvent.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"

#include <stdarg.h>

namespace blink {

static void recordDecodeSpeedHistogram(SharedBuffer* buffer, double decodeTime, size_t decodedSize)
{
    if (decodeTime <= 0)
        return;

    double kbPerSecond = decodedSize / (1000 * decodeTime);
    if (buffer->size() >= 4) {
        const char* data = buffer->data();
        if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == 'F') {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, woffHistogram, new CustomCountHistogram("WebFont.DecodeSpeed.WOFF", 1000, 300000, 50));
            woffHistogram.count(kbPerSecond);
            return;
        }

        if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == '2') {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, woff2Histogram, new CustomCountHistogram("WebFont.DecodeSpeed.WOFF2", 1000, 300000, 50));
            woff2Histogram.count(kbPerSecond);
            return;
        }
    }

    DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, sfntHistogram, new CustomCountHistogram("WebFont.DecodeSpeed.SFNT", 1000, 300000, 50));
    sfntHistogram.count(kbPerSecond);
}

PassRefPtr<SharedBuffer> OpenTypeSanitizer::sanitize()
{
    if (!m_buffer) {
        setErrorString("Empty Buffer");
        return nullptr;
    }

    // This is the largest web font size which we'll try to transcode.
    static const size_t maxWebFontSize = 30 * 1024 * 1024; // 30 MB
    if (m_buffer->size() > maxWebFontSize) {
        setErrorString("Web font size more than 30MB");
        return nullptr;
    }

    // A transcoded font is usually smaller than an original font.
    // However, it can be slightly bigger than the original one due to
    // name table replacement and/or padding for glyf table.
    //
    // With WOFF fonts, however, we'll be decompressing, so the result can be
    // much larger than the original.

    ots::ExpandingMemoryStream output(m_buffer->size(), maxWebFontSize);
    double start = currentTime();
    BlinkOTSContext otsContext;

    TRACE_EVENT_BEGIN0("blink", "DecodeFont");
    bool ok = otsContext.Process(&output, reinterpret_cast<const uint8_t*>(m_buffer->data()), m_buffer->size());
    TRACE_EVENT_END0("blink", "DecodeFont");

    if (!ok) {
        setErrorString(otsContext.getErrorString());
        return nullptr;
    }

    const size_t transcodeLen = output.Tell();
    recordDecodeSpeedHistogram(m_buffer, currentTime() - start, transcodeLen);
    return SharedBuffer::create(static_cast<unsigned char*>(output.get()), transcodeLen);
}

bool OpenTypeSanitizer::supportsFormat(const String& format)
{
    return equalIgnoringCase(format, "woff") || equalIgnoringCase(format, "woff2");
}

void BlinkOTSContext::Message(int level, const char *format, ...)
{
    va_list args;
    va_start(args, format);

#if COMPILER(MSVC)
    int result = _vscprintf(format, args);
#else
    char ch;
    int result = vsnprintf(&ch, 1, format, args);
#endif
    va_end(args);

    if (result <= 0) {
        m_errorString = String("OTS Error");
    } else {
        Vector<char, 256> buffer;
        unsigned len = result;
        buffer.grow(len + 1);

        va_start(args, format);
        vsnprintf(buffer.data(), buffer.size(), format, args);
        va_end(args);
        m_errorString = StringImpl::create(reinterpret_cast<const LChar*>(buffer.data()), len);
    }
}

#if !defined(HB_VERSION_ATLEAST)
#define HB_VERSION_ATLEAST(major, minor, micro) 0
#endif

ots::TableAction BlinkOTSContext::GetTableAction(uint32_t tag)
{
    const uint32_t cbdtTag = OTS_TAG('C', 'B', 'D', 'T');
    const uint32_t cblcTag = OTS_TAG('C', 'B', 'L', 'C');
    const uint32_t colrTag = OTS_TAG('C', 'O', 'L', 'R');
    const uint32_t cpalTag = OTS_TAG('C', 'P', 'A', 'L');
#if HB_VERSION_ATLEAST(1, 0, 0)
    const uint32_t gdefTag = OTS_TAG('G', 'D', 'E', 'F');
    const uint32_t gposTag = OTS_TAG('G', 'P', 'O', 'S');
    const uint32_t gsubTag = OTS_TAG('G', 'S', 'U', 'B');
#endif

    switch (tag) {
    // Google Color Emoji Tables
    case cbdtTag:
    case cblcTag:
    // Windows Color Emoji Tables
    case colrTag:
    case cpalTag:
#if HB_VERSION_ATLEAST(1, 0, 0)
    // Let HarfBuzz handle how to deal with broken tables.
    case gdefTag:
    case gposTag:
    case gsubTag:
#endif
        return ots::TABLE_ACTION_PASSTHRU;
    default:
        return ots::TABLE_ACTION_DEFAULT;
    }
}

} // namespace blink
