// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/ftp_directory_listing_response_delegate.h"

#include <vector>

#include "base/i18n/icu_string_conversions.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/ftp/ftp_directory_listing_parsers.h"
#include "net/ftp/ftp_server_type_histograms.h"
#include "unicode/ucsdet.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLLoaderClient.h"

using WebKit::WebURLLoader;
using WebKit::WebURLLoaderClient;
using WebKit::WebURLResponse;

namespace {

// A very simple-minded character encoding detection.
// TODO(jungshik): We can apply more heuristics here (e.g. using various hints
// like TLD, the UI language/default encoding of a client, etc). In that case,
// this should be pulled out of here and moved somewhere in base because there
// can be other use cases.
std::string DetectEncoding(const std::string& text) {
  if (IsStringASCII(text))
    return std::string();
  UErrorCode status = U_ZERO_ERROR;
  UCharsetDetector* detector = ucsdet_open(&status);
  ucsdet_setText(detector, text.data(), static_cast<int32_t>(text.length()),
                 &status);
  const UCharsetMatch* match = ucsdet_detect(detector, &status);
  const char* encoding = ucsdet_getName(match, &status);
  ucsdet_close(detector);
  // Should we check the quality of the match? A rather arbitrary number is
  // assigned by ICU and it's hard to come up with a lower limit.
  if (U_FAILURE(status))
    return std::string();
  return encoding;
}

string16 RawByteSequenceToFilename(const char* raw_filename,
                                   const std::string& encoding) {
  if (encoding.empty())
    return ASCIIToUTF16(raw_filename);

  // Try the detected encoding before falling back to the native codepage.
  // Using the native codepage does not make much sense, but we don't have
  // much else to resort to.
  string16 filename;
  if (!base::CodepageToUTF16(raw_filename, encoding.c_str(),
                             base::OnStringConversionError::SUBSTITUTE,
                             &filename))
    filename = WideToUTF16Hack(base::SysNativeMBToWide(raw_filename));
  return filename;
}

void ExtractFullLinesFromBuffer(std::string* buffer,
                                std::vector<std::string>* lines) {
  int cut_pos = 0;
  for (size_t i = 0; i < buffer->length(); i++) {
    if ((*buffer)[i] != '\n')
      continue;
    size_t line_length = i - cut_pos;
    if (line_length > 0 && (*buffer)[i - 1] == '\r')
      line_length--;
    lines->push_back(buffer->substr(cut_pos, line_length));
    cut_pos = i + 1;
  }
  buffer->erase(0, cut_pos);
}

void LogFtpServerType(char server_type) {
  switch (server_type) {
    case 'E':
      net::UpdateFtpServerTypeHistograms(net::SERVER_EPLF);
      break;
    case 'V':
      net::UpdateFtpServerTypeHistograms(net::SERVER_VMS);
      break;
    case 'C':
      net::UpdateFtpServerTypeHistograms(net::SERVER_CMS);
      break;
    case 'W':
      net::UpdateFtpServerTypeHistograms(net::SERVER_DOS);
      break;
    case 'O':
      net::UpdateFtpServerTypeHistograms(net::SERVER_OS2);
      break;
    case 'U':
      net::UpdateFtpServerTypeHistograms(net::SERVER_LSL);
      break;
    case 'w':
      net::UpdateFtpServerTypeHistograms(net::SERVER_W16);
      break;
    case 'D':
      net::UpdateFtpServerTypeHistograms(net::SERVER_DLS);
      break;
    default:
      net::UpdateFtpServerTypeHistograms(net::SERVER_UNKNOWN);
      break;
  }
}

}  // namespace

