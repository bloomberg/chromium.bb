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

#ifndef OpenTypeSanitizer_h
#define OpenTypeSanitizer_h

#include "opentype-sanitiser.h"
#include "wtf/Forward.h"

namespace blink {

class SharedBuffer;

class OpenTypeSanitizer {
public:
    explicit OpenTypeSanitizer(SharedBuffer* buffer)
        : m_buffer(buffer)
    {
    }

    PassRefPtr<SharedBuffer> sanitize();

    static bool supportsFormat(const String&);

private:
    SharedBuffer* const m_buffer;
};

class BlinkOTSContext: public ots::OTSContext {
public:
        // TODO: Implement "Message" to support user friendly messages
        virtual ots::TableAction GetTableAction(uint32_t tag)
        {
#define TABLE_TAG(c1, c2, c3, c4) ((uint32_t)((((uint8_t)(c1)) << 24) | (((uint8_t)(c2)) << 16) | (((uint8_t)(c3)) << 8) | ((uint8_t)(c4))))

            const uint32_t cbdtTag = TABLE_TAG('C', 'B', 'D', 'T');
            const uint32_t cblcTag = TABLE_TAG('C', 'B', 'L', 'C');
            const uint32_t colrTag = TABLE_TAG('C', 'O', 'L', 'R');
            const uint32_t cpalTag = TABLE_TAG('C', 'P', 'A', 'L');

            switch (tag) {
            // Google Color Emoji Tables
            case cbdtTag:
            case cblcTag:
            // Windows Color Emoji Tables
            case colrTag:
            case cpalTag:
                return ots::TABLE_ACTION_PASSTHRU;
            default:
                return ots::TABLE_ACTION_DEFAULT;
            }
#undef TABLE_TAG
        }
};

} // namespace blink

#endif // OpenTypeSanitizer_h
