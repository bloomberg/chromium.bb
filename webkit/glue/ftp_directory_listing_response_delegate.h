// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A delegate class of WebURLLoaderImpl that handles text/vnd.chromium.ftp-dir
// data.

#ifndef WEBKIT_GLUE_FTP_DIRECTORY_LISTING_RESPONSE_DELEGATE_H_
#define WEBKIT_GLUE_FTP_DIRECTORY_LISTING_RESPONSE_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLResponse.h"

namespace WebKit {
class WebURLLoader;
class WebURLLoaderClient;
}

class GURL;

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
  void Init(const GURL& response_url);

  void SendDataToClient(const std::string& data);

  // Pointers to the client and associated loader so we can make callbacks as
  // we parse pieces of data.
  WebKit::WebURLLoaderClient* client_;
  WebKit::WebURLLoader* loader_;

  // Buffer for data received from the network.
  std::string buffer_;

  DISALLOW_COPY_AND_ASSIGN(FtpDirectoryListingResponseDelegate);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_FTP_DIRECTORY_LISTING_RESPONSE_DELEGATE_H_
