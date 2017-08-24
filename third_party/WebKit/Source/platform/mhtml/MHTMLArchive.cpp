/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/mhtml/MHTMLArchive.h"

#include "build/build_config.h"
#include "platform/DateComponents.h"
#include "platform/SerializedResource.h"
#include "platform/SharedBuffer.h"
#include "platform/mhtml/ArchiveResource.h"
#include "platform/mhtml/MHTMLParser.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/text/QuotedPrintable.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/CryptographicallyRandomNumber.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/DateMath.h"
#include "platform/wtf/text/Base64.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

const char* const kQuotedPrintable = "quoted-printable";
const char* const kBase64 = "base64";
const char* const kBinary = "binary";

static String ReplaceNonPrintableCharacters(const String& text) {
  StringBuilder string_builder;
  for (size_t i = 0; i < text.length(); ++i) {
    if (IsASCIIPrintable(text[i]))
      string_builder.Append(text[i]);
    else
      string_builder.Append('?');
  }
  return string_builder.ToString();
}

MHTMLArchive::MHTMLArchive() {}

MHTMLArchive* MHTMLArchive::Create(const KURL& url,
                                   PassRefPtr<const SharedBuffer> data) {
  // MHTML pages can only be loaded from local URLs, http/https URLs, and
  // content URLs(Android specific).  The latter is now allowed due to full
  // sandboxing enforcement on MHTML pages.
  if (!CanLoadArchive(url))
    return nullptr;

  MHTMLParser parser(std::move(data));
  HeapVector<Member<ArchiveResource>> resources = parser.ParseArchive();
  if (resources.IsEmpty())
    return nullptr;  // Invalid MHTML file.

  MHTMLArchive* archive = new MHTMLArchive;

  size_t resources_count = resources.size();
  // The first document suitable resource is the main resource of the top frame.
  for (size_t i = 0; i < resources_count; ++i) {
    if (archive->MainResource()) {
      archive->AddSubresource(resources[i].Get());
      continue;
    }

    const AtomicString& mime_type = resources[i]->MimeType();
    bool is_mime_type_suitable_for_main_resource =
        MIMETypeRegistry::IsSupportedNonImageMIMEType(mime_type);
    // Want to allow image-only MHTML archives, but retain behavior for other
    // documents that have already been created expecting the first HTML page to
    // be considered the main resource.
    if (resources_count == 1 &&
        MIMETypeRegistry::IsSupportedImageResourceMIMEType(mime_type)) {
      is_mime_type_suitable_for_main_resource = true;
    }
    // explicitly disallow JS and CSS as the main resource.
    if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type) ||
        MIMETypeRegistry::IsSupportedStyleSheetMIMEType(mime_type))
      is_mime_type_suitable_for_main_resource = false;

    if (is_mime_type_suitable_for_main_resource)
      archive->SetMainResource(resources[i].Get());
    else
      archive->AddSubresource(resources[i].Get());
  }
  return archive;
}

bool MHTMLArchive::CanLoadArchive(const KURL& url) {
  // MHTML pages can only be loaded from local URLs, http/https URLs, and
  // content URLs(Android specific).  The latter is now allowed due to full
  // sandboxing enforcement on MHTML pages.
  if (SchemeRegistry::ShouldTreatURLSchemeAsLocal(url.Protocol()))
    return true;
  if (url.ProtocolIsInHTTPFamily())
    return true;
#if defined(OS_ANDROID)
  if (url.ProtocolIs("content"))
    return true;
#endif
  return false;
}

void MHTMLArchive::GenerateMHTMLHeader(const String& boundary,
                                       const KURL& url,
                                       const String& title,
                                       const String& mime_type,
                                       Vector<char>& output_buffer) {
  DCHECK(!boundary.IsEmpty());
  DCHECK(!mime_type.IsEmpty());

  DateComponents now;
  now.SetMillisecondsSinceEpochForDateTime(CurrentTimeMS());
  // TODO(lukasza): Passing individual date/time components seems fragile.
  String date_string = MakeRFC2822DateString(
      now.WeekDay(), now.MonthDay(), now.Month(), now.FullYear(), now.Hour(),
      now.Minute(), now.Second(), 0);

  StringBuilder string_builder;
  string_builder.Append("From: <Saved by Blink>\r\n");

  // Add the versioning information. This can be used to maintain these headers
  // for backward compatibility.
  string_builder.Append("X-Snapshot-Version: 1.0\r\n");
  // Encode the title as sequences of printable ASCII characters per RFC 1342
  // (https://tools.ietf.org/html/rfc1342). Specially, the encoded title will be
  // as:   =?utf-8?Q?encoded_text?=
  // where, "utf-8" is the chosen charset to represent the title and "Q" is the
  // Quoted-Printable format to convert to 7-bit printable ASCII characters.
  CString utf8_title = title.Utf8();
  Vector<char> encoded_title;
  QuotedPrintableEncode(utf8_title.data(), utf8_title.length(), encoded_title);
  string_builder.Append("X-Snapshot-Title: =?utf-8?Q?");
  string_builder.Append(encoded_title.data(), encoded_title.size());
  string_builder.Append("?=\r\n");
  // Add the document URL in the MHTML headers in order to avoid complicated
  // parsing to locate it in the multipart body headers.
  string_builder.Append("X-Snapshot-Content-Location: ");
  string_builder.Append(url.GetString());

  string_builder.Append("\r\nSubject: ");
  // We replace non ASCII characters with '?' characters to match IE's behavior.
  string_builder.Append(ReplaceNonPrintableCharacters(title));
  string_builder.Append("\r\nDate: ");
  string_builder.Append(date_string);
  string_builder.Append("\r\nMIME-Version: 1.0\r\n");
  string_builder.Append("Content-Type: multipart/related;\r\n");
  string_builder.Append("\ttype=\"");
  string_builder.Append(mime_type);
  string_builder.Append("\";\r\n");
  string_builder.Append("\tboundary=\"");
  string_builder.Append(boundary);
  string_builder.Append("\"\r\n\r\n");

  // We use utf8() below instead of ascii() as ascii() replaces CRLFs with ??
  // (we still only have put ASCII characters in it).
  DCHECK(string_builder.ToString().ContainsOnlyASCII());
  CString ascii_string = string_builder.ToString().Utf8();

  output_buffer.Append(ascii_string.data(), ascii_string.length());
}

