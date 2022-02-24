// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_SERVICE_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace guest_os {

// A service to hold the subservices that make up the Guest OS API surface.
// NOTE: We don't start at browser startup, instead being created on-demand. At
// some point we may change that, but for now creating is cheap, we won't always
// be used (e.g. if the guest os flag isn't set), and we don't need to listen
// for events - everything we care about knows how to access us via KeyedService
// machinery.
class GuestOsService : public KeyedService {
 public:
  GuestOsService();
  ~GuestOsService() override;

  // Helper method to get the service instance for the given profile.
  static GuestOsService* GetForProfile(Profile* profile);
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_SERVICE_H_
