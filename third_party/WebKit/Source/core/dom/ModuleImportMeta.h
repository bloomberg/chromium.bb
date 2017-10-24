// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleImportMeta_h
#define ModuleImportMeta_h

#include "core/CoreExport.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

// Represents import.meta data structure, which is the return value of
// https://html.spec.whatwg.org/#hostgetimportmetaproperties
class CORE_EXPORT ModuleImportMeta final {
 public:
  explicit ModuleImportMeta(const String& url) : url_(url) {}

  const String& Url() const { return url_; }

 private:
  const String url_;
};

}  // namespace blink

#endif
