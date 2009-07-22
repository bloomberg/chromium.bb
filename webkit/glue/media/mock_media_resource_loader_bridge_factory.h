// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_MOCK_RESOURCE_LOADER_BRIDGE_H_
#define WEBKIT_GLUE_MOCK_RESOURCE_LOADER_BRIDGE_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/glue/media/media_resource_loader_bridge_factory.h"

namespace webkit_glue {

class MockMediaResourceLoaderBridgeFactory
    : public webkit_glue::MediaResourceLoaderBridgeFactory {
 public:
  MockMediaResourceLoaderBridgeFactory() {
  }

  virtual ~MockMediaResourceLoaderBridgeFactory() {
    OnDestroy();
  }

  MOCK_METHOD4(CreateBridge,
               webkit_glue::ResourceLoaderBridge*(const GURL& url,
                                                  int load_flags,
                                                  int64 first_byte_position,
                                                  int64 last_byte_position));
  MOCK_METHOD0(OnDestroy, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaResourceLoaderBridgeFactory);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_MOCK_RESOURCE_LOADER_BRIDGE_H_
