// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/synced_printers_manager.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/observer_list_threadsafe.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/printing/enterprise_printers_provider.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/printers_sync_bridge.h"
#include "chrome/browser/chromeos/printing/specifics_translation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

class SyncedPrintersManagerImpl : public SyncedPrintersManager,
                                  public PrintersSyncBridge::Observer,
                                  public EnterprisePrintersProvider::Observer {
 public:
  SyncedPrintersManagerImpl(Profile* profile,
                            std::unique_ptr<PrintersSyncBridge> sync_bridge)
      : profile_(profile),
        sync_bridge_(std::move(sync_bridge)),
        observers_(new base::ObserverListThreadSafe<
                   SyncedPrintersManager::Observer>()),
        weak_factory_(this) {
    printers_provider_ =
        EnterprisePrintersProvider::Create(CrosSettings::Get(), profile_);
    printers_provider_->AddObserver(this);
    sync_bridge_->AddObserver(this);
  }

  ~SyncedPrintersManagerImpl() override {
    printers_provider_->RemoveObserver(this);
    sync_bridge_->RemoveObserver(this);
  }

  std::vector<Printer> GetSavedPrinters() const override {
    // No need to lock here, since sync_bridge_ is thread safe and we don't
    // touch anything else.
    std::vector<Printer> printers;
    std::vector<sync_pb::PrinterSpecifics> values =
        sync_bridge_->GetAllPrinters();
    for (const auto& value : values) {
      printers.push_back(*SpecificsToPrinter(value));
    }
    return printers;
  }

  bool GetEnterprisePrinters(std::vector<Printer>* printers) const override {
    base::AutoLock l(lock_);
    if (printers != nullptr)
      *printers = GetEnterprisePrintersLocked();
    return enterprise_printers_are_ready_;
  }

  std::unique_ptr<Printer> GetPrinter(
      const std::string& printer_id) const override {
    base::AutoLock l(lock_);
    return GetPrinterLocked(printer_id);
  }

  void UpdateSavedPrinter(const Printer& printer) override {
    base::AutoLock l(lock_);
    UpdateSavedPrinterLocked(printer);
  }

  bool RemoveSavedPrinter(const std::string& printer_id) override {
    return sync_bridge_->RemovePrinter(printer_id);
  }

  void AddObserver(SyncedPrintersManager::Observer* observer) override {
    observers_->AddObserver(observer);
  }

  void RemoveObserver(SyncedPrintersManager::Observer* observer) override {
    observers_->RemoveObserver(observer);
  }

  void PrinterInstalled(const Printer& printer) override {
    base::AutoLock l(lock_);
    installed_printer_fingerprints_[printer.id()] =
        PrinterConfigurer::SetupFingerprint(printer);

    // Register this printer if it's the first time we're using it.
    if (!IsPrinterRegistered(printer.id())) {
      UpdateSavedPrinterLocked(printer);
    }
  }

  bool IsConfigurationCurrent(const Printer& printer) const override {
    base::AutoLock l(lock_);
    auto found = installed_printer_fingerprints_.find(printer.id());
    if (found == installed_printer_fingerprints_.end())
      return false;

    return found->second == PrinterConfigurer::SetupFingerprint(printer);
  }

  PrintersSyncBridge* GetSyncBridge() override { return sync_bridge_.get(); }

  // PrintersSyncBridge::Observer override.
  void OnPrintersUpdated() override {
    observers_->Notify(
        FROM_HERE, &SyncedPrintersManager::Observer::OnSavedPrintersChanged);
  }

  // EnterprisePrintersProvider::Observer override
  void OnPrintersChanged(
      bool complete,
      const std::unordered_map<std::string, Printer>& printers) override {
    // Enterprise printers policy changed.  Update the lists.
    base::AutoLock l(lock_);
    enterprise_printers_ = printers;
    enterprise_printers_are_ready_ = complete;
    observers_->Notify(
        FROM_HERE,
        &SyncedPrintersManager::Observer::OnEnterprisePrintersChanged);
  }

 private:
  std::unique_ptr<Printer> GetPrinterLocked(
      const std::string& printer_id) const {
    lock_.AssertAcquired();
    // check for a policy printer first
    auto found = enterprise_printers_.find(printer_id);
    if (found != enterprise_printers_.end()) {
      // Copy a printer.
      return std::make_unique<Printer>(found->second);
    }

    base::Optional<sync_pb::PrinterSpecifics> printer =
        sync_bridge_->GetPrinter(printer_id);
    return printer.has_value() ? SpecificsToPrinter(*printer) : nullptr;
  }

  // Determines whether or not the printer with the given |printer_id| has
  // already been registered.
  bool IsPrinterRegistered(const std::string& printer_id) {
    return enterprise_printers_.find(printer_id) !=
               enterprise_printers_.end() ||
           sync_bridge_->HasPrinter(printer_id);
  }

  void UpdateSavedPrinterLocked(const Printer& printer_arg) {
    lock_.AssertAcquired();
    DCHECK_EQ(Printer::SRC_USER_PREFS, printer_arg.source());

    // Need a local copy since we may set the id.
    Printer printer = printer_arg;
    if (printer.id().empty()) {
      printer.set_id(base::GenerateGUID());
    }

    sync_bridge_->UpdatePrinter(PrinterToSpecifics(printer));
  }

  std::vector<Printer> GetEnterprisePrintersLocked() const {
    lock_.AssertAcquired();
    std::vector<Printer> ret;
    ret.reserve(enterprise_printers_.size());
    for (auto kv : enterprise_printers_) {
      ret.push_back(kv.second);
    }
    return ret;
  }

  mutable base::Lock lock_;

  Profile* profile_;

  // The backend for profile printers.
  std::unique_ptr<PrintersSyncBridge> sync_bridge_;

  // The Object that provides updates about enterprise printers.
  std::unique_ptr<EnterprisePrintersProvider> printers_provider_;

  // Enterprise printers as of the last time we got a policy update.
  std::unordered_map<std::string, Printer> enterprise_printers_;
  // This flag is set to true if all enterprise policies were loaded.
  bool enterprise_printers_are_ready_ = false;

  // Map of printer ids to PrinterConfigurer setup fingerprints at the time
  // the printers was last installed with CUPS.
  std::map<std::string, std::string> installed_printer_fingerprints_;

  scoped_refptr<base::ObserverListThreadSafe<SyncedPrintersManager::Observer>>
      observers_;
  base::WeakPtrFactory<SyncedPrintersManagerImpl> weak_factory_;
};

}  // namespace

// static
void SyncedPrintersManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  EnterprisePrintersProvider::RegisterProfilePrefs(registry);
}

// static
std::unique_ptr<SyncedPrintersManager> SyncedPrintersManager::Create(
    Profile* profile,
    std::unique_ptr<PrintersSyncBridge> sync_bridge) {
  return std::make_unique<SyncedPrintersManagerImpl>(profile,
                                                     std::move(sync_bridge));
}

}  // namespace chromeos
