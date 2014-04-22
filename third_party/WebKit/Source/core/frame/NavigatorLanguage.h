// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorLanguage_h
#define NavigatorLanguage_h

#include "wtf/text/AtomicString.h"

namespace WebCore {

class NavigatorLanguage {
public:
    AtomicString language(bool&);
};

} // namespace WebCore

#endif // NavigatorLanguage_h
