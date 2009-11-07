// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A delegate class of WebURLLoaderImpl that handles text/vnd.chromium.ftp-dir
// data.

#ifndef WEBKIT_GLUE_FTP_DIRECTORY_LISTING_RESPONSE_DELEGATE_H_
#define WEBKIT_GLUE_FTP_DIRECTORY_LISTING_RESPONSE_DELEGATE_H_

#include <string>

#include "net/ftp/ftp_directory_listing_buffer.h"
#include "net/third_party/parseftp/ParseFTPList.h"
#include "webkit/api/public/WebURLResponse.h"

namespace WebKit {
class WebURLLoader;
class WebURLLoaderClient;
}

namespace webkit_glue {

class FtpDirectoryListingResponseDelegate {
 public:
  FtpDirectoryListingResponseDelegate(WebKit::WebURLLoaderClient* client,
                                      WebKit::WebURLLoader* loader,
                                      const WebKit::WebURLResponse& response);

  // Passed through from ResourceHandleInternal
  void OnReceivedData(const char* data, int data_len);
  void OnCompletedRequest();

 private:
  void Init();

  // Use the old parser to process received listing data.
  void FeedFallbackParser();

  void AppendEntryToResponseBuffer(const net::FtpDirectoryListingEntry& entry);

  void SendResponseBufferToClient();

  // Pointers to the client and associated loader so we can make callbacks as
  // we parse pieces of data.
  WebKit::WebURLLoaderClient* client_;
  WebKit::WebURLLoader* loader_;

  // The original resource response for this request.  We use this as a
  // starting point for each parts response.
  WebKit::WebURLResponse original_response_;

  // Data buffer also responsible for parsing the listing data (the new parser).
  // TODO(phajdan.jr): Use only the new parser, when it is more compatible.
  net::FtpDirectoryListingBuffer buffer_;

  // True if the new parser couldn't recognize the received listing format
  // and we switched to the old parser.
  bool parser_fallback_;

  // State kept between parsing each line of the response.
  struct net::list_state parse_state_;

  // Detected encoding of the response.
  std::string encoding_;

  // Buffer to hold not-yet-parsed input.
  std::string input_buffer_;

  // Buffer to hold response not-yet-sent to the caller.
  std::string response_buffer_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FTP_DIRECTORY_LISTING_RESPONSE_DELEGATE_H_
