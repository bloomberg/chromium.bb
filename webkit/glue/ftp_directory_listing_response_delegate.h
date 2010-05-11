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
#include "third_party/WebKit/WebKit/chromium/public/WebURLResponse.h"

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

  // Converts |filename| to detected server encoding and puts the result
  // in |raw_bytes| (if no conversion is necessary, an empty string is used).
  // Returns true on success.
  bool ConvertToServerEncoding(const string16& filename,
                               std::string* raw_bytes) const;

  // Fetches the listing entries from the buffer and sends them to the client.
  void ProcessReceivedEntries();

  void SendDataToClient(const std::string& data);

  // Pointers to the client and associated loader so we can make callbacks as
  // we parse pieces of data.
  WebKit::WebURLLoaderClient* client_;
  WebKit::WebURLLoader* loader_;

  // The original resource response for this request.  We use this as a
  // starting point for each parts response.
  WebKit::WebURLResponse original_response_;

  // Data buffer also responsible for parsing the listing data.
  net::FtpDirectoryListingBuffer buffer_;

  // True if we updated histogram data (we only want to do it once).
  bool updated_histograms_;

  // True if we got an error when parsing the response.
  bool had_parsing_error_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FTP_DIRECTORY_LISTING_RESPONSE_DELEGATE_H_
