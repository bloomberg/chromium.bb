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

#include "platform/mhtml/MHTMLParser.h"

#include "platform/mhtml/ArchiveResource.h"
#include "platform/network/ParsedContentType.h"
#include "platform/text/QuotedPrintable.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/Base64.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/StringConcatenate.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

// This class is a limited MIME parser used to parse the MIME headers of MHTML
// files.
class MIMEHeader : public GarbageCollectedFinalized<MIMEHeader> {
 public:
  static MIMEHeader* Create() { return new MIMEHeader; }

  enum Encoding {
    kQuotedPrintable,
    kBase64,
    kEightBit,
    kSevenBit,
    kBinary,
    kUnknown
  };

  static MIMEHeader* ParseHeader(SharedBufferChunkReader* cr_lf_line_reader);

  bool IsMultipart() const {
    return content_type_.StartsWithIgnoringASCIICase("multipart/");
  }

  String ContentType() const { return content_type_; }
  String Charset() const { return charset_; }
  Encoding ContentTransferEncoding() const {
    return content_transfer_encoding_;
  }
  String ContentLocation() const { return content_location_; }
  String ContentID() const { return content_id_; }

  // Multi-part type and boundaries are only valid for multipart MIME headers.
  String MultiPartType() const { return multipart_type_; }
  String EndOfPartBoundary() const { return end_of_part_boundary_; }
  String EndOfDocumentBoundary() const { return end_of_document_boundary_; }

  DEFINE_INLINE_TRACE() {}

 private:
  MIMEHeader();

  static Encoding ParseContentTransferEncoding(const String&);

  String content_type_;
  String charset_;
  Encoding content_transfer_encoding_;
  String content_location_;
  String content_id_;
  String multipart_type_;
  String end_of_part_boundary_;
  String end_of_document_boundary_;
};

typedef HashMap<String, String> KeyValueMap;

static KeyValueMap RetrieveKeyValuePairs(SharedBufferChunkReader* buffer) {
  KeyValueMap key_value_pairs;
  String line;
  String key;
  StringBuilder value;
  while (!(line = buffer->NextChunkAsUTF8StringWithLatin1Fallback()).IsNull()) {
    if (line.IsEmpty())
      break;  // Empty line means end of key/value section.
    if (line[0] == '\t') {
      value.Append(line.Substring(1));
      continue;
    }
    // New key/value, store the previous one if any.
    if (!key.IsEmpty()) {
      if (key_value_pairs.find(key) != key_value_pairs.end())
        DVLOG(1) << "Key duplicate found in MIME header. Key is '" << key
                 << "', previous value replaced.";
      key_value_pairs.insert(key, value.ToString().StripWhiteSpace());
      key = String();
      value.Clear();
    }
    size_t semi_colon_index = line.find(':');
    if (semi_colon_index == kNotFound) {
      // This is not a key value pair, ignore.
      continue;
    }
    key =
        line.Substring(0, semi_colon_index).DeprecatedLower().StripWhiteSpace();
    value.Append(line.Substring(semi_colon_index + 1));
  }
  // Store the last property if there is one.
  if (!key.IsEmpty())
    key_value_pairs.Set(key, value.ToString().StripWhiteSpace());
  return key_value_pairs;
}

MIMEHeader* MIMEHeader::ParseHeader(SharedBufferChunkReader* buffer) {
  MIMEHeader* mime_header = MIMEHeader::Create();
  KeyValueMap key_value_pairs = RetrieveKeyValuePairs(buffer);
  KeyValueMap::iterator mime_parameters_iterator =
      key_value_pairs.find("content-type");
  if (mime_parameters_iterator != key_value_pairs.end()) {
    ParsedContentType parsed_content_type(mime_parameters_iterator->value,
                                          ParsedContentType::Mode::kRelaxed);
    mime_header->content_type_ = parsed_content_type.MimeType();
    if (!mime_header->IsMultipart()) {
      mime_header->charset_ = parsed_content_type.Charset().StripWhiteSpace();
    } else {
      mime_header->multipart_type_ =
          parsed_content_type.ParameterValueForName("type");
      mime_header->end_of_part_boundary_ =
          parsed_content_type.ParameterValueForName("boundary");
      if (mime_header->end_of_part_boundary_.IsNull()) {
        DVLOG(1) << "No boundary found in multipart MIME header.";
        return nullptr;
      }
      mime_header->end_of_part_boundary_.insert("--", 0);
      mime_header->end_of_document_boundary_ =
          mime_header->end_of_part_boundary_;
      mime_header->end_of_document_boundary_.append("--");
    }
  }

  mime_parameters_iterator = key_value_pairs.find("content-transfer-encoding");
  if (mime_parameters_iterator != key_value_pairs.end())
    mime_header->content_transfer_encoding_ =
        ParseContentTransferEncoding(mime_parameters_iterator->value);

  mime_parameters_iterator = key_value_pairs.find("content-location");
  if (mime_parameters_iterator != key_value_pairs.end())
    mime_header->content_location_ = mime_parameters_iterator->value;

  // See rfc2557 - section 8.3 - Use of the Content-ID header and CID URLs.
  mime_parameters_iterator = key_value_pairs.find("content-id");
  if (mime_parameters_iterator != key_value_pairs.end())
    mime_header->content_id_ = mime_parameters_iterator->value;

  return mime_header;
}

