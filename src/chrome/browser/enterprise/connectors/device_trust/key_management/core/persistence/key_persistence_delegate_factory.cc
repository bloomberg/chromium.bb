// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"

#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"

#if defined(OS_WIN)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/win_key_persistence_delegate.h"
#elif defined(OS_MAC)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mac_key_persistence_delegate.h"
#elif defined(OS_LINUX)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/linux_key_persistence_delegate.h"
#endif

namespace enterprise_connectors {

namespace {

absl::optional<KeyPersistenceDelegateFactory*>& GetTestInstanceStorage() {
  static absl::optional<KeyPersistenceDelegateFactory*> storage;
  return storage;
}

}  // namespace

// static
KeyPersistenceDelegateFactory* KeyPersistenceDelegateFactory::GetInstance() {
  absl::optional<KeyPersistenceDelegateFactory*>& test_instance =
      GetTestInstanceStorage();
  if (test_instance.has_value() && test_instance.value()) {
    return test_instance.value();
  }
  return base::Singleton<KeyPersistenceDelegateFactory>::get();
}

std::unique_ptr<KeyPersistenceDelegate>
KeyPersistenceDelegateFactory::CreateKeyPersistenceDelegate() {
#if defined(OS_WIN)
  return std::make_unique<WinKeyPersistenceDelegate>();
#elif defined(OS_MAC)
  return std::make_unique<MacKeyPersistenceDelegate>();
#elif defined(OS_LINUX)
  return std::make_unique<LinuxKeyPersistenceDelegate>();
#else
  NOTREACHED();
  return nullptr;
#endif
}

// static
void KeyPersistenceDelegateFactory::SetInstanceForTesting(
    KeyPersistenceDelegateFactory* factory) {
  DCHECK(factory);
  GetTestInstanceStorage().emplace(factory);
}

// static
void KeyPersistenceDelegateFactory::ClearInstanceForTesting() {
  GetTestInstanceStorage().reset();
}

}  // namespace enterprise_connectors
