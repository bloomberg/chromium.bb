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

#include "config.h"

#include "core/html/track/vtt/VTTTokenizer.h"

#include "core/xml/parser/MarkupTokenizerInlines.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/unicode/CharacterNames.h"

namespace WebCore {

#define WEBVTT_BEGIN_STATE(stateName) BEGIN_STATE(VTTTokenizerState, stateName)
#define WEBVTT_ADVANCE_TO(stateName) ADVANCE_TO(VTTTokenizerState, stateName)

template<unsigned charactersCount>
ALWAYS_INLINE bool equalLiteral(const StringBuilder& s, const char (&characters)[charactersCount])
{
    return WTF::equal(s, reinterpret_cast<const LChar*>(characters), charactersCount - 1);
}

static void addNewClass(StringBuilder& classes, const StringBuilder& newClass)
{
    if (!classes.isEmpty())
        classes.append(' ');
    classes.append(newClass);
}

VTTTokenizer::VTTTokenizer(const String& input)
    : m_input(input)
    , m_inputStreamPreprocessor(this)
{
    reset();

    // Append a EOF marker and close the input "stream".
    ASSERT(!m_input.isClosed());
    m_input.append(SegmentedString(String(&kEndOfFileMarker, 1)));
    m_input.close();
}

void VTTTokenizer::reset()
{
    m_token = 0;
}

bool VTTTokenizer::nextToken(VTTToken& token)
{
    // If we have a token in progress, then we're supposed to be called back
    // with the same token so we can finish it.
    ASSERT(!m_token || m_token == &token);
    m_token = &token;

    if (m_input.isEmpty() || !m_inputStreamPreprocessor.peek(m_input))
        return haveBufferedCharacterToken();

    UChar cc = m_inputStreamPreprocessor.nextInputCharacter();
    if (cc == kEndOfFileMarker) {
        m_inputStreamPreprocessor.advance(m_input);
        return false;
    }

    StringBuilder buffer;
    StringBuilder result;
    StringBuilder classes;
    m_state = VTTTokenizerState::DataState;

    // The ADVANCE_TO helper macros expect this name ('source') on the input variable.
    SegmentedString& source = m_input;

    // 4.8.10.13.4 WebVTT cue text tokenizer
    switch (m_state) {
        WEBVTT_BEGIN_STATE(DataState) {
            if (cc == '&') {
                buffer.append(static_cast<LChar>(cc));
                WEBVTT_ADVANCE_TO(EscapeState);
            } else if (cc == '<') {
                if (result.isEmpty()) {
                    WEBVTT_ADVANCE_TO(TagState);
                } else {
                    // We don't want to advance input or perform a state transition - just return a (new) token.
                    // (On the next call to nextToken we will see '<' again, but take the other branch in this if instead.)
                    return emitToken(VTTToken::StringToken(result.toString()));
                }
            } else if (cc == kEndOfFileMarker) {
                return advanceAndEmitToken(source, VTTToken::StringToken(result.toString()));
            } else {
                result.append(cc);
                WEBVTT_ADVANCE_TO(DataState);
            }
        }
        END_STATE()

        WEBVTT_BEGIN_STATE(EscapeState) {
            if (cc == ';') {
                if (equalLiteral(buffer, "&amp")) {
                    result.append('&');
                } else if (equalLiteral(buffer, "&lt")) {
                    result.append('<');
                } else if (equalLiteral(buffer, "&gt")) {
                    result.append('>');
                } else if (equalLiteral(buffer, "&lrm")) {
                    result.append(leftToRightMark);
                } else if (equalLiteral(buffer, "&rlm")) {
                    result.append(rightToLeftMark);
                } else if (equalLiteral(buffer, "&nbsp")) {
                    result.append(noBreakSpace);
                } else {
                    buffer.append(static_cast<LChar>(cc));
                    result.append(buffer);
                }
                buffer.clear();
                WEBVTT_ADVANCE_TO(DataState);
            } else if (isASCIIAlphanumeric(cc)) {
                buffer.append(static_cast<LChar>(cc));
                WEBVTT_ADVANCE_TO(EscapeState);
            } else if (cc == kEndOfFileMarker) {
                result.append(buffer);
                return advanceAndEmitToken(source, VTTToken::StringToken(result.toString()));
            } else {
                if (!equalLiteral(buffer, "&"))
                    result.append(buffer);
                buffer.clear();
                WEBVTT_ADVANCE_TO(DataState);
            }
        }
        END_STATE()

        WEBVTT_BEGIN_STATE(TagState) {
            if (isTokenizerWhitespace(cc)) {
                ASSERT(result.isEmpty());
                WEBVTT_ADVANCE_TO(StartTagAnnotationState);
            } else if (cc == '.') {
                ASSERT(result.isEmpty());
                WEBVTT_ADVANCE_TO(StartTagClassState);
            } else if (cc == '/') {
                WEBVTT_ADVANCE_TO(EndTagState);
            } else if (WTF::isASCIIDigit(cc)) {
                result.append(cc);
                WEBVTT_ADVANCE_TO(TimestampTagState);
            } else if (cc == '>' || cc == kEndOfFileMarker) {
                ASSERT(result.isEmpty());
                return advanceAndEmitToken(source, VTTToken::StartTag(result.toString()));
            } else {
                result.append(cc);
                WEBVTT_ADVANCE_TO(StartTagState);
            }
        }
        END_STATE()

        WEBVTT_BEGIN_STATE(StartTagState) {
            if (isTokenizerWhitespace(cc)) {
                WEBVTT_ADVANCE_TO(StartTagAnnotationState);
            } else if (cc == '.') {
                WEBVTT_ADVANCE_TO(StartTagClassState);
            } else if (cc == '>' || cc == kEndOfFileMarker) {
                return advanceAndEmitToken(source, VTTToken::StartTag(result.toString()));
            } else {
                result.append(cc);
                WEBVTT_ADVANCE_TO(StartTagState);
            }
        }
        END_STATE()

        WEBVTT_BEGIN_STATE(StartTagClassState) {
            if (isTokenizerWhitespace(cc)) {
                addNewClass(classes, buffer);
                buffer.clear();
                WEBVTT_ADVANCE_TO(StartTagAnnotationState);
            } else if (cc == '.') {
                addNewClass(classes, buffer);
                buffer.clear();
                WEBVTT_ADVANCE_TO(StartTagClassState);
            } else if (cc == '>' || cc == kEndOfFileMarker) {
                addNewClass(classes, buffer);
                buffer.clear();
                return advanceAndEmitToken(source, VTTToken::StartTag(result.toString(), classes.toAtomicString()));
            } else {
                buffer.append(cc);
                WEBVTT_ADVANCE_TO(StartTagClassState);
            }
        }
        END_STATE()

        WEBVTT_BEGIN_STATE(StartTagAnnotationState) {
            if (cc == '>' || cc == kEndOfFileMarker) {
                return advanceAndEmitToken(source, VTTToken::StartTag(result.toString(), classes.toAtomicString(), buffer.toAtomicString()));
            }
            buffer.append(cc);
            WEBVTT_ADVANCE_TO(StartTagAnnotationState);
        }
        END_STATE()

        WEBVTT_BEGIN_STATE(EndTagState) {
            if (cc == '>' || cc == kEndOfFileMarker)
                return advanceAndEmitToken(source, VTTToken::EndTag(result.toString()));
            result.append(cc);
            WEBVTT_ADVANCE_TO(EndTagState);
        }
        END_STATE()

        WEBVTT_BEGIN_STATE(TimestampTagState) {
            if (cc == '>' || cc == kEndOfFileMarker)
                return advanceAndEmitToken(source, VTTToken::TimestampTag(result.toString()));
            result.append(cc);
            WEBVTT_ADVANCE_TO(TimestampTagState);
        }
        END_STATE()

    }

    ASSERT_NOT_REACHED();
    return false;
}

}

