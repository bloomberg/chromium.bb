// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/simple_webmimeregistry_impl.h"

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebString.h"

using WebKit::WebString;
using WebKit::WebMimeRegistry;

namespace webkit_glue {

//static
std::string SimpleWebMimeRegistryImpl::ToASCIIOrEmpty(const WebString& string) {
  return IsStringASCII(string) ? UTF16ToASCII(string) : std::string();
}

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
  // Media features are only supported at the content/ layer.
  return IsNotSupported;
}

WebMimeRegistry::SupportsType SimpleWebMimeRegistryImpl::supportsMediaMIMEType(
    const WebString& mime_type,
    const WebString& codecs,
    const WebString& key_system) {
  // Media features are only supported at the content/ layer.
  return IsNotSupported;
}

bool SimpleWebMimeRegistryImpl::supportsMediaSourceMIMEType(
    const WebString& mime_type,
    const WebString& codecs) {
  // Media features are only supported at the content/ layer.
  return IsNotSupported;
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
      base::FilePath::FromUTF16Unsafe(file_extension).value(), &mime_type);
  return WebString::fromUTF8(mime_type);
}

WebString SimpleWebMimeRegistryImpl::wellKnownMimeTypeForExtension(
    const WebString& file_extension) {
  std::string mime_type;
  net::GetWellKnownMimeTypeFromExtension(
      base::FilePath::FromUTF16Unsafe(file_extension).value(), &mime_type);
  return WebString::fromUTF8(mime_type);
}

WebString SimpleWebMimeRegistryImpl::mimeTypeFromFile(
    const WebString& file_path) {
  std::string mime_type;
  net::GetMimeTypeFromFile(base::FilePath::FromUTF16Unsafe(file_path),
                           &mime_type);
  return WebString::fromUTF8(mime_type);
}

WebString SimpleWebMimeRegistryImpl::preferredExtensionForMIMEType(
    const WebString& mime_type) {
  base::FilePath::StringType file_extension;
  net::GetPreferredExtensionForMimeType(ToASCIIOrEmpty(mime_type),
                                        &file_extension);
  return base::FilePath(file_extension).AsUTF16Unsafe();
}

}  // namespace webkit_glue
