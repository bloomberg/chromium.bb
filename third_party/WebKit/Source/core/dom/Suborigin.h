// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Suborigin_h
#define Suborigin_h

#include "core/CoreExport.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;

class CORE_EXPORT SuboriginPolicy {
public:
    static void logSuboriginHeaderError(Document&, const String& message);
    static String parseSuboriginName(Document&, const String& header);
};

} // namespace blink

#endif // Suborigin_h