MIMEHeader::Encoding MIMEHeader::ParseContentTransferEncoding(
    const String& text) {
  String encoding = text.StripWhiteSpace().DeprecatedLower();
  if (encoding == "base64")
    return kBase64;
  if (encoding == "quoted-printable")
    return kQuotedPrintable;
  if (encoding == "8bit")
    return kEightBit;
  if (encoding == "7bit")
    return kSevenBit;
  if (encoding == "binary")
    return kBinary;
  DVLOG(1) << "Unknown encoding '" << text << "' found in MIME header.";
  return kUnknown;
}

MIMEHeader::MIMEHeader() : content_transfer_encoding_(kUnknown) {}

static bool SkipLinesUntilBoundaryFound(SharedBufferChunkReader& line_reader,
                                        const String& boundary) {
  String line;
  while (!(line = line_reader.NextChunkAsUTF8StringWithLatin1Fallback())
              .IsNull()) {
    if (line == boundary)
      return true;
  }
  return false;
}

MHTMLParser::MHTMLParser(RefPtr<const SharedBuffer> data)
    : line_reader_(std::move(data), "\r\n") {}

HeapVector<Member<ArchiveResource>> MHTMLParser::ParseArchive() {
  MIMEHeader* header = MIMEHeader::ParseHeader(&line_reader_);
  HeapVector<Member<ArchiveResource>> resources;
  if (!ParseArchiveWithHeader(header, resources))
    resources.clear();
  return resources;
}

bool MHTMLParser::ParseArchiveWithHeader(
    MIMEHeader* header,
    HeapVector<Member<ArchiveResource>>& resources) {
  if (!header) {
    DVLOG(1) << "Failed to parse MHTML part: no header.";
    return false;
  }

  if (!header->IsMultipart()) {
    // With IE a page with no resource is not multi-part.
    bool end_of_archive_reached = false;
    ArchiveResource* resource =
        ParseNextPart(*header, String(), String(), end_of_archive_reached);
    if (!resource)
      return false;
    resources.push_back(resource);
    return true;
  }

  // Skip the message content (it's a generic browser specific message).
  SkipLinesUntilBoundaryFound(line_reader_, header->EndOfPartBoundary());

  bool end_of_archive = false;
  while (!end_of_archive) {
    MIMEHeader* resource_header = MIMEHeader::ParseHeader(&line_reader_);
    if (!resource_header) {
      DVLOG(1) << "Failed to parse MHTML, invalid MIME header.";
      return false;
    }
    if (resource_header->ContentType() == "multipart/alternative") {
      // Ignore IE nesting which makes little sense (IE seems to nest only some
      // of the frames).
      if (!ParseArchiveWithHeader(resource_header, resources)) {
        DVLOG(1) << "Failed to parse MHTML subframe.";
        return false;
      }
      SkipLinesUntilBoundaryFound(line_reader_, header->EndOfPartBoundary());
      continue;
    }

    ArchiveResource* resource =
        ParseNextPart(*resource_header, header->EndOfPartBoundary(),
                      header->EndOfDocumentBoundary(), end_of_archive);
    if (!resource) {
      DVLOG(1) << "Failed to parse MHTML part.";
      return false;
    }
    resources.push_back(resource);
  }
  return true;
}

