// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/ftp_directory_listing_response_delegate.h"

#include <vector>

#include "base/i18n/icu_encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/time.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/ftp/ftp_directory_listing_parser.h"
#include "net/ftp/ftp_server_type_histograms.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLLoaderClient.h"

using net::FtpDirectoryListingEntry;

using WebKit::WebURLLoader;
using WebKit::WebURLLoaderClient;
using WebKit::WebURLResponse;

namespace {

string16 ConvertPathToUTF16(const std::string& path) {
  // Per RFC 2640, FTP servers should use UTF-8 or its proper subset ASCII,
  // but many old FTP servers use legacy encodings. Try UTF-8 first.
  if (IsStringUTF8(path))
    return UTF8ToUTF16(path);

  // Try detecting the encoding. The sample is rather small though, so it may
  // fail.
  std::string encoding;
  if (base::DetectEncoding(path, &encoding) && !encoding.empty()) {
    string16 path_utf16;
    if (base::CodepageToUTF16(path, encoding.c_str(),
                              base::OnStringConversionError::SUBSTITUTE,
                              &path_utf16)) {
      return path_utf16;
    }
  }

  // Use system native encoding as the last resort.
  return WideToUTF16Hack(base::SysNativeMBToWide(path));
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
      buffer_(base::Time::Now()),
      updated_histograms_(false),
      had_parsing_error_(false) {
  Init();
}

void FtpDirectoryListingResponseDelegate::OnReceivedData(const char* data,
                                                         int data_len) {
  if (had_parsing_error_)
    return;

  if (buffer_.ConsumeData(data, data_len) == net::OK)
    ProcessReceivedEntries();
  else
    had_parsing_error_ = true;
}

void FtpDirectoryListingResponseDelegate::OnCompletedRequest() {
  if (!had_parsing_error_ && buffer_.ProcessRemainingData() == net::OK)
    ProcessReceivedEntries();
  else
    had_parsing_error_ = true;

  if (had_parsing_error_)
    SendDataToClient("<script>onListingParsingError();</script>\n");
}

void FtpDirectoryListingResponseDelegate::Init() {
  GURL response_url(original_response_.url());
  UnescapeRule::Type unescape_rules = UnescapeRule::SPACES |
                                      UnescapeRule::URL_SPECIAL_CHARS;
  std::string unescaped_path = UnescapeURLComponent(response_url.path(),
                                                    unescape_rules);
  SendDataToClient(net::GetDirectoryListingHeader(
                       ConvertPathToUTF16(unescaped_path)));

  // If this isn't top level directory (i.e. the path isn't "/",)
  // add a link to the parent directory.
  if (response_url.path().length() > 1) {
    SendDataToClient(net::GetDirectoryListingEntry(
        ASCIIToUTF16(".."), std::string(), false, 0, base::Time()));
  }
}

bool FtpDirectoryListingResponseDelegate::ConvertToServerEncoding(
    const string16& filename, std::string* raw_bytes) const {
  if (buffer_.encoding().empty()) {
    *raw_bytes = std::string();
    return true;
  }

  return base::UTF16ToCodepage(filename, buffer_.encoding().c_str(),
                               base::OnStringConversionError::FAIL,
                               raw_bytes);
}

void FtpDirectoryListingResponseDelegate::ProcessReceivedEntries() {
  if (!updated_histograms_ && buffer_.EntryAvailable()) {
    // Only log the server type if we got enough data to reliably detect it.
    net::UpdateFtpServerTypeHistograms(buffer_.GetServerType());
    updated_histograms_ = true;
  }

  while (buffer_.EntryAvailable()) {
    FtpDirectoryListingEntry entry = buffer_.PopEntry();

    // Skip the current and parent directory entries in the listing. Our header
    // always includes them.
    if (EqualsASCII(entry.name, ".") || EqualsASCII(entry.name, ".."))
      continue;

    bool is_directory = (entry.type == FtpDirectoryListingEntry::DIRECTORY);
    int64 size = entry.size;
    if (entry.type != FtpDirectoryListingEntry::FILE)
      size = 0;
    std::string raw_bytes;
    if (ConvertToServerEncoding(entry.name, &raw_bytes)) {
      SendDataToClient(net::GetDirectoryListingEntry(
          entry.name, raw_bytes, is_directory, size, entry.last_modified));
    } else {
      // Consider an encoding problem a non-fatal error. The server's support
      // for non-ASCII characters might be buggy. Display an error message,
      // but keep trying to display the rest of the listing (most file names
      // are ASCII anyway, we could be just unlucky with this one).
      had_parsing_error_ = true;
    }
  }
}

void FtpDirectoryListingResponseDelegate::SendDataToClient(
    const std::string& data) {
  client_->didReceiveData(loader_, data.data(), data.length());
}

}  // namespace webkit_glue
