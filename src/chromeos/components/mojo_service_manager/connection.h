// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MOJO_SERVICE_MANAGER_CONNECTION_H_
#define CHROMEOS_COMPONENTS_MOJO_SERVICE_MANAGER_CONNECTION_H_

#include "base/component_export.h"
#include "chromeos/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos::mojo_service_manager {

// Connects to the mojo service manager. Returns false if cannot connect.
// This will will block until finishes.
COMPONENT_EXPORT(CHROMEOS_MOJO_SERVICE_MANAGER)
bool BootstrapServiceManagerConnection();

// Returns whether connects to the mojo service manager.
COMPONENT_EXPORT(CHROMEOS_MOJO_SERVICE_MANAGER)
bool IsServiceManagerConnected();

// Resets the connection to the mojo service manager.
COMPONENT_EXPORT(CHROMEOS_MOJO_SERVICE_MANAGER)
void ResetServiceManagerConnection();

// Returns the interface to access the service manager.
COMPONENT_EXPORT(CHROMEOS_MOJO_SERVICE_MANAGER)
mojom::ServiceManagerProxy* GetServiceManagerProxy();

// Sets the mojo remote for testing.
COMPONENT_EXPORT(CHROMEOS_MOJO_SERVICE_MANAGER)
void SetServiceManagerRemoteForTesting(
    mojo::PendingRemote<mojom::ServiceManager> remote);

}  // namespace chromeos::mojo_service_manager

#endif  // CHROMEOS_COMPONENTS_MOJO_SERVICE_MANAGER_CONNECTION_H_