void MHTMLArchive::GenerateMHTMLPart(const String& boundary,
                                     const String& content_id,
                                     EncodingPolicy encoding_policy,
                                     const SerializedResource& resource,
                                     Vector<char>& output_buffer) {
  DCHECK(!boundary.IsEmpty());
  DCHECK(content_id.IsEmpty() || content_id[0] == '<');

  StringBuilder string_builder;
  string_builder.Append("--");
  string_builder.Append(boundary);
  string_builder.Append("\r\n");

  string_builder.Append("Content-Type: ");
  string_builder.Append(resource.mime_type);
  string_builder.Append("\r\n");

  if (!content_id.IsEmpty()) {
    string_builder.Append("Content-ID: ");
    string_builder.Append(content_id);
    string_builder.Append("\r\n");
  }

  const char* content_encoding = 0;
  if (encoding_policy == kUseBinaryEncoding)
    content_encoding = kBinary;
  else if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
               resource.mime_type) ||
           MIMETypeRegistry::IsSupportedNonImageMIMEType(resource.mime_type))
    content_encoding = kQuotedPrintable;
  else
    content_encoding = kBase64;

  string_builder.Append("Content-Transfer-Encoding: ");
  string_builder.Append(content_encoding);
  string_builder.Append("\r\n");

  if (!resource.url.ProtocolIsAbout()) {
    string_builder.Append("Content-Location: ");
    string_builder.Append(resource.url.GetString());
    string_builder.Append("\r\n");
  }

  string_builder.Append("\r\n");

  CString ascii_string = string_builder.ToString().Utf8();
  output_buffer.Append(ascii_string.data(), ascii_string.length());

  if (!strcmp(content_encoding, kBinary)) {
    const char* data;
    size_t position = 0;
    while (size_t length = resource.data->GetSomeData(data, position)) {
      output_buffer.Append(data, length);
      position += length;
    }
  } else {
    // FIXME: ideally we would encode the content as a stream without having to
    // fetch it all.
    const SharedBuffer::DeprecatedFlatData flat_data(resource.data);
    const char* data = flat_data.Data();
    size_t data_length = flat_data.size();
    Vector<char> encoded_data;
    if (!strcmp(content_encoding, kQuotedPrintable)) {
      QuotedPrintableEncode(data, data_length, encoded_data);
      output_buffer.Append(encoded_data.data(), encoded_data.size());
      output_buffer.Append("\r\n", 2u);
    } else {
      DCHECK(!strcmp(content_encoding, kBase64));
      // We are not specifying insertLFs = true below as it would cut the lines
      // with LFs and MHTML requires CRLFs.
      Base64Encode(data, data_length, encoded_data);
      const size_t kMaximumLineLength = 76;
      size_t index = 0;
      size_t encoded_data_length = encoded_data.size();
      do {
        size_t line_length =
            std::min(encoded_data_length - index, kMaximumLineLength);
        output_buffer.Append(encoded_data.data() + index, line_length);
        output_buffer.Append("\r\n", 2u);
        index += kMaximumLineLength;
      } while (index < encoded_data_length);
    }
  }
}

void MHTMLArchive::GenerateMHTMLFooterForTesting(const String& boundary,
                                                 Vector<char>& output_buffer) {
  DCHECK(!boundary.IsEmpty());
  CString ascii_string = String("--" + boundary + "--\r\n").Utf8();
  output_buffer.Append(ascii_string.data(), ascii_string.length());
}

void MHTMLArchive::SetMainResource(ArchiveResource* main_resource) {
  main_resource_ = main_resource;
}

void MHTMLArchive::AddSubresource(ArchiveResource* resource) {
  const KURL& url = resource->Url();
  subresources_.Set(url, resource);
  KURL cid_uri = MHTMLParser::ConvertContentIDToURI(resource->ContentID());
  if (cid_uri.IsValid())
    subresources_.Set(cid_uri, resource);
}

ArchiveResource* MHTMLArchive::SubresourceForURL(const KURL& url) const {
  return subresources_.at(url.GetString());
}

DEFINE_TRACE(MHTMLArchive) {
  visitor->Trace(main_resource_);
  visitor->Trace(subresources_);
}

}  // namespace blink
