// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DBUS_ASH_DBUS_HELPER_H_
#define ASH_DBUS_ASH_DBUS_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class Thread;
}

namespace dbus {
class Bus;
}

namespace ash {

// In Classic/SingleProcessMash, owns the dbus::Bus* provided by Chrome.
// In MultiProcessMash, creates and owns the dbus::Bus instance.
class AshDBusHelper {
 public:
  // Creates the helper with an existing dbus::Bus instance in single process
  // mode. If |bus| is null, fake dbus clients are being used and
  // |use_real_clients_| will be set to false.
  static std::unique_ptr<AshDBusHelper> CreateWithExistingBus(
      scoped_refptr<dbus::Bus> bus);

  // Creates the helper in multi process mode.
  static std::unique_ptr<AshDBusHelper> Create();

  ~AshDBusHelper();

  dbus::Bus* bus() { return bus_.get(); }
  bool use_real_clients() const { return use_real_clients_; }

 protected:
  explicit AshDBusHelper(bool use_real_clients);
  void InitializeDBus();

 private:
  // Set to false if fake dbus clients are being used (|bus_| will be null).
  const bool use_real_clients_;

  // The dbus::Bus instance provided or created (see comments above).
  scoped_refptr<dbus::Bus> bus_;

  // Thread required when a dbus::Bus instance is created.
  std::unique_ptr<base::Thread> dbus_thread_;

  DISALLOW_COPY_AND_ASSIGN(AshDBusHelper);
};

}  // namespace ash

#endif  // ASH_DBUS_ASH_DBUS_HELPER_H_
