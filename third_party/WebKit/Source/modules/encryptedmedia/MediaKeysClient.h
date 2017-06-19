// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaKeysClient_h
#define MediaKeysClient_h

#include "modules/ModulesExport.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ExecutionContext;
class WebEncryptedMediaClient;

class MODULES_EXPORT MediaKeysClient {
  DISALLOW_NEW();

 public:
  MediaKeysClient();
  ~MediaKeysClient() {}

  WebEncryptedMediaClient* EncryptedMediaClient(ExecutionContext*);
};

}  // namespace blink

#endif  // MediaKeysClient_h
