// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ErrorSupport_h
#define ErrorSupport_h

#include "platform/PlatformExport.h"
#include "platform/inspector_protocol/Collections.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace protocol {

class PLATFORM_EXPORT ErrorSupport {
public:
    ErrorSupport();
    ErrorSupport(String* errorString);
    ~ErrorSupport();

    void push();
    void setName(const String&);
    void pop();
    void addError(const String&);
    bool hasErrors();
    String errors();

private:
    protocol::Vector<String> m_path;
    protocol::Vector<String> m_errors;
    String* m_errorString;
};

} // namespace platform
} // namespace blink

#endif // !defined(ErrorSupport_h)
