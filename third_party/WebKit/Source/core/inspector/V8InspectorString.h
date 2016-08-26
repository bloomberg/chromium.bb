// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InspectorString_h
#define V8InspectorString_h

#include "core/CoreExport.h"
#include "platform/v8_inspector/public/StringBuffer.h"
#include "platform/v8_inspector/public/StringView.h"
#include "wtf/text/StringView.h"
#include "wtf/text/WTFString.h"

#include <memory>

namespace blink {

// Note that passed string must outlive the resulting StringView. This implies it must not be a temporary object.
CORE_EXPORT v8_inspector::StringView toV8InspectorStringView(const StringView&);
CORE_EXPORT std::unique_ptr<v8_inspector::StringBuffer> toV8InspectorStringBuffer(const StringView&);
CORE_EXPORT String toCoreString(const v8_inspector::StringView&);
CORE_EXPORT String toCoreString(std::unique_ptr<v8_inspector::StringBuffer>);

} // namespace blink

#endif // V8InspectorString_h
