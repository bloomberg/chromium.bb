// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_webmimeregistry_impl.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"

using WebKit::WebString;
using WebKit::WebMimeRegistry;

namespace {

// Convert a WebString to ASCII, falling back on an empty string in the case
// of a non-ASCII string.
std::string ToASCIIOrEmpty(const WebString& string) {
  return IsStringASCII(string) ? UTF16ToASCII(string) : std::string();
}

}  // namespace

TestShellWebMimeRegistryImpl::TestShellWebMimeRegistryImpl() {
  net::GetMediaTypesBlacklistedForTests(&blacklisted_media_types_);

  net::GetMediaCodecsBlacklistedForTests(&blacklisted_media_codecs_);
}

TestShellWebMimeRegistryImpl::~TestShellWebMimeRegistryImpl() {}

// Returns IsNotSupported if mime_type or any of the codecs are not supported.
// Otherwse, defers to the real registry.
WebMimeRegistry::SupportsType
TestShellWebMimeRegistryImpl::supportsMediaMIMEType(
    const WebString& mime_type,
    const WebString& codecs,
    const WebKit::WebString& key_system) {
  if (IsBlacklistedMediaMimeType(ToASCIIOrEmpty(mime_type)))
    return IsNotSupported;

  std::vector<std::string> parsed_codecs;
  net::ParseCodecString(ToASCIIOrEmpty(codecs), &parsed_codecs, true);
  if (HasBlacklistedMediaCodecs(parsed_codecs))
    return IsNotSupported;

  return SimpleWebMimeRegistryImpl::supportsMediaMIMEType(
      mime_type, codecs, key_system);
}

bool TestShellWebMimeRegistryImpl::IsBlacklistedMediaMimeType(
    const std::string& mime_type) {
  for (size_t i = 0; i < blacklisted_media_types_.size(); ++i) {
    if (blacklisted_media_types_[i] == mime_type)
      return true;
  }
  return false;
}

bool TestShellWebMimeRegistryImpl::HasBlacklistedMediaCodecs(
    const std::vector<std::string>& codecs) {
  for (size_t i = 0; i < codecs.size(); ++i) {
    for (size_t j = 0; j < blacklisted_media_codecs_.size(); ++j) {
      if (blacklisted_media_codecs_[j] == codecs[i])
        return true;
    }
  }
  return false;
}
