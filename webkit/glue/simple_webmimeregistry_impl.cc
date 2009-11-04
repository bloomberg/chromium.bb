// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/glue/simple_webmimeregistry_impl.h"

#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "net/base/mime_util.h"
#include "webkit/api/public/WebString.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebString;
using WebKit::WebMimeRegistry;

namespace {

// Convert a WebString to ASCII, falling back on an empty string in the case
// of a non-ASCII string.
std::string AsASCII(const WebString& string) {
  if (!IsStringASCII(string))
    return EmptyString();
  return UTF16ToASCII(string);
}

}  // namespace

namespace webkit_glue {

WebMimeRegistry::SupportsType SimpleWebMimeRegistryImpl::supportsMIMEType(
    const WebString& mime_type) {
  if (!net::IsSupportedMimeType(AsASCII(mime_type).c_str()))
    return WebMimeRegistry::IsNotSupported;
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType SimpleWebMimeRegistryImpl::supportsImageMIMEType(
    const WebString& mime_type) {
  if (!net::IsSupportedImageMimeType(AsASCII(mime_type).c_str()))
    return WebMimeRegistry::IsNotSupported;
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType SimpleWebMimeRegistryImpl::supportsJavaScriptMIMEType(
    const WebString& mime_type) {
  if (!net::IsSupportedJavascriptMimeType(AsASCII(mime_type).c_str()))
    return WebMimeRegistry::IsNotSupported;
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType SimpleWebMimeRegistryImpl::supportsMediaMIMEType(
    const WebString& mime_type, const WebString& codecs) {
  // Not supporting the container is a flat-out no.
  if (!net::IsSupportedMediaMimeType(AsASCII(mime_type).c_str()))
    return IsNotSupported;

  // If we don't recognize the codec, it's possible we support it.
  std::vector<std::string> parsed_codecs;
  net::ParseCodecString(AsASCII(codecs).c_str(), &parsed_codecs);
  if (!net::AreSupportedMediaCodecs(parsed_codecs))
    return MayBeSupported;

  // Otherwise we have a perfect match.
  return IsSupported;
}

WebMimeRegistry::SupportsType SimpleWebMimeRegistryImpl::supportsNonImageMIMEType(
    const WebString& mime_type) {
  if (!net::IsSupportedNonImageMimeType(AsASCII(mime_type).c_str()))
    return WebMimeRegistry::IsNotSupported;
  return WebMimeRegistry::IsSupported;
}

WebString SimpleWebMimeRegistryImpl::mimeTypeForExtension(
    const WebString& file_extension) {
  std::string mime_type;
  net::GetMimeTypeFromExtension(
      WebStringToFilePathString(file_extension), &mime_type);
  return ASCIIToUTF16(mime_type);
}

WebString SimpleWebMimeRegistryImpl::mimeTypeFromFile(
    const WebString& file_path) {
  std::string mime_type;
  net::GetMimeTypeFromFile(
      FilePath(WebStringToFilePathString(file_path)), &mime_type);
  return ASCIIToUTF16(mime_type);
}

WebString SimpleWebMimeRegistryImpl::preferredExtensionForMIMEType(
    const WebString& mime_type) {
  FilePath::StringType file_extension;
  net::GetPreferredExtensionForMimeType(AsASCII(mime_type),
                                        &file_extension);
  return FilePathStringToWebString(file_extension);
}

}  // namespace webkit_glue
