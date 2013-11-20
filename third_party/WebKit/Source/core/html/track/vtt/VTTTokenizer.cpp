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
#include "wtf/unicode/CharacterNames.h"

namespace WebCore {

#define WEBVTT_BEGIN_STATE(stateName) BEGIN_STATE(VTTTokenizerState, stateName)
#define WEBVTT_ADVANCE_TO(stateName) ADVANCE_TO(VTTTokenizerState, stateName)

template<unsigned charactersCount>
ALWAYS_INLINE bool equalLiteral(const StringBuilder& s, const char (&characters)[charactersCount])
{
    return WTF::equal(s, reinterpret_cast<const LChar*>(characters), charactersCount - 1);
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
    m_buffer.clear();
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

    m_state = VTTTokenizerState::DataState;

    // The ADVANCE_TO helper macros expect this name ('source') on the input variable.
    SegmentedString& source = m_input;

    // 4.8.10.13.4 WebVTT cue text tokenizer
    switch (m_state) {
        WEBVTT_BEGIN_STATE(DataState) {
            if (cc == '&') {
                m_buffer.append(static_cast<LChar>(cc));
                WEBVTT_ADVANCE_TO(EscapeState);
            } else if (cc == '<') {
                if (m_token->characters().isEmpty()) {
                    WEBVTT_ADVANCE_TO(TagState);
                } else {
                    // We don't want to advance input or perform a state transition - just return a (new) token.
                    // (On the next call to nextToken we will see '<' again, but take the other branch in this if instead.)
                    return emitToken(VTTTokenTypes::Character);
                }
            } else if (cc == kEndOfFileMarker) {
                return advanceAndEmitToken(source, VTTTokenTypes::Character);
            } else {
                bufferCharacter(cc);
                WEBVTT_ADVANCE_TO(DataState);
            }
        }
        END_STATE()

        WEBVTT_BEGIN_STATE(EscapeState) {
            if (cc == ';') {
                if (equalLiteral(m_buffer, "&amp")) {
                    bufferCharacter('&');
                } else if (equalLiteral(m_buffer, "&lt")) {
                    bufferCharacter('<');
                } else if (equalLiteral(m_buffer, "&gt")) {
                    bufferCharacter('>');
                } else if (equalLiteral(m_buffer, "&lrm")) {
                    bufferCharacter(leftToRightMark);
                } else if (equalLiteral(m_buffer, "&rlm")) {
                    bufferCharacter(rightToLeftMark);
                } else if (equalLiteral(m_buffer, "&nbsp")) {
                    bufferCharacter(noBreakSpace);
                } else {
                    m_buffer.append(static_cast<LChar>(cc));
                    m_token->appendToCharacter(m_buffer);
                }
                m_buffer.clear();
                WEBVTT_ADVANCE_TO(DataState);
            } else if (isASCIIAlphanumeric(cc)) {
                m_buffer.append(static_cast<LChar>(cc));
                WEBVTT_ADVANCE_TO(EscapeState);
            } else if (cc == kEndOfFileMarker) {
                m_token->appendToCharacter(m_buffer);
                return advanceAndEmitToken(source, VTTTokenTypes::Character);
            } else {
                if (!equalLiteral(m_buffer, "&"))
                    m_token->appendToCharacter(m_buffer);
                m_buffer.clear();
                WEBVTT_ADVANCE_TO(DataState);
            }
        }
        END_STATE()

        WEBVTT_BEGIN_STATE(TagState) {
            if (isTokenizerWhitespace(cc)) {
                m_token->beginEmptyStartTag();
                WEBVTT_ADVANCE_TO(StartTagAnnotationState);
            } else if (cc == '.') {
                m_token->beginEmptyStartTag();
                WEBVTT_ADVANCE_TO(StartTagClassState);
            } else if (cc == '/') {
                WEBVTT_ADVANCE_TO(EndTagState);
            } else if (WTF::isASCIIDigit(cc)) {
                m_token->beginTimestampTag(cc);
                WEBVTT_ADVANCE_TO(TimestampTagState);
            } else if (cc == '>' || cc == kEndOfFileMarker) {
                m_token->beginEmptyStartTag();
                return advanceAndEmitToken(source, VTTTokenTypes::StartTag);
            } else {
                m_token->beginStartTag(cc);
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
                return advanceAndEmitToken(source, VTTTokenTypes::StartTag);
            } else {
                m_token->appendToName(cc);
                WEBVTT_ADVANCE_TO(StartTagState);
            }
        }
        END_STATE()

        WEBVTT_BEGIN_STATE(StartTagClassState) {
            if (isTokenizerWhitespace(cc)) {
                m_token->addNewClass();
                WEBVTT_ADVANCE_TO(StartTagAnnotationState);
            } else if (cc == '.') {
                m_token->addNewClass();
                WEBVTT_ADVANCE_TO(StartTagClassState);
            } else if (cc == '>' || cc == kEndOfFileMarker) {
                m_token->addNewClass();
                return advanceAndEmitToken(source, VTTTokenTypes::StartTag);
            } else {
                m_token->appendToClass(cc);
                WEBVTT_ADVANCE_TO(StartTagClassState);
            }

        }
        END_STATE()

        WEBVTT_BEGIN_STATE(StartTagAnnotationState) {
            if (cc == '>' || cc == kEndOfFileMarker) {
                m_token->addNewAnnotation();
                return advanceAndEmitToken(source, VTTTokenTypes::StartTag);
            }
            m_token->appendToAnnotation(cc);
            WEBVTT_ADVANCE_TO(StartTagAnnotationState);
        }
        END_STATE()

        WEBVTT_BEGIN_STATE(EndTagState) {
            if (cc == '>' || cc == kEndOfFileMarker)
                return advanceAndEmitToken(source, VTTTokenTypes::EndTag);
            m_token->appendToName(cc);
            WEBVTT_ADVANCE_TO(EndTagState);
        }
        END_STATE()

        WEBVTT_BEGIN_STATE(TimestampTagState) {
            if (cc == '>' || cc == kEndOfFileMarker)
                return advanceAndEmitToken(source, VTTTokenTypes::TimestampTag);
            m_token->appendToTimestamp(cc);
            WEBVTT_ADVANCE_TO(TimestampTagState);
        }
        END_STATE()

    }

    ASSERT_NOT_REACHED();
    return false;
}

}

