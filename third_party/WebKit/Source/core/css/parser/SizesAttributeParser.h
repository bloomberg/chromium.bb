// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SizesAttributeParser_h
#define SizesAttributeParser_h

#include "core/css/MediaValues.h"
#include "core/css/parser/MediaQueryBlockWatcher.h"
#include "core/css/parser/MediaQueryParser.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class SizesAttributeParser {
    STACK_ALLOCATED();
public:
    SizesAttributeParser(PassRefPtr<MediaValues>, const String&);

    bool viewportDependant() const { return m_viewportDependant; }
    float length();

private:
    bool parse(Vector<MediaQueryToken>& tokens);
    bool parseMediaConditionAndLength(MediaQueryTokenIterator startToken, MediaQueryTokenIterator endToken);
    float effectiveSize();
    bool calculateLengthInPixels(MediaQueryTokenIterator startToken, MediaQueryTokenIterator endToken, float& result);
    bool mediaConditionMatches(PassRefPtrWillBeRawPtr<MediaQuerySet> mediaCondition);
    unsigned effectiveSizeDefaultValue();

    RefPtrWillBeMember<MediaQuerySet> m_mediaCondition;
    RefPtr<MediaValues> m_mediaValues;
    float m_length;
    bool m_lengthWasSet;
    bool m_viewportDependant;
    Vector<MediaQueryToken> m_tokens;
    bool m_isValid;
    MediaQueryBlockWatcher m_blockWatcher;
};

} // namespace

#endif

