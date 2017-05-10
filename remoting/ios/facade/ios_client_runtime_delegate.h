// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_FACADE_IOS_CLIENT_RUNTIME_DELEGATE_H_
#define REMOTING_IOS_FACADE_IOS_CLIENT_RUNTIME_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/chromoting_client_runtime.h"

namespace remoting {

class IosClientRuntimeDelegate : public ChromotingClientRuntime::Delegate {
 public:
  IosClientRuntimeDelegate();
  ~IosClientRuntimeDelegate() override;

  // remoting::ChromotingClientRuntime::Delegate overrides.
  void RuntimeWillShutdown() override;
  void RuntimeDidShutdown() override;
  void RequestAuthTokenForLogger() override;

  base::WeakPtr<IosClientRuntimeDelegate> GetWeakPtr();

 private:
  ChromotingClientRuntime* runtime_;

  base::WeakPtrFactory<IosClientRuntimeDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IosClientRuntimeDelegate);
};

}  // namespace remoting

#endif  // REMOTING_IOS_FACADE_CLIENT_RUNTIME_DELEGATE_H_
