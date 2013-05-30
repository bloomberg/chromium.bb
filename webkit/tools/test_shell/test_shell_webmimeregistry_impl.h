// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

  // Override to force that we only support types and codecs that are supported
  // by all variations of Chromium.
  //
  // Media layout tests use canPlayType() to determine the test input files.
  // Different flavours of Chromium support different codecs, which has an
  // impact on how canPlayType() behaves.  Since Chromium's baselines and
  // expectations are generated against the common set of types, we need to
  // prevent canPlayType() from indicating it supports other types when running
  // layout tests.
  virtual WebKit::WebMimeRegistry::SupportsType supportsMediaMIMEType(
      const WebKit::WebString&,
      const WebKit::WebString&,
      const WebKit::WebString&) OVERRIDE;

 private:
  bool IsBlacklistedMediaMimeType(const std::string& mime_type);
  bool HasBlacklistedMediaCodecs(const std::vector<std::string>& codecs);

  std::vector<std::string> blacklisted_media_types_;
  std::vector<std::string> blacklisted_media_codecs_;

  DISALLOW_COPY_AND_ASSIGN(TestShellWebMimeRegistryImpl);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBMIMEREGISTRY_IMPL_H_
