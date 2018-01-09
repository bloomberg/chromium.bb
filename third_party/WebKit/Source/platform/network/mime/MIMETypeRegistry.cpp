// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/mime/MIMETypeRegistry.h"

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "media/base/mime_util.h"
#include "media/filters/stream_parser_factory.h"
#include "net/base/mime_util.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/FilePathConversion.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/mime_registry.mojom-blink.h"
#include "third_party/WebKit/common/mime_util/mime_util.h"

namespace blink {

namespace {

struct MimeRegistryPtrHolder {
 public:
  MimeRegistryPtrHolder() {
    Platform::Current()->GetInterfaceProvider()->GetInterface(
        mojo::MakeRequest(&mime_registry));
  }
  ~MimeRegistryPtrHolder() = default;

  mojom::blink::MimeRegistryPtr mime_registry;
};

std::string ToASCIIOrEmpty(const WebString& string) {
  return string.ContainsOnlyASCII() ? string.Ascii() : std::string();
}

template <typename CHARTYPE, typename SIZETYPE>
std::string ToLowerASCIIInternal(CHARTYPE* str, SIZETYPE length) {
  std::string lower_ascii;
  lower_ascii.reserve(length);
  for (CHARTYPE* p = str; p < str + length; p++)
    lower_ascii.push_back(base::ToLowerASCII(static_cast<char>(*p)));
  return lower_ascii;
}

// Does the same as ToASCIIOrEmpty, but also makes the chars lower.
std::string ToLowerASCIIOrEmpty(const String& str) {
  if (str.IsEmpty() || !str.ContainsOnlyASCII())
    return std::string();
  if (str.Is8Bit())
    return ToLowerASCIIInternal(str.Characters8(), str.length());
  return ToLowerASCIIInternal(str.Characters16(), str.length());
}

STATIC_ASSERT_ENUM(MIMETypeRegistry::kIsNotSupported, media::IsNotSupported);
STATIC_ASSERT_ENUM(MIMETypeRegistry::kIsSupported, media::IsSupported);
STATIC_ASSERT_ENUM(MIMETypeRegistry::kMayBeSupported, media::MayBeSupported);

}  // namespace

String MIMETypeRegistry::GetMIMETypeForExtension(const String& ext) {
  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  DEFINE_STATIC_LOCAL(MimeRegistryPtrHolder, registry_holder, ());
  String mime_type;
  if (!registry_holder.mime_registry->GetMimeTypeFromExtension(
          ext.IsNull() ? "" : ext, &mime_type)) {
    return String();
  }
  return mime_type;
}

String MIMETypeRegistry::GetWellKnownMIMETypeForExtension(const String& ext) {
  // This method must be thread safe and should not consult the OS/registry.
  std::string mime_type;
  net::GetWellKnownMimeTypeFromExtension(WebStringToFilePath(ext).value(),
                                         &mime_type);
  return String::FromUTF8(mime_type.data(), mime_type.length());
}

bool MIMETypeRegistry::IsSupportedMIMEType(const String& mime_type) {
  return blink::IsSupportedMimeType(ToLowerASCIIOrEmpty(mime_type));
}

bool MIMETypeRegistry::IsSupportedImageMIMEType(const String& mime_type) {
  return blink::IsSupportedImageMimeType(ToLowerASCIIOrEmpty(mime_type));
}

bool MIMETypeRegistry::IsSupportedImageResourceMIMEType(
    const String& mime_type) {
  return IsSupportedImageMIMEType(mime_type);
}

bool MIMETypeRegistry::IsSupportedImagePrefixedMIMEType(
    const String& mime_type) {
  std::string ascii_mime_type = ToLowerASCIIOrEmpty(mime_type);
  return (blink::IsSupportedImageMimeType(ascii_mime_type) ||
          (base::StartsWith(ascii_mime_type, "image/",
                            base::CompareCase::SENSITIVE) &&
           blink::IsSupportedNonImageMimeType(ascii_mime_type)));
}

bool MIMETypeRegistry::IsSupportedImageMIMETypeForEncoding(
    const String& mime_type) {
  if (DeprecatedEqualIgnoringCase(mime_type, "image/jpeg") ||
      DeprecatedEqualIgnoringCase(mime_type, "image/png"))
    return true;
  if (DeprecatedEqualIgnoringCase(mime_type, "image/webp"))
    return true;
  return false;
}

bool MIMETypeRegistry::IsSupportedJavaScriptMIMEType(const String& mime_type) {
  return blink::IsSupportedJavascriptMimeType(ToLowerASCIIOrEmpty(mime_type));
}

bool MIMETypeRegistry::IsLegacySupportedJavaScriptLanguage(
    const String& language) {
  // Mozilla 1.8 accepts javascript1.0 - javascript1.7, but WinIE 7 accepts only
  // javascript1.1 - javascript1.3.
  // Mozilla 1.8 and WinIE 7 both accept javascript and livescript.
  // WinIE 7 accepts ecmascript and jscript, but Mozilla 1.8 doesn't.
  // Neither Mozilla 1.8 nor WinIE 7 accept leading or trailing whitespace.
  // We want to accept all the values that either of these browsers accept, but
  // not other values.

  // FIXME: This function is not HTML5 compliant. These belong in the MIME
  // registry as "text/javascript<version>" entries.
  return EqualIgnoringASCIICase(language, "javascript") ||
         EqualIgnoringASCIICase(language, "javascript1.0") ||
         EqualIgnoringASCIICase(language, "javascript1.1") ||
         EqualIgnoringASCIICase(language, "javascript1.2") ||
         EqualIgnoringASCIICase(language, "javascript1.3") ||
         EqualIgnoringASCIICase(language, "javascript1.4") ||
         EqualIgnoringASCIICase(language, "javascript1.5") ||
         EqualIgnoringASCIICase(language, "javascript1.6") ||
         EqualIgnoringASCIICase(language, "javascript1.7") ||
         EqualIgnoringASCIICase(language, "livescript") ||
         EqualIgnoringASCIICase(language, "ecmascript") ||
         EqualIgnoringASCIICase(language, "jscript");
}

bool MIMETypeRegistry::IsSupportedNonImageMIMEType(const String& mime_type) {
  return blink::IsSupportedNonImageMimeType(ToLowerASCIIOrEmpty(mime_type));
}

bool MIMETypeRegistry::IsSupportedMediaMIMEType(const String& mime_type,
                                                const String& codecs) {
  return SupportsMediaMIMEType(mime_type, codecs) != kIsNotSupported;
}

MIMETypeRegistry::SupportsType MIMETypeRegistry::SupportsMediaMIMEType(
    const String& mime_type,
    const String& codecs) {
  const std::string ascii_mime_type = ToLowerASCIIOrEmpty(mime_type);
  std::vector<std::string> codec_vector;
  media::SplitCodecsToVector(ToASCIIOrEmpty(codecs), &codec_vector, false);
  return static_cast<SupportsType>(
      media::IsSupportedMediaFormat(ascii_mime_type, codec_vector));
}

bool MIMETypeRegistry::IsSupportedMediaSourceMIMEType(const String& mime_type,
                                                      const String& codecs) {
  const std::string ascii_mime_type = ToLowerASCIIOrEmpty(mime_type);
  if (ascii_mime_type.empty())
    return false;
  std::vector<std::string> parsed_codec_ids;
  media::SplitCodecsToVector(ToASCIIOrEmpty(codecs), &parsed_codec_ids, false);
  return static_cast<MIMETypeRegistry::SupportsType>(
      media::StreamParserFactory::IsTypeSupported(ascii_mime_type,
                                                  parsed_codec_ids));
}

bool MIMETypeRegistry::IsJavaAppletMIMEType(const String& mime_type) {
  // Since this set is very limited and is likely to remain so we won't bother
  // with the overhead of using a hash set.  Any of the MIME types below may be
  // followed by any number of specific versions of the JVM, which is why we use
  // startsWith()
  return mime_type.StartsWithIgnoringASCIICase("application/x-java-applet") ||
         mime_type.StartsWithIgnoringASCIICase("application/x-java-bean") ||
         mime_type.StartsWithIgnoringASCIICase("application/x-java-vm");
}

bool MIMETypeRegistry::IsSupportedStyleSheetMIMEType(const String& mime_type) {
  return DeprecatedEqualIgnoringCase(mime_type, "text/css");
}

bool MIMETypeRegistry::IsSupportedFontMIMEType(const String& mime_type) {
  static const unsigned kFontLen = 5;
  if (!mime_type.StartsWithIgnoringASCIICase("font/"))
    return false;
  String sub_type = mime_type.Substring(kFontLen).DeprecatedLower();
  return sub_type == "woff" || sub_type == "woff2" || sub_type == "otf" ||
         sub_type == "ttf" || sub_type == "sfnt";
}

bool MIMETypeRegistry::IsSupportedTextTrackMIMEType(const String& mime_type) {
  return DeprecatedEqualIgnoringCase(mime_type, "text/vtt");
}

}  // namespace blink
