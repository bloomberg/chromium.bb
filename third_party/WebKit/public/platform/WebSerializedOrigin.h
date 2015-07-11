// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSerializedOrigin_h
#define WebSerializedOrigin_h

#include "WebString.h"

#if !INSIDE_BLINK
#include <url/deprecated_serialized_origin.h>
#endif

namespace blink {

class SecurityOrigin;
class WebSecurityOrigin;

// WebSerializedOrigin represents a serialized Web Origin specified in RFC6454.
class WebSerializedOrigin {
public:
    WebSerializedOrigin() : m_string("null") { }
    BLINK_PLATFORM_EXPORT WebSerializedOrigin(const WebSecurityOrigin&);

#if INSIDE_BLINK
    BLINK_PLATFORM_EXPORT WebSerializedOrigin(const SecurityOrigin&);
#else
    WebSerializedOrigin(const url::DeprecatedSerializedOrigin& origin) : m_string(WebString::fromUTF8(origin.string())) { }
    operator url::DeprecatedSerializedOrigin() const { return url::DeprecatedSerializedOrigin(m_string.utf8()); }
#endif

    const WebString& string() const
    {
        return m_string;
    }

private:
    const WebString m_string;
};

} // namespace blink

#endif // #ifndef SerializedOrigin_h
