// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Parser_h
#define Parser_h

#include "platform/PlatformExport.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace protocol {

class Value;

PLATFORM_EXPORT PassOwnPtr<Value> parseJSON(const String& json);

} // namespace platform
} // namespace blink

#endif // !defined(Parser_h)
