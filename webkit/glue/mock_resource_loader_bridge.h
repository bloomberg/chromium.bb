// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_MOCK_RESOURCE_LOADER_BRIDGE_H_
#define WEBKIT_GLUE_MOCK_RESOURCE_LOADER_BRIDGE_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/glue/resource_loader_bridge.h"

class FilePath;

namespace webkit_glue {

class MockResourceLoaderBridge : public webkit_glue::ResourceLoaderBridge {
 public:
  MockResourceLoaderBridge() {
  }

  virtual ~MockResourceLoaderBridge() {
    OnDestroy();
  }

  MOCK_METHOD2(AppendDataToUpload, void(const char* data, int data_len));
  MOCK_METHOD4(AppendFileRangeToUpload,
               void(const FilePath& file_path,
                    uint64 offset,
                    uint64 length,
                    const base::Time& expected_modification_time));
  MOCK_METHOD1(AppendBlobToUpload, void(const GURL& blob_url));
  MOCK_METHOD1(SetUploadIdentifier, void(int64 identifier));
  MOCK_METHOD1(Start, bool(ResourceLoaderBridge::Peer* peer));
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD1(SetDefersLoading, void(bool value));
  MOCK_METHOD1(SyncLoad, void(SyncLoadResponse* response));
  MOCK_METHOD0(OnDestroy, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockResourceLoaderBridge);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MOCK_RESOURCE_LOADER_BRIDGE_H_
