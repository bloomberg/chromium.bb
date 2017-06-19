// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StorageClientImpl_h
#define StorageClientImpl_h

#include <memory>
#include "modules/ModulesExport.h"
#include "modules/storage/StorageClient.h"

namespace blink {

class WebViewBase;

// TODO(sashab): Merge this into StorageClient.
class MODULES_EXPORT StorageClientImpl
    : NON_EXPORTED_BASE(public StorageClient) {
 public:
  explicit StorageClientImpl(WebViewBase*);

  std::unique_ptr<StorageNamespace> CreateSessionStorageNamespace() override;
  bool CanAccessStorage(LocalFrame*, StorageType) const override;

 private:
  WebViewBase* web_view_;
};

}  // namespace blink

#endif  // StorageClientImpl_h
