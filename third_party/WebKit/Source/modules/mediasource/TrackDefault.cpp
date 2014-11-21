// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/mediasource/TrackDefault.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/track/AudioTrack.h"
#include "core/html/track/TextTrack.h"
#include "core/html/track/VideoTrack.h"

namespace blink {

static const AtomicString& audioKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, audio, ("audio", AtomicString::ConstructFromLiteral));
    return audio;
}

static const AtomicString& videoKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, video, ("video", AtomicString::ConstructFromLiteral));
    return video;
}

static const AtomicString& textKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, text, ("text", AtomicString::ConstructFromLiteral));
    return text;
}

TrackDefault::TrackDefault(const AtomicString& type, const String& language, const String& label, const Vector<String>& kinds, const String& byteStreamTrackID, ExceptionState& exceptionState)
{
    // Per 11 Nov 2014 Editor's Draft
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#idl-def-TrackDefault
    // with expectation that
    // https://www.w3.org/Bugs/Public/show_bug.cgi?id=27352 will be fixed soon:
    // When this method is invoked, the user agent must run the following steps:
    // 1. if |language| is not an empty string and |language| is not a BCP 47
    //    language tag, then throw an INVALID_ACCESS_ERR and abort these steps.
    // FIXME: Implement BCP 47 language tag validation.

    if (type == audioKeyword()) {
        // 2.1. If |type| equals "audio":
        //      If any string in |kinds| contains a value that is not listed as
        //      applying to audio in the kind categories table, then throw a
        //      TypeError and abort these steps.
        for (const String& kind : kinds) {
            if (!AudioTrack::isValidKindKeyword(kind)) {
                exceptionState.throwTypeError("Invalid audio track default kind '" + kind + "'");
                return;
            }
        }
    } else if (type == videoKeyword()) {
        // 2.2. If |type| equals "video":
        //      If any string in |kinds| contains a value that is not listed as
        //      applying to video in the kind categories table, then throw a
        //      TypeError and abort these steps.
        for (const String& kind : kinds) {
            if (!VideoTrack::isValidKindKeyword(kind)) {
                exceptionState.throwTypeError("Invalid video track default kind '" + kind + "'");
                return;
            }
        }
    } else if (type == textKeyword()) {
        // 2.3. If |type| equals "text":
        //      If any string in |kinds| contains a value that is not listed in the
        //      text track kind list, then throw a TypeError and abort these
        //      steps.
        for (const String& kind : kinds) {
            if (!TextTrack::isValidKindKeyword(kind)) {
                exceptionState.throwTypeError("Invalid text track default kind '" + kind + "'");
                return;
            }
        }
    } else {
        ASSERT_NOT_REACHED(); // IDL enforcement should prevent this case.
    }

    // 3. Set the type attribute on this new object to |type|.
    m_type = type;

    // 4. Set the language attribute on this new object to |language|.
    m_language = language;

    // 5. Set the label attribute on this new object to |label|.
    m_label = label;

    // 6. Set the kinds attribute on this new object to |kinds|.
    m_kinds = kinds;

    // 7. Set the byteStreamTrackID attribute on this new object to
    //    |byteStreamTrackID|.
    m_byteStreamTrackID = byteStreamTrackID;
}

TrackDefault::~TrackDefault()
{
}

} // namespace blink