ArchiveResource* MHTMLParser::ParseNextPart(
    const MIMEHeader& mime_header,
    const String& end_of_part_boundary,
    const String& end_of_document_boundary,
    bool& end_of_archive_reached) {
  DCHECK_EQ(end_of_part_boundary.IsEmpty(), end_of_document_boundary.IsEmpty());

  // If no content transfer encoding is specified, default to binary encoding.
  MIMEHeader::Encoding content_transfer_encoding =
      mime_header.ContentTransferEncoding();
  if (content_transfer_encoding == MIMEHeader::kUnknown)
    content_transfer_encoding = MIMEHeader::kBinary;

  Vector<char> content;
  const bool check_boundary = !end_of_part_boundary.IsEmpty();
  bool end_of_part_reached = false;
  if (content_transfer_encoding == MIMEHeader::kBinary) {
    if (!check_boundary) {
      DVLOG(1) << "Binary contents requires end of part";
      return nullptr;
    }
    line_reader_.SetSeparator(end_of_part_boundary.Utf8().data());
    if (!line_reader_.NextChunk(content)) {
      DVLOG(1) << "Binary contents requires end of part";
      return nullptr;
    }
    line_reader_.SetSeparator("\r\n");
    Vector<char> next_chars;
    if (line_reader_.Peek(next_chars, 2) != 2) {
      DVLOG(1) << "Invalid seperator.";
      return nullptr;
    }
    end_of_part_reached = true;
    DCHECK(next_chars.size() == 2);
    end_of_archive_reached = (next_chars[0] == '-' && next_chars[1] == '-');
    if (!end_of_archive_reached) {
      String line = line_reader_.NextChunkAsUTF8StringWithLatin1Fallback();
      if (!line.IsEmpty()) {
        DVLOG(1) << "No CRLF at end of binary section.";
        return nullptr;
      }
    }
  } else {
    String line;
    while (!(line = line_reader_.NextChunkAsUTF8StringWithLatin1Fallback())
                .IsNull()) {
      end_of_archive_reached = (line == end_of_document_boundary);
      if (check_boundary &&
          (line == end_of_part_boundary || end_of_archive_reached)) {
        end_of_part_reached = true;
        break;
      }
      // Note that we use line.utf8() and not line.ascii() as ascii turns
      // special characters (such as tab, line-feed...) into '?'.
      content.Append(line.Utf8().data(), line.length());
      if (content_transfer_encoding == MIMEHeader::kQuotedPrintable) {
        // The line reader removes the \r\n, but we need them for the content in
        // this case as the QuotedPrintable decoder expects CR-LF terminated
        // lines.
        content.Append("\r\n", 2u);
      }
    }
  }
  if (!end_of_part_reached && check_boundary) {
    DVLOG(1) << "No boundary found for MHTML part.";
    return nullptr;
  }

  Vector<char> data;
  switch (content_transfer_encoding) {
    case MIMEHeader::kBase64:
      if (!Base64Decode(content.data(), content.size(), data)) {
        DVLOG(1) << "Invalid base64 content for MHTML part.";
        return nullptr;
      }
      break;
    case MIMEHeader::kQuotedPrintable:
      QuotedPrintableDecode(content.data(), content.size(), data);
      break;
    case MIMEHeader::kEightBit:
    case MIMEHeader::kSevenBit:
    case MIMEHeader::kBinary:
      data.Append(content.data(), content.size());
      break;
    default:
      DVLOG(1) << "Invalid encoding for MHTML part.";
      return nullptr;
  }
  RefPtr<SharedBuffer> content_buffer = SharedBuffer::AdoptVector(data);
  // FIXME: the URL in the MIME header could be relative, we should resolve it
  // if it is.  The specs mentions 5 ways to resolve a URL:
  // http://tools.ietf.org/html/rfc2557#section-5
  // IE and Firefox (UNMht) seem to generate only absolute URLs.
  KURL location = KURL(NullURL(), mime_header.ContentLocation());
  return ArchiveResource::Create(content_buffer, location,
                                 mime_header.ContentID(),
                                 AtomicString(mime_header.ContentType()),
                                 AtomicString(mime_header.Charset()));
}

// static
KURL MHTMLParser::ConvertContentIDToURI(const String& content_id) {
  // This function is based primarily on an example from rfc2557 in section
  // 9.5, but also based on more normative parts of specs like:
  // - rfc2557 - MHTML - section 8.3 - "Use of the Content-ID header and CID
  //                                    URLs"
  // - rfc1738 - URL - section 4 (reserved scheme names;  includes "cid")
  // - rfc2387 - multipart/related - section 3.4 - "Syntax" (cid := msg-id)
  // - rfc0822 - msg-id = "<" addr-spec ">"; addr-spec = local-part "@" domain

  if (content_id.length() <= 2)
    return KURL();

  if (!content_id.StartsWith('<') || !content_id.EndsWith('>'))
    return KURL();

  StringBuilder uri_builder;
  uri_builder.Append("cid:");
  uri_builder.Append(content_id, 1, content_id.length() - 2);
  return KURL(NullURL(), uri_builder.ToString());
}

}  // namespace blink
