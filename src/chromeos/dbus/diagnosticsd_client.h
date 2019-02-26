// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DIAGNOSTICSD_CLIENT_H_
#define CHROMEOS_DBUS_DIAGNOSTICSD_CLIENT_H_

#include <memory>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "dbus/object_proxy.h"

namespace chromeos {

class CHROMEOS_EXPORT DiagnosticsdClient : public DBusClient {
 public:
  // Factory function.
  static std::unique_ptr<DiagnosticsdClient> Create();

  // Registers |callback| to run when the diagnosticsd service becomes
  // available.
  virtual void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) = 0;

  // Bootstrap the Mojo connection between Chrome and the diagnosticsd daemon.
  // |fd| is the file descriptor with the child end of the Mojo pipe.
  virtual void BootstrapMojoConnection(base::ScopedFD fd,
                                       VoidDBusMethodCallback callback) = 0;

 protected:
  // Create() should be used instead.
  DiagnosticsdClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DIAGNOSTICSD_CLIENT_H_
