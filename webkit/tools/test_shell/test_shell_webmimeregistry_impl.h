// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBMIMEREGISTRY_IMPL_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBMIMEREGISTRY_IMPL_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"

class TestShellWebMimeRegistryImpl
    : public webkit_glue::SimpleWebMimeRegistryImpl {
 public:
  TestShellWebMimeRegistryImpl();
  virtual ~TestShellWebMimeRegistryImpl();

  // Override to force that we only support ogg, vorbis and theora.
  //
  // Media layout tests use canPlayType() to determine the test input files.
  // Different flavours of Chromium support different codecs, which has an
  // impact on how canPlayType() behaves.  Since Chromium's baselines are
  // generated against ogg/vorbis/theora content we need to lock down how
  // canPlayType() behaves when running layout tests.
  virtual WebKit::WebMimeRegistry::SupportsType supportsMediaMIMEType(
      const WebKit::WebString&, const WebKit::WebString&) OVERRIDE;

 private:
  bool IsSupportedMediaMimeType(const std::string& mime_type);
  bool AreSupportedMediaCodecs(const std::vector<std::string>& codecs);

  typedef base::hash_set<std::string> MimeMappings;
  MimeMappings media_map_;
  MimeMappings codecs_map_;

  DISALLOW_COPY_AND_ASSIGN(TestShellWebMimeRegistryImpl);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBMIMEREGISTRY_IMPL_H_
