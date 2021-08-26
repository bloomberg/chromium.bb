// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_LOCAL_PRINTER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_LOCAL_PRINTER_ASH_H_

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/print_servers_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;
class ProfileManager;

namespace chromeos {
class CupsPrinterStatus;
class PpdProvider;
class Printer;
class PrinterConfigurer;
struct PrintServersConfig;
}  // namespace chromeos

namespace crosapi {

// Implements the crosapi interface for LocalPrinter. Lives in Ash-Chrome on the
// UI thread.
class LocalPrinterAsh : public mojom::LocalPrinter,
                        public ProfileManagerObserver,
                        public chromeos::CupsPrintJobManager::Observer,
                        public chromeos::PrintServersManager::Observer {
 public:
  LocalPrinterAsh();
  LocalPrinterAsh(const LocalPrinterAsh&) = delete;
  LocalPrinterAsh& operator=(const LocalPrinterAsh&) = delete;

  // As it observes browser context keyed services, this object must
  // be destroyed after said services are destroyed (which occurs in
  // ChromeBrowserMainParts::PostMainMessageLoopRun()). CrosapiAsh
  // is destroyed in ~ChromeBrowserMainParts() which occurs after
  // all browser context keyed services have been destroyed.
  ~LocalPrinterAsh() override;

  // The mojom PrintServersConfig object contains all information in the
  // PrintServersConfig object.
  static mojom::PrintServersConfigPtr ConfigToMojom(
      const chromeos::PrintServersConfig& config);

  // The mojom LocalDestinationInfo object is a subset of the chromeos Printer
  // object.
  static mojom::LocalDestinationInfoPtr PrinterToMojom(
      const chromeos::Printer& printer);

  // The mojom PrinterStatus object contains all information in the
  // CupsPrinterStatus object.
  static mojom::PrinterStatusPtr StatusToMojom(
      const chromeos::CupsPrinterStatus& status);

  void BindReceiver(mojo::PendingReceiver<mojom::LocalPrinter> receiver);

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // chromeos::CupsPrintJobManager::Observer:
  void OnPrintJobCreated(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobStarted(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobUpdated(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobSuspended(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobResumed(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobDone(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobError(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobCancelled(base::WeakPtr<chromeos::CupsPrintJob> job) override;

  // chromeos::PrintServersManager::Observer:
  void OnPrintServersChanged(
      const chromeos::PrintServersConfig& config) override;
  void OnServerPrintersChanged(
      const std::vector<chromeos::PrinterDetector::DetectedPrinter>&) override;

  // crosapi::mojom::LocalPrinter:
  void GetPrinters(GetPrintersCallback callback) override;
  void GetCapability(const std::string& printer_id,
                     GetCapabilityCallback callback) override;
  void GetEulaUrl(const std::string& printer_id,
                  GetEulaUrlCallback callback) override;
  void GetStatus(const std::string& printer_id,
                 GetStatusCallback callback) override;
  void ShowSystemPrintSettings(
      ShowSystemPrintSettingsCallback callback) override;
  void CreatePrintJob(mojom::PrintJobPtr job,
                      CreatePrintJobCallback callback) override;
  void CancelPrintJob(const std::string& printer_id,
                      unsigned int job_id,
                      CancelPrintJobCallback callback) override;
  void GetPrintServersConfig(GetPrintServersConfigCallback callback) override;
  void ChoosePrintServers(const std::vector<std::string>& print_server_ids,
                          ChoosePrintServersCallback callback) override;
  void AddPrintServerObserver(
      mojo::PendingRemote<mojom::PrintServerObserver> remote,
      AddPrintServerObserverCallback callback) override;
  void GetPolicies(GetPoliciesCallback callback) override;
  void GetUsernamePerPolicy(GetUsernamePerPolicyCallback callback) override;
  void GetPrinterTypeDenyList(GetPrinterTypeDenyListCallback callback) override;
  void AddPrintJobObserver(mojo::PendingRemote<mojom::PrintJobObserver> remote,
                           mojom::PrintJobSource source,
                           AddPrintJobObserverCallback callback) override;

 private:
  void NotifyPrintJobUpdate(base::WeakPtr<chromeos::CupsPrintJob> job,
                            mojom::PrintJobStatus status);

  // Exposed so that unit tests can override them.
  virtual Profile* GetProfile();
  virtual scoped_refptr<chromeos::PpdProvider> CreatePpdProvider(
      Profile* profile);
  virtual std::unique_ptr<chromeos::PrinterConfigurer> CreatePrinterConfigurer(
      Profile* profile);

  ProfileManager* profile_manager_ = nullptr;

  bool observers_registered_ = false;

  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::LocalPrinter> receivers_;

  mojo::RemoteSet<mojom::PrintServerObserver> print_server_remotes_;

  // Remotes which observe all print jobs.
  mojo::RemoteSet<mojom::PrintJobObserver> print_job_remotes_;

  // Remotes which observe only extension print jobs.
  mojo::RemoteSet<mojom::PrintJobObserver> extension_print_job_remotes_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_LOCAL_PRINTER_ASH_H_
