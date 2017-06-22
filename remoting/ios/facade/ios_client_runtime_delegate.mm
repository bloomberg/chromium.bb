// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "remoting/ios/facade/ios_client_runtime_delegate.h"

#import "base/mac/bind_objc_block.h"
#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/sys_string_conversions.h"

namespace remoting {

IosClientRuntimeDelegate::IosClientRuntimeDelegate() : weak_factory_(this) {
  runtime_ = ChromotingClientRuntime::GetInstance();
}

IosClientRuntimeDelegate::~IosClientRuntimeDelegate() {}

void IosClientRuntimeDelegate::RuntimeWillShutdown() {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  // Nothing to do.
}

void IosClientRuntimeDelegate::RuntimeDidShutdown() {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  // Nothing to do.
}

void IosClientRuntimeDelegate::RequestAuthTokenForLogger() {
  if (!runtime_->ui_task_runner()->BelongsToCurrentThread()) {
    runtime_->ui_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&IosClientRuntimeDelegate::RequestAuthTokenForLogger,
                   base::Unretained(this)));
    return;
  }
  if ([RemotingService.instance.authentication.user isAuthenticated]) {
    [RemotingService.instance.authentication
        callbackWithAccessToken:^(RemotingAuthenticationStatus status,
                                  NSString* userEmail, NSString* accessToken) {
          if (status == RemotingAuthenticationStatusSuccess) {
            // Set the new auth token for the log writer on the network thread.
            std::string access_token = base::SysNSStringToUTF8(accessToken);
            runtime_->network_task_runner()->PostTask(
                FROM_HERE, base::BindBlockArc(^{
                  runtime_->log_writer()->SetAuthToken(access_token);
                }));
          } else {
            LOG(ERROR) << "Failed to fetch access token for log writer. ("
                       << status << ")";
          }
        }];
  }
}

base::WeakPtr<IosClientRuntimeDelegate> IosClientRuntimeDelegate::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
