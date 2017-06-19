// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StorageClient_h
#define StorageClient_h

#include <memory>
#include "modules/ModulesExport.h"
#include "modules/storage/StorageArea.h"
#include "modules/storage/StorageClient.h"

namespace blink {

class StorageNamespace;
class WebViewBase;

class MODULES_EXPORT StorageClient {
 public:
  explicit StorageClient(WebViewBase*);
  ~StorageClient() {}

  std::unique_ptr<StorageNamespace> CreateSessionStorageNamespace();
  bool CanAccessStorage(LocalFrame*, StorageType) const;

 private:
  WebViewBase* web_view_;
};

}  // namespace blink

#endif  // StorageClient_h
