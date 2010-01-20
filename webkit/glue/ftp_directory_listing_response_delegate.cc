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
#include "net/ftp/ftp_directory_listing_parser.h"
#include "net/ftp/ftp_server_type_histograms.h"
#include "unicode/ucsdet.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLLoaderClient.h"

using net::FtpDirectoryListingEntry;

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

}  // namespace

namespace webkit_glue {

FtpDirectoryListingResponseDelegate::FtpDirectoryListingResponseDelegate(
    WebURLLoaderClient* client,
    WebURLLoader* loader,
    const WebURLResponse& response)
    : client_(client),
      loader_(loader),
      original_response_(response),
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

  SendDataToClient(net::GetDirectoryListingHeader(path_utf16));

  // If this isn't top level directory (i.e. the path isn't "/",)
  // add a link to the parent directory.
  if (response_url.path().length() > 1) {
    SendDataToClient(net::GetDirectoryListingEntry(
        ASCIIToUTF16(".."), std::string(), false, 0, base::Time()));
  }
}

void FtpDirectoryListingResponseDelegate::ProcessReceivedEntries() {
  if (!updated_histograms_ && buffer_.EntryAvailable()) {
    // Only log the server type if we got enough data to reliably detect it.
    net::UpdateFtpServerTypeHistograms(buffer_.GetServerType());
    updated_histograms_ = true;
  }

  while (buffer_.EntryAvailable()) {
    FtpDirectoryListingEntry entry = buffer_.PopEntry();
    bool is_directory = (entry.type == FtpDirectoryListingEntry::DIRECTORY);
    int64 size = entry.size;
    if (entry.type != FtpDirectoryListingEntry::FILE)
      size = 0;
    SendDataToClient(net::GetDirectoryListingEntry(
        entry.name, std::string(), is_directory, size, entry.last_modified));
  }
}

void FtpDirectoryListingResponseDelegate::SendDataToClient(
    const std::string& data) {
  client_->didReceiveData(loader_, data.data(), data.length());
}

}  // namespace webkit_glue
