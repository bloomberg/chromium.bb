// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/lib/weak_service_provider.h"

#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"

namespace mojo {
namespace internal {

WeakServiceProvider::WeakServiceProvider(ServiceProviderImpl* creator,
                                         ServiceProvider* service_provider)
    : creator_(creator), service_provider_(service_provider) {
}

WeakServiceProvider::~WeakServiceProvider() {
  if (creator_)
    creator_->ClearRemote();
}

void WeakServiceProvider::Clear() {
  creator_ = nullptr;
  service_provider_ = nullptr;
}

void WeakServiceProvider::ConnectToService(
    const String& service_name,
    ScopedMessagePipeHandle client_handle) {
  if (service_provider_)
    service_provider_->ConnectToService(service_name, client_handle.Pass());
}

}  // namespace internal
}  // namespace mojo
