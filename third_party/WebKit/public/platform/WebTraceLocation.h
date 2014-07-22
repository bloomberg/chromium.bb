// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTraceLocation_h
#define WebTraceLocation_h

namespace blink {

// This class is used to keep track of where posted tasks originate. See base/location.h in Chromium.
class WebTraceLocation {
public:
    // The strings passed in are not copied and must live for the duration of the program.
    WebTraceLocation(const char* functionName, const char* fileName)
        : m_functionName(functionName)
        , m_fileName(fileName)
    { }

    const char* functionName() const { return m_functionName; }
    const char* fileName() const { return m_fileName; }

private:
    const char* m_functionName;
    const char* m_fileName;
};

}

#endif // WebTraceLocation_h