namespace webkit_glue {

FtpDirectoryListingResponseDelegate::FtpDirectoryListingResponseDelegate(
    WebURLLoaderClient* client,
    WebURLLoader* loader,
    const WebURLResponse& response)
    : client_(client),
      loader_(loader),
      original_response_(response),
      parser_fallback_(false) {
  Init();
}

void FtpDirectoryListingResponseDelegate::OnReceivedData(const char* data,
                                                         int data_len) {
  // Keep the raw data in case we have to use the old parser later.
  input_buffer_.append(data, data_len);

  if (!parser_fallback_) {
    if (buffer_.ConsumeData(data, data_len) == net::OK)
      return;
    parser_fallback_ = true;
  }

  FeedFallbackParser();
  SendResponseBufferToClient();
}

void FtpDirectoryListingResponseDelegate::OnCompletedRequest() {
  if (!parser_fallback_) {
    if (buffer_.ProcessRemainingData() == net::OK) {
      while (buffer_.EntryAvailable())
        AppendEntryToResponseBuffer(buffer_.PopEntry());
      SendResponseBufferToClient();
      net::UpdateFtpServerTypeHistograms(buffer_.GetServerType());
      return;
    }
    parser_fallback_ = true;
  }

  FeedFallbackParser();
  SendResponseBufferToClient();

  // Only log the server type if we got enough data to reliably detect it.
  if (parse_state_.parsed_one)
    LogFtpServerType(parse_state_.lstyle);
}

void FtpDirectoryListingResponseDelegate::Init() {
  memset(&parse_state_, 0, sizeof(parse_state_));

  GURL response_url(original_response_.url());
  UnescapeRule::Type unescape_rules = UnescapeRule::SPACES |
                                      UnescapeRule::URL_SPECIAL_CHARS;
  std::string unescaped_path = UnescapeURLComponent(response_url.path(),
                                                    unescape_rules);
  string16 path_utf16;
  // Per RFC 2640, FTP servers should use UTF-8 or its proper subset ASCII,
  // but many old FTP servers use legacy encodings. Try UTF-8 first and
  // detect the encoding.
  if (IsStringUTF8(unescaped_path)) {
    path_utf16 = UTF8ToUTF16(unescaped_path);
  } else {
    std::string encoding = DetectEncoding(unescaped_path);
    // Try the detected encoding. If it fails, resort to the
    // OS native encoding.
    if (encoding.empty() ||
        !base::CodepageToUTF16(unescaped_path, encoding.c_str(),
                               base::OnStringConversionError::SUBSTITUTE,
                               &path_utf16))
      path_utf16 = WideToUTF16Hack(base::SysNativeMBToWide(unescaped_path));
  }

  response_buffer_ = net::GetDirectoryListingHeader(path_utf16);

  // If this isn't top level directory (i.e. the path isn't "/",)
  // add a link to the parent directory.
  if (response_url.path().length() > 1) {
    response_buffer_.append(
        net::GetDirectoryListingEntry(ASCIIToUTF16(".."),
                                      std::string(),
                                      false, 0,
                                      base::Time()));
  }
}

void FtpDirectoryListingResponseDelegate::FeedFallbackParser() {
  // If all we've seen so far is ASCII, encoding_ is empty. Try to detect the
  // encoding. We don't do the separate UTF-8 check here because the encoding
  // detection with a longer chunk (as opposed to the relatively short path
  // component of the url) is unlikely to mistake UTF-8 for a legacy encoding.
  // If it turns out to be wrong, a separate UTF-8 check has to be added.
  //
  // TODO(jungshik): UTF-8 has to be 'enforced' without any heuristics when
  // we're talking to an FTP server compliant to RFC 2640 (that is, its response
  // to FEAT command includes 'UTF8').
  // See http://wiki.filezilla-project.org/Character_Set
  if (encoding_.empty())
    encoding_ = DetectEncoding(input_buffer_);

  std::vector<std::string> lines;
  ExtractFullLinesFromBuffer(&input_buffer_, &lines);

  for (std::vector<std::string>::const_iterator line = lines.begin();
       line != lines.end(); ++line) {
    struct net::list_result result;
    int line_type = net::ParseFTPList(line->c_str(), &parse_state_, &result);

    // The original code assumed months are in range 0-11 (PRExplodedTime),
    // but our Time class expects a 1-12 range. Adjust it here, because
    // the third-party parsing code uses bit-shifting on the month,
    // and it'd be too easy to break that logic.
    result.fe_time.month++;
    DCHECK_LE(1, result.fe_time.month);
    DCHECK_GE(12, result.fe_time.month);

    int64 file_size;
    switch (line_type) {
      case 'd':  // Directory entry.
        response_buffer_.append(net::GetDirectoryListingEntry(
            RawByteSequenceToFilename(result.fe_fname, encoding_),
            result.fe_fname, true, 0,
            base::Time::FromLocalExploded(result.fe_time)));
        break;
      case 'f':  // File entry.
        if (StringToInt64(result.fe_size, &file_size)) {
          response_buffer_.append(net::GetDirectoryListingEntry(
              RawByteSequenceToFilename(result.fe_fname, encoding_),
              result.fe_fname, false, file_size,
              base::Time::FromLocalExploded(result.fe_time)));
        }
        break;
      case 'l': {  // Symlink entry.
          std::string filename(result.fe_fname, result.fe_fnlen);

          // Parsers for styles 'U' and 'W' handle " -> " themselves.
          if (parse_state_.lstyle != 'U' && parse_state_.lstyle != 'W') {
            std::string::size_type offset = filename.find(" -> ");
            if (offset != std::string::npos)
              filename = filename.substr(0, offset);
          }

          if (StringToInt64(result.fe_size, &file_size)) {
            response_buffer_.append(net::GetDirectoryListingEntry(
                RawByteSequenceToFilename(filename.c_str(), encoding_),
                filename, false, file_size,
                base::Time::FromLocalExploded(result.fe_time)));
          }
        }
        break;
      case '?':  // Junk entry.
      case '"':  // Comment entry.
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

void FtpDirectoryListingResponseDelegate::AppendEntryToResponseBuffer(
    const net::FtpDirectoryListingEntry& entry) {
  switch (entry.type) {
    case net::FtpDirectoryListingEntry::FILE:
      response_buffer_.append(
          net::GetDirectoryListingEntry(entry.name, std::string(), false,
                                        entry.size, entry.last_modified));
      break;
    case net::FtpDirectoryListingEntry::DIRECTORY:
      response_buffer_.append(
          net::GetDirectoryListingEntry(entry.name, std::string(), true,
                                        0, entry.last_modified));
      break;
    case net::FtpDirectoryListingEntry::SYMLINK:
      response_buffer_.append(
          net::GetDirectoryListingEntry(entry.name, std::string(), false,
                                        0, entry.last_modified));
      break;
    default:
      NOTREACHED();
      break;
  }
}

void FtpDirectoryListingResponseDelegate::SendResponseBufferToClient() {
  if (!response_buffer_.empty()) {
    client_->didReceiveData(loader_, response_buffer_.data(),
                            response_buffer_.length());
    response_buffer_.clear();
  }
}

}  // namespace webkit_glue
