// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_HIBERMAN_FAKE_HIBERMAN_CLIENT_H_
#define CHROMEOS_DBUS_HIBERMAN_FAKE_HIBERMAN_CLIENT_H_

#include "chromeos/dbus/hiberman/hiberman_client.h"

#include <vector>

#include "base/component_export.h"

namespace chromeos {

// Class which satisfies the implementation of a HibermanClient but does not
// actually wire up to dbus. Used in testing.
class COMPONENT_EXPORT(HIBERMAN_CLIENT) FakeHibermanClient
    : public HibermanClient {
 public:
  FakeHibermanClient();
  ~FakeHibermanClient() override;

  // Not copyable or movable.
  FakeHibermanClient(const FakeHibermanClient&) = delete;
  FakeHibermanClient& operator=(const FakeHibermanClient&) = delete;

  // Checks that a FakeHibermanClient instance was initialized and returns
  // it.
  static FakeHibermanClient* Get();

  // HibermanClient override:
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void ResumeFromHibernate(const std::string& account_id,
                           ResumeFromHibernateCallback callback) override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::FakeHibermanClient;
}

#endif  // CHROMEOS_DBUS_HIBERMAN_FAKE_HIBERMAN_CLIENT_H_
