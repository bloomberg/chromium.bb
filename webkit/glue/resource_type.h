// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_RESOURCE_TYPE_H__
#define WEBKIT_GLUE_RESOURCE_TYPE_H__

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "webkit/glue/webkit_glue_export.h"

class ResourceType {
 public:
  enum Type {
    MAIN_FRAME = 0,  // top level page
    SUB_FRAME,       // frame or iframe
    STYLESHEET,      // a CSS stylesheet
    SCRIPT,          // an external script
    IMAGE,           // an image (jpg/gif/png/etc)
    FONT_RESOURCE,   // a font
    SUB_RESOURCE,    // an "other" subresource.
    OBJECT,          // an object (or embed) tag for a plugin,
                     // or a resource that a plugin requested.
    MEDIA,           // a media resource.
    WORKER,          // the main resource of a dedicated worker.
    SHARED_WORKER,   // the main resource of a shared worker.
    PREFETCH,        // an explicitly requested prefetch
    PRERENDER,       // an explicitly requested prerender
    FAVICON,         // a favicon
    XHR,             // a XMLHttpRequest
    LAST_TYPE        // Place holder so we don't need to change ValidType
                     // everytime.
  };

  static bool ValidType(int32 type) {
    return type >= MAIN_FRAME && type < LAST_TYPE;
  }

  static Type FromInt(int32 type) {
    return static_cast<Type>(type);
  }

  WEBKIT_GLUE_EXPORT static Type FromTargetType(
      WebKit::WebURLRequest::TargetType type);

  static bool IsFrame(ResourceType::Type type) {
    return type == MAIN_FRAME || type == SUB_FRAME;
  }

  static bool IsSharedWorker(ResourceType::Type type) {
    return type == SHARED_WORKER;
  }

  static bool IsSubresource(ResourceType::Type type) {
    return type == STYLESHEET ||
           type == SCRIPT ||
           type == IMAGE ||
           type == FONT_RESOURCE ||
           type == SUB_RESOURCE ||
           type == WORKER ||
           type == XHR;
  }

 private:
  // Don't instantiate this class.
  ResourceType();
  ~ResourceType();
};
#endif  // WEBKIT_GLUE_RESOURCE_TYPE_H__
