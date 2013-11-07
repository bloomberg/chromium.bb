// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_SIMPLE_WEBMIMEREGISTRY_IMPL_H_
#define WEBKIT_GLUE_SIMPLE_WEBMIMEREGISTRY_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "third_party/WebKit/public/platform/WebMimeRegistry.h"
#include "webkit/glue/webkit_glue_export.h"

namespace webkit_glue {

class WEBKIT_GLUE_EXPORT SimpleWebMimeRegistryImpl :
    NON_EXPORTED_BASE(public blink::WebMimeRegistry) {
 public:
  SimpleWebMimeRegistryImpl() {}
  virtual ~SimpleWebMimeRegistryImpl() {}

  // Convert a WebString to ASCII, falling back on an empty string in the case
  // of a non-ASCII string.
  static std::string ToASCIIOrEmpty(const blink::WebString& string);

  // WebMimeRegistry methods:
  virtual blink::WebMimeRegistry::SupportsType supportsMIMEType(
      const blink::WebString&);
  virtual blink::WebMimeRegistry::SupportsType supportsImageMIMEType(
      const blink::WebString&);
  virtual blink::WebMimeRegistry::SupportsType supportsJavaScriptMIMEType(
      const blink::WebString&);
  virtual blink::WebMimeRegistry::SupportsType supportsMediaMIMEType(
      const blink::WebString&,
      const blink::WebString&,
      const blink::WebString&);
  virtual bool supportsMediaSourceMIMEType(const blink::WebString&,
                                           const blink::WebString&);
  virtual blink::WebMimeRegistry::SupportsType supportsNonImageMIMEType(
      const blink::WebString&);
  virtual blink::WebString mimeTypeForExtension(const blink::WebString&);
  virtual blink::WebString wellKnownMimeTypeForExtension(
      const blink::WebString&);
  virtual blink::WebString mimeTypeFromFile(const blink::WebString&);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_SIMPLE_WEBMIMEREGISTRY_IMPL_H_
