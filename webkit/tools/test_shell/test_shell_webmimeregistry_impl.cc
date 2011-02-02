// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_webmimeregistry_impl.h"

#include "base/string_util.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"

using WebKit::WebString;
using WebKit::WebMimeRegistry;

namespace {

// Convert a WebString to ASCII, falling back on an empty string in the case
// of a non-ASCII string.
std::string ToASCIIOrEmpty(const WebString& string) {
  if (!IsStringASCII(string))
    return std::string();
  return UTF16ToASCII(string);
}

}  // namespace

TestShellWebMimeRegistryImpl::TestShellWebMimeRegistryImpl() {
  // Claim we support Ogg+Theora/Vorbis.
  media_map_.insert("video/ogg");
  media_map_.insert("audio/ogg");
  media_map_.insert("application/ogg");
  codecs_map_.insert("theora");
  codecs_map_.insert("vorbis");

  // Claim we support WAV.
  media_map_.insert("audio/wav");
  media_map_.insert("audio/x-wav");
  codecs_map_.insert("1");  // PCM for WAV.
}

TestShellWebMimeRegistryImpl::~TestShellWebMimeRegistryImpl() {}

WebMimeRegistry::SupportsType
TestShellWebMimeRegistryImpl::supportsMediaMIMEType(
    const WebString& mime_type, const WebString& codecs) {
  // Not supporting the container is a flat-out no.
  if (!IsSupportedMediaMimeType(ToASCIIOrEmpty(mime_type).c_str()))
    return IsNotSupported;

  // If we don't recognize the codec, it's possible we support it.
  std::vector<std::string> parsed_codecs;
  net::ParseCodecString(ToASCIIOrEmpty(codecs).c_str(), &parsed_codecs, true);
  if (!AreSupportedMediaCodecs(parsed_codecs))
    return MayBeSupported;

  // Otherwise we have a perfect match.
  return IsSupported;
}

bool TestShellWebMimeRegistryImpl::IsSupportedMediaMimeType(
    const std::string& mime_type) {
  return media_map_.find(mime_type) != media_map_.end();
}

bool TestShellWebMimeRegistryImpl::AreSupportedMediaCodecs(
    const std::vector<std::string>& codecs) {
  for (size_t i = 0; i < codecs.size(); ++i) {
    if (codecs_map_.find(codecs[i]) == codecs_map_.end()) {
      return false;
    }
  }
  return true;
}
