// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_TYPE_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_TYPE_H_

#include "content/common/content_export.h"

namespace content {

// Used in histograms; explicitly assign each type and do not re-use old values.
enum ResourceType {
  RESOURCE_TYPE_MAIN_FRAME = 0,       // top level page
  RESOURCE_TYPE_SUB_FRAME = 1,        // frame or iframe
  RESOURCE_TYPE_STYLESHEET = 2,       // a CSS stylesheet
  RESOURCE_TYPE_SCRIPT = 3,           // an external script
  RESOURCE_TYPE_IMAGE = 4,            // an image (jpg/gif/png/etc)
  RESOURCE_TYPE_FONT_RESOURCE = 5,    // a font
  RESOURCE_TYPE_SUB_RESOURCE = 6,     // an "other" subresource.
  RESOURCE_TYPE_OBJECT = 7,           // an object (or embed) tag for a plugin.
  RESOURCE_TYPE_MEDIA = 8,            // a media resource.
  RESOURCE_TYPE_WORKER = 9,           // the main resource of a dedicated
                                      // worker.
  RESOURCE_TYPE_SHARED_WORKER = 10,   // the main resource of a shared worker.
  RESOURCE_TYPE_PREFETCH = 11,        // an explicitly requested prefetch
  RESOURCE_TYPE_FAVICON = 12,         // a favicon
  RESOURCE_TYPE_XHR = 13,             // a XMLHttpRequest
  RESOURCE_TYPE_PING = 14,            // a ping request for <a ping>/sendBeacon.
  RESOURCE_TYPE_SERVICE_WORKER = 15,  // the main resource of a service worker.
  RESOURCE_TYPE_CSP_REPORT = 16,      // a report of Content Security Policy
                                      // violations.
  RESOURCE_TYPE_PLUGIN_RESOURCE = 17, // a resource that a plugin requested.
  RESOURCE_TYPE_LAST_TYPE
};

CONTENT_EXPORT bool IsResourceTypeFrame(ResourceType type);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_TYPE_H_
