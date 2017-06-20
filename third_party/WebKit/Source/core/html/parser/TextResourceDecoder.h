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
#include "platform/loader/fetch/TextResourceDecoderOptions.h"
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

  static std::unique_ptr<TextResourceDecoder> Create(
      const TextResourceDecoderOptions& options) {
    return WTF::WrapUnique(new TextResourceDecoder(options));
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

  bool SawError() const { return saw_error_; }
  size_t CheckForBOM(const char*, size_t);

 protected:
  TextResourceDecoder(const TextResourceDecoderOptions&);

 private:
  static const WTF::TextEncoding& DefaultEncoding(
      TextResourceDecoderOptions::ContentType,
      const WTF::TextEncoding& default_encoding);

  bool CheckForCSSCharset(const char*, size_t, bool& moved_data_to_buffer);
  bool CheckForXMLCharset(const char*, size_t, bool& moved_data_to_buffer);
  void CheckForMetaCharset(const char*, size_t);
  void AutoDetectEncodingIfAllowed(const char* data, size_t len);

  const TextResourceDecoderOptions options_;

  WTF::TextEncoding encoding_;
  std::unique_ptr<TextCodec> codec_;
  EncodingSource source_;
  Vector<char> buffer_;
  bool checked_for_bom_;
  bool checked_for_css_charset_;
  bool checked_for_xml_charset_;
  bool checked_for_meta_charset_;
  bool saw_error_;

  std::unique_ptr<HTMLMetaCharsetParser> charset_parser_;
};

}  // namespace blink

#endif
