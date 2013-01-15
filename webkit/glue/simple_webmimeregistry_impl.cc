// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/simple_webmimeregistry_impl.h"

#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/media/crypto/key_systems.h"

using WebKit::WebString;
using WebKit::WebMimeRegistry;

namespace {

// Convert a WebString to ASCII, falling back on an empty string in the case
// of a non-ASCII string.
std::string ToASCIIOrEmpty(const WebString& string) {
  return IsStringASCII(string) ? UTF16ToASCII(string) : std::string();
}

}  // namespace

namespace webkit_glue {

WebMimeRegistry::SupportsType SimpleWebMimeRegistryImpl::supportsMIMEType(
    const WebString& mime_type) {
  return net::IsSupportedMimeType(ToASCIIOrEmpty(mime_type)) ?
      WebMimeRegistry::IsSupported : WebMimeRegistry::IsNotSupported;
}

WebMimeRegistry::SupportsType SimpleWebMimeRegistryImpl::supportsImageMIMEType(
    const WebString& mime_type) {
  return net::IsSupportedImageMimeType(ToASCIIOrEmpty(mime_type)) ?
      WebMimeRegistry::IsSupported : WebMimeRegistry::IsNotSupported;
}

WebMimeRegistry::SupportsType
    SimpleWebMimeRegistryImpl::supportsJavaScriptMIMEType(
    const WebString& mime_type) {
  return net::IsSupportedJavascriptMimeType(ToASCIIOrEmpty(mime_type)) ?
      WebMimeRegistry::IsSupported : WebMimeRegistry::IsNotSupported;
}

// When debugging layout tests failures in the test shell,
// see TestShellWebMimeRegistryImpl.
WebMimeRegistry::SupportsType SimpleWebMimeRegistryImpl::supportsMediaMIMEType(
    const WebString& mime_type, const WebString& codecs) {
  return supportsMediaMIMEType(mime_type, codecs, WebString());
}

WebMimeRegistry::SupportsType SimpleWebMimeRegistryImpl::supportsMediaMIMEType(
    const WebString& mime_type,
    const WebString& codecs,
    const WebString& key_system) {
  const std::string mime_type_ascii = ToASCIIOrEmpty(mime_type);
  // Not supporting the container is a flat-out no.
  if (!net::IsSupportedMediaMimeType(mime_type_ascii))
    return IsNotSupported;

  if (!key_system.isEmpty()) {
    // Check whether the key system is supported with the mime_type and codecs.

    // Not supporting the key system is a flat-out no.
    if (!webkit_media::IsSupportedKeySystem(key_system))
      return IsNotSupported;

    std::vector<std::string> strict_codecs;
    bool strip_suffix = !net::IsStrictMediaMimeType(mime_type_ascii);
    net::ParseCodecString(ToASCIIOrEmpty(codecs), &strict_codecs, strip_suffix);

    if (!webkit_media::IsSupportedKeySystemWithMediaMimeType(
            mime_type_ascii, strict_codecs, ToASCIIOrEmpty(key_system)))
      return IsNotSupported;

    // Continue processing the mime_type and codecs.
  }

  // Check list of strict codecs to see if it is supported.
  if (net::IsStrictMediaMimeType(mime_type_ascii)) {
    // We support the container, but no codecs were specified.
    if (codecs.isNull())
      return MayBeSupported;

    // Check if the codecs are a perfect match.
    std::vector<std::string> strict_codecs;
    net::ParseCodecString(ToASCIIOrEmpty(codecs), &strict_codecs, false);
    if (!net::IsSupportedStrictMediaMimeType(mime_type_ascii, strict_codecs))
      return IsNotSupported;

    // Good to go!
    return IsSupported;
  }

  // If we don't recognize the codec, it's possible we support it.
  std::vector<std::string> parsed_codecs;
  net::ParseCodecString(ToASCIIOrEmpty(codecs), &parsed_codecs, true);
  if (!net::AreSupportedMediaCodecs(parsed_codecs))
    return MayBeSupported;

  // Otherwise we have a perfect match.
  return IsSupported;
}

WebMimeRegistry::SupportsType
    SimpleWebMimeRegistryImpl::supportsNonImageMIMEType(
    const WebString& mime_type) {
  return net::IsSupportedNonImageMimeType(ToASCIIOrEmpty(mime_type)) ?
      WebMimeRegistry::IsSupported : WebMimeRegistry::IsNotSupported;
}

WebString SimpleWebMimeRegistryImpl::mimeTypeForExtension(
    const WebString& file_extension) {
  std::string mime_type;
  net::GetMimeTypeFromExtension(
      webkit_base::WebStringToFilePathString(file_extension), &mime_type);
  return ASCIIToUTF16(mime_type);
}

WebString SimpleWebMimeRegistryImpl::wellKnownMimeTypeForExtension(
    const WebString& file_extension) {
  std::string mime_type;
  net::GetWellKnownMimeTypeFromExtension(
      webkit_base::WebStringToFilePathString(file_extension), &mime_type);
  return ASCIIToUTF16(mime_type);
}

WebString SimpleWebMimeRegistryImpl::mimeTypeFromFile(
    const WebString& file_path) {
  std::string mime_type;
  net::GetMimeTypeFromFile(webkit_base::WebStringToFilePath(file_path),
                           &mime_type);
  return ASCIIToUTF16(mime_type);
}

WebString SimpleWebMimeRegistryImpl::preferredExtensionForMIMEType(
    const WebString& mime_type) {
  FilePath::StringType file_extension;
  net::GetPreferredExtensionForMimeType(ToASCIIOrEmpty(mime_type),
                                        &file_extension);
  return webkit_base::FilePathStringToWebString(file_extension);
}

}  // namespace webkit_glue
