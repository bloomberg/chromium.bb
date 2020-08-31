// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/printers_service_provider.h"

#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_proxy.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

PrintersServiceProvider::PrintersServiceProvider() = default;

PrintersServiceProvider::~PrintersServiceProvider() = default;

void PrintersServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object_ = exported_object;
  auto* proxy = CupsPrintersManagerFactory::GetInstance()->GetProxy();
  DCHECK(proxy);
  printers_manager_observer_.Add(proxy);
}

void PrintersServiceProvider::OnPrintersChanged(
    PrinterClass printer_class,
    const std::vector<Printer>& /*printers*/) {
  // Signal is suppressed for discovered printers because they require setup
  // before being usable.
  if (printer_class == PrinterClass::kDiscovered) {
    return;
  }
  DVLOG(1) << "Emitting printers changed DBus event";
  EmitSignal();
}

void PrintersServiceProvider::EmitSignal() {
  DCHECK(exported_object_);

  dbus::Signal signal(chromeos::kPrintersServiceInterface,
                      chromeos::kPrintersServicePrintersChangedSignal);
  exported_object_->SendSignal(&signal);
}

}  // namespace chromeos
