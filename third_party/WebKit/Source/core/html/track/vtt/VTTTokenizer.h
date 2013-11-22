/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#ifndef VTTTokenizer_h
#define VTTTokenizer_h

#include "core/html/parser/InputStreamPreprocessor.h"
#include "core/html/track/vtt/VTTToken.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

class VTTTokenizerState {
public:
    enum State {
        DataState,
        EscapeState,
        TagState,
        StartTagState,
        StartTagClassState,
        StartTagAnnotationState,
        EndTagState,
        TimestampTagState,
    };
};

class VTTTokenizer {
    WTF_MAKE_NONCOPYABLE(VTTTokenizer);
public:
    explicit VTTTokenizer(const String& input);

    typedef VTTTokenizerState State;

    void reset();

    bool nextToken(VTTToken&);

    inline bool haveBufferedCharacterToken() { return false; }

    inline bool advanceAndEmitToken(SegmentedString& source, const VTTToken& token)
    {
        source.advanceAndUpdateLineNumber();
        return emitToken(token);
    }

    inline bool emitToken(const VTTToken& token)
    {
        *m_token = token;
        return true;
    }

    bool shouldSkipNullCharacters() const { return true; }

private:
    // m_token is owned by the caller. If nextToken is not on the stack,
    // this member might be pointing to unallocated memory.
    VTTToken* m_token;

    // This member does not really need to be an instance member - it's only used in nextToken.
    // The reason it's stored here is because of the use of the ADVANCE_TO state helpers.
    VTTTokenizerState::State m_state;

    SegmentedString m_input;

    // ://www.whatwg.org/specs/web-apps/current-work/#preprocessing-the-input-stream
    InputStreamPreprocessor<VTTTokenizer> m_inputStreamPreprocessor;
};

}

#endif
