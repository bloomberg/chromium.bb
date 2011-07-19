// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MOCKS_MOCK_WEBURLLOADER_H_
#define WEBKIT_MOCKS_MOCK_WEBURLLOADER_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/glue/weburlloader_impl.h"

namespace webkit_glue {

class MockWebURLLoader : public WebKit::WebURLLoader {
 public:
  MockWebURLLoader();
  virtual ~MockWebURLLoader();

  MOCK_METHOD4(loadSynchronously, void(const WebKit::WebURLRequest& request,
                                       WebKit::WebURLResponse& response,
                                       WebKit::WebURLError& error,
                                       WebKit::WebData& data));
  MOCK_METHOD2(loadAsynchronously, void(const WebKit::WebURLRequest& request,
                                        WebKit::WebURLLoaderClient* client));
  MOCK_METHOD0(cancel, void());
  MOCK_METHOD1(setDefersLoading, void(bool value));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebURLLoader);
};

}  // namespace webkit_glue

#endif  // WEBKIT_MOCKS_MOCK_WEBURLLOADER_H_
