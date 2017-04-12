// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CrossOriginAttribute_h
#define CrossOriginAttribute_h

#include "platform/CrossOriginAttributeValue.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

CrossOriginAttributeValue GetCrossOriginAttributeValue(const String& value);

}  // namespace blink

#endif
