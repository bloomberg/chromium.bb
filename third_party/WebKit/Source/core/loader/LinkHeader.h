// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LinkHeader_h
#define LinkHeader_h

#include "wtf/text/WTFString.h"

namespace blink {

class LinkHeader {
public:
    LinkHeader(const String& header);

    const String url() const { return m_url; };
    const String rel() const { return m_rel; };
    bool valid() const { return m_isValid; };

    enum LinkParameterName {
        LinkParameterUnknown,
        LinkParameterRel,
    };

private:
    template <typename CharType>
    bool init(CharType* headerValue, unsigned len);
    void setValue(LinkParameterName, String value);

    String m_url;
    String m_rel;
    bool m_isValid;
};

}

#endif
