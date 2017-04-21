// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "remoting/client/ios/facade/ios_client_runtime_delegate.h"

#import "base/mac/bind_objc_block.h"
#import "remoting/client/ios/facade/remoting_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"

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
  // TODO(nicholss): Need to work out how to provide the logger with auth token
  // at the correct time in the app. This was hanging the app bootup. Removing
  // for now but this needs to happen the correct way soon.
  // if ([[RemotingService SharedInstance] getUser]) {
  //   [[RemotingService SharedInstance]
  //   callbackWithAccessToken:base::BindBlockArc(
  //       ^(remoting::OAuthTokenGetter::Status status,
  //         const std::string& user_email, const std::string& access_token) {
  //         // TODO(nicholss): Check status.
  //           runtime_->log_writer()->SetAuthToken(access_token);
  //       })];
  // }
}

base::WeakPtr<IosClientRuntimeDelegate> IosClientRuntimeDelegate::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
