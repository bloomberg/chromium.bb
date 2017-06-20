/*
    Copyright (C) 1999 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
    Copyright (C) 2006, 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

*/

#ifndef TextResourceDecoder_h
#define TextResourceDecoder_h

#include <memory>
#include "core/CoreExport.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/TextEncoding.h"

namespace blink {

class HTMLMetaCharsetParser;

class CORE_EXPORT TextResourceDecoder {
  USING_FAST_MALLOC(TextResourceDecoder);
  WTF_MAKE_NONCOPYABLE(TextResourceDecoder);

 public:
  enum EncodingSource {
    kDefaultEncoding,
    kAutoDetectedEncoding,
    kEncodingFromContentSniffing,
    kEncodingFromXMLHeader,
    kEncodingFromMetaTag,
    kEncodingFromCSSCharset,
    kEncodingFromHTTPHeader,
    kEncodingFromParentFrame
  };

  enum ContentType {
    kPlainTextContent,
    kHTMLContent,
    kXMLContent,
    kCSSContent,
    kMaxContentType = kCSSContent
  };  // PlainText only checks for BOM.

  static std::unique_ptr<TextResourceDecoder> Create(
      ContentType content_type,
      const WTF::TextEncoding& default_encoding = WTF::TextEncoding()) {
    return WTF::WrapUnique(
        new TextResourceDecoder(content_type, default_encoding,
                                kUseContentAndBOMBasedDetection, KURL()));
  }

  static std::unique_ptr<TextResourceDecoder> CreateWithAutoDetection(
      ContentType content_type,
      const WTF::TextEncoding& default_encoding,
      const KURL& url) {
    return WTF::WrapUnique(new TextResourceDecoder(
        content_type, default_encoding, kUseAllAutoDetection, url));
  }

  // Corresponds to utf-8 decode in Encoding spec:
  // https://encoding.spec.whatwg.org/#utf-8-decode.
  static std::unique_ptr<TextResourceDecoder> CreateAlwaysUseUTF8ForText() {
    return WTF::WrapUnique(new TextResourceDecoder(
        kPlainTextContent, UTF8Encoding(), kAlwaysUseUTF8ForText, KURL()));
  }
  ~TextResourceDecoder();

  void SetEncoding(const WTF::TextEncoding&, EncodingSource);
  const WTF::TextEncoding& Encoding() const { return encoding_; }
  bool EncodingWasDetectedHeuristically() const {
    return source_ == kAutoDetectedEncoding ||
           source_ == kEncodingFromContentSniffing;
  }

  String Decode(const char* data, size_t length);
  String Flush();

  void SetHintEncoding(const WTF::TextEncoding& encoding) {
    hint_encoding_ = encoding.GetName();
  }

  void UseLenientXMLDecoding() { use_lenient_xml_decoding_ = true; }
  bool SawError() const { return saw_error_; }
  size_t CheckForBOM(const char*, size_t);

 protected:
  // TextResourceDecoder does three kind of encoding detection:
  // 1. By BOM,
  // 2. By Content if |m_contentType| is not |PlainTextContext|
  //    (e.g. <meta> tag for HTML), and
  // 3. By detectTextEncoding().
  enum EncodingDetectionOption {
    // Use 1. + 2. + 3.
    kUseAllAutoDetection,

    // Use 1. + 2.
    kUseContentAndBOMBasedDetection,

    // Use None of them.
    // |m_contentType| must be |PlainTextContent| and
    // |m_encoding| must be UTF8Encoding.
    // This doesn't change encoding based on BOMs, but still processes
    // utf-8 BOMs so that utf-8 BOMs don't appear in the decoded result.
    kAlwaysUseUTF8ForText
  };

  TextResourceDecoder(ContentType,
                      const WTF::TextEncoding& default_encoding,
                      EncodingDetectionOption,
                      const KURL& hint_url);

 private:
  static const WTF::TextEncoding& DefaultEncoding(
      ContentType,
      const WTF::TextEncoding& default_encoding);

  bool CheckForCSSCharset(const char*, size_t, bool& moved_data_to_buffer);
  bool CheckForXMLCharset(const char*, size_t, bool& moved_data_to_buffer);
  void CheckForMetaCharset(const char*, size_t);
  bool ShouldAutoDetect() const;

  ContentType content_type_;
  WTF::TextEncoding encoding_;
  std::unique_ptr<TextCodec> codec_;
  EncodingSource source_;
  const char* hint_encoding_;
  const KURL hint_url_;
  Vector<char> buffer_;
  char hint_language_[3];
  bool checked_for_bom_;
  bool checked_for_css_charset_;
  bool checked_for_xml_charset_;
  bool checked_for_meta_charset_;
  bool use_lenient_xml_decoding_;  // Don't stop on XML decoding errors.
  bool saw_error_;
  EncodingDetectionOption encoding_detection_option_;

  std::unique_ptr<HTMLMetaCharsetParser> charset_parser_;
};

}  // namespace blink

#endif
