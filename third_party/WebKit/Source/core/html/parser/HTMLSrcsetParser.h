/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef HTMLSrcsetParser_h
#define HTMLSrcsetParser_h

#include "wtf/text/WTFString.h"

namespace WebCore {

class ImageCandidate {
public:
    ImageCandidate()
        : m_scaleFactor(1.0)
    {
    }

    ImageCandidate(const String& source, unsigned start, unsigned length, float scaleFactor)
        : m_string(source.createView(start, length))
        , m_scaleFactor(scaleFactor)
    {
    }

    String toString() const
    {
        return m_string.toString();
    }

    inline float scaleFactor() const
    {
        return m_scaleFactor;
    }

    inline bool isEmpty() const
    {
        return m_string.isEmpty();
    }

private:
    StringView m_string;
    float m_scaleFactor;
};

ImageCandidate bestFitSourceForSrcsetAttribute(float deviceScaleFactor, const String& srcsetAttribute);

String bestFitSourceForImageAttributes(float deviceScaleFactor, const String& srcAttribute, const String& srcsetAttribute);

String bestFitSourceForImageAttributes(float deviceScaleFactor, const String& srcAttribute, ImageCandidate& srcsetImageCandidate);

}

#endif
