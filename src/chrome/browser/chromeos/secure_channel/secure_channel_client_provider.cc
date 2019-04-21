// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/secure_channel/secure_channel_client_provider.h"

#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client_impl.h"
#include "content/public/common/service_manager_connection.h"

namespace chromeos {

namespace secure_channel {

SecureChannelClientProvider::SecureChannelClientProvider() = default;

SecureChannelClientProvider::~SecureChannelClientProvider() = default;

// static
SecureChannelClientProvider* SecureChannelClientProvider::GetInstance() {
  static base::NoDestructor<SecureChannelClientProvider> provider;
  return provider.get();
}

SecureChannelClient* SecureChannelClientProvider::GetClient() {
  if (!secure_channel_client_) {
    // ServiceManagerConnection::GetForProcess() returns null in tests.
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()
            ? content::ServiceManagerConnection::GetForProcess()->GetConnector()
            : nullptr;

    secure_channel_client_ =
        SecureChannelClientImpl::Factory::Get()->BuildInstance(connector);
  }

  return secure_channel_client_.get();
}

}  // namespace secure_channel

}  // namespace chromeos
