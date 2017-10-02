// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResourceBundleHelper_h
#define ResourceBundleHelper_h

#include "platform/PlatformExport.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

// Provides access to ui::ResourceBundle in Blink. This
// allows Blink to directly load resources from the bundle.
class ResourceBundleHelper {
  STATIC_ONLY(ResourceBundleHelper);

 public:
  // Returns the contents of a resource as a string specified by the
  // resource id from Grit.
  static PLATFORM_EXPORT String GetResourceAsString(int resource_id);

  // Uncompresses a gzipped resource and returns it as a string. The resource
  // is specified by the resource id from Grit.
  static PLATFORM_EXPORT String UncompressResourceAsString(int resource_id);
};

}  // namespace blink

#endif  // ResourceBundleHelper_h
