// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_default.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_features.h"

#if defined(OS_MAC)
#include "chrome/common/printing/printer_capabilities_mac.h"
#endif

#if defined(OS_WIN)
#include "base/threading/thread_restrictions.h"
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#endif

namespace printing {

namespace {

scoped_refptr<base::TaskRunner> CreatePrinterHandlerTaskRunner() {
  // USER_VISIBLE because the result is displayed in the print preview dialog.
#if !defined(OS_WIN)
  static constexpr base::TaskTraits kTraits = {
      base::MayBlock(), base::TaskPriority::USER_VISIBLE};
#endif

#if defined(USE_CUPS)
  // CUPS is thread safe.
  return base::ThreadPool::CreateTaskRunner(kTraits);
#elif defined(OS_WIN)
  // Windows drivers are likely not thread-safe and need to be accessed on the
  // UI thread.
  return content::GetUIThreadTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
#else
  // Be conservative on unsupported platforms.
  return base::ThreadPool::CreateSingleThreadTaskRunner(kTraits);
#endif
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)

void OnDidGetDefaultPrinterName(
    PrinterHandler::DefaultPrinterCallback callback,
    mojom::DefaultPrinterNameResultPtr printer_name) {
  if (printer_name->is_result_code()) {
    PRINTER_LOG(ERROR) << "Failure getting default printer, result: "
                       << printer_name->get_result_code();
    std::move(callback).Run(std::string());
    return;
  }

  VLOG(1) << "Default Printer: " << printer_name->get_default_printer_name();
  std::move(callback).Run(printer_name->get_default_printer_name());
}

void OnDidEnumeratePrinters(
    PrinterHandler::AddedPrintersCallback added_printers_callback,
    PrinterHandler::GetPrintersDoneCallback done_callback,
    mojom::PrinterListResultPtr printer_list) {
  if (printer_list->is_result_code()) {
    PRINTER_LOG(ERROR) << "Failure enumerating local printers, result: "
                       << printer_list->get_result_code();
  }

  ConvertPrinterListForCallback(
      std::move(added_printers_callback), std::move(done_callback),
      printer_list->is_printer_list() ? printer_list->get_printer_list()
                                      : PrinterList());
}

void OnDidFetchCapabilities(
    const std::string& device_name,
    bool elevated_privileges,
    bool has_secure_protocol,
    PrinterHandler::GetCapabilityCallback callback,
    mojom::PrinterCapsAndInfoResultPtr printer_caps_and_info) {
  if (printer_caps_and_info->is_result_code()) {
    LOG(WARNING) << "Failure fetching printer capabilities for " << device_name
                 << " - error " << printer_caps_and_info->get_result_code();

    // If we failed because of access denied then we could retry at an elevated
    // privilege (if not already elevated).
    if (printer_caps_and_info->get_result_code() ==
            mojom::ResultCode::kAccessDenied &&
        !elevated_privileges) {
      // Register that this printer requires elevated privileges.
      PrintBackendServiceManager& service_mgr =
          PrintBackendServiceManager::GetInstance();
      service_mgr.SetPrinterDriverRequiresElevatedPrivilege(device_name);

      // Retry the operation which should now happen at a higher privilege
      // level.
      service_mgr.FetchCapabilities(
          device_name,
          base::BindOnce(&OnDidFetchCapabilities, device_name,
                         /*elevated_privileges=*/true, has_secure_protocol,
                         std::move(callback)));
      return;
    }

    // Unable to fallback, call back without data.
    std::move(callback).Run(base::Value());
    return;
  }

  VLOG(1) << "Received printer info & capabilities for " << device_name;
  const mojom::PrinterCapsAndInfoPtr& caps_and_info =
      printer_caps_and_info->get_printer_caps_and_info();
  base::Value settings = AssemblePrinterSettings(
      device_name, caps_and_info->printer_info,
      caps_and_info->user_defined_papers, has_secure_protocol,
      &caps_and_info->printer_caps);
  std::move(callback).Run(std::move(settings));
}

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

}  // namespace

// static
PrinterList LocalPrinterHandlerDefault::EnumeratePrintersAsync(
    const std::string& locale) {
#if defined(OS_WIN)
  // Blocking is needed here because Windows printer drivers are oftentimes
  // not thread-safe and have to be accessed on the UI thread.
  base::ScopedAllowBlocking allow_blocking;
#endif

  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(locale));

  PrinterList printer_list;
  mojom::ResultCode result = print_backend->EnumeratePrinters(&printer_list);
  if (result != mojom::ResultCode::kSuccess) {
    PRINTER_LOG(ERROR) << "Failure enumerating local printers, result: "
                       << result;
  }
  return printer_list;
}

// static
base::Value LocalPrinterHandlerDefault::FetchCapabilitiesAsync(
    const std::string& device_name,
    const std::string& locale) {
  PrinterSemanticCapsAndDefaults::Papers user_defined_papers;
#if defined(OS_MAC)
  user_defined_papers = GetMacCustomPaperSizes();
#endif

#if defined(OS_WIN)
  // Blocking is needed here because Windows printer drivers are oftentimes
  // not thread-safe and have to be accessed on the UI thread.
  base::ScopedAllowBlocking allow_blocking;
#endif

  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(locale));

  VLOG(1) << "Get printer capabilities start for " << device_name;

  PrinterBasicInfo basic_info;
  if (print_backend->GetPrinterBasicInfo(device_name, &basic_info) !=
      mojom::ResultCode::kSuccess) {
    LOG(WARNING) << "Invalid printer " << device_name;
    return base::Value();
  }

  return GetSettingsOnBlockingTaskRunner(
      device_name, basic_info, std::move(user_defined_papers),
      /*has_secure_protocol=*/false, print_backend);
}

// static
std::string LocalPrinterHandlerDefault::GetDefaultPrinterAsync(
    const std::string& locale) {
#if defined(OS_WIN)
  // Blocking is needed here because Windows printer drivers are oftentimes
  // not thread-safe and have to be accessed on the UI thread.
  base::ScopedAllowBlocking allow_blocking;
#endif

  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(locale));

  std::string default_printer;
  mojom::ResultCode result =
      print_backend->GetDefaultPrinterName(default_printer);
  if (result != mojom::ResultCode::kSuccess) {
    PRINTER_LOG(ERROR) << "Failure getting default printer name, result: "
                       << result;
    return std::string();
  }
  VLOG(1) << "Default Printer: " << default_printer;
  return default_printer;
}

LocalPrinterHandlerDefault::LocalPrinterHandlerDefault(
    content::WebContents* preview_web_contents)
    : preview_web_contents_(preview_web_contents),
      task_runner_(CreatePrinterHandlerTaskRunner()) {}

LocalPrinterHandlerDefault::~LocalPrinterHandlerDefault() {}

void LocalPrinterHandlerDefault::Reset() {}

void LocalPrinterHandlerDefault::GetDefaultPrinter(DefaultPrinterCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (base::FeatureList::IsEnabled(features::kEnableOopPrintDrivers)) {
    VLOG(1) << "Getting default printer via service";
    PrintBackendServiceManager& service_mgr =
        PrintBackendServiceManager::GetInstance();
    service_mgr.GetDefaultPrinterName(
        base::BindOnce(&OnDidGetDefaultPrinterName, std::move(cb)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  VLOG(1) << "Getting default printer in-process";
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&GetDefaultPrinterAsync,
                     g_browser_process->GetApplicationLocale()),
      std::move(cb));
}

void LocalPrinterHandlerDefault::StartGetPrinters(
    AddedPrintersCallback callback,
    GetPrintersDoneCallback done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (base::FeatureList::IsEnabled(features::kEnableOopPrintDrivers)) {
    VLOG(1) << "Enumerate printers start via service";
    PrintBackendServiceManager& service_mgr =
        PrintBackendServiceManager::GetInstance();
    service_mgr.EnumeratePrinters(
        base::BindOnce(&OnDidEnumeratePrinters, std::move(callback),
                       std::move(done_callback)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  VLOG(1) << "Enumerate printers start in-process";
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&EnumeratePrintersAsync,
                     g_browser_process->GetApplicationLocale()),
      base::BindOnce(&ConvertPrinterListForCallback, std::move(callback),
                     std::move(done_callback)));
}

void LocalPrinterHandlerDefault::StartGetCapability(
    const std::string& device_name,
    GetCapabilityCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (base::FeatureList::IsEnabled(features::kEnableOopPrintDrivers)) {
    VLOG(1) << "Getting printer capabilities via service for " << device_name;
    PrintBackendServiceManager& service_mgr =
        PrintBackendServiceManager::GetInstance();
    service_mgr.FetchCapabilities(
        device_name,
        base::BindOnce(
            &OnDidFetchCapabilities, device_name,
            service_mgr.PrinterDriverRequiresElevatedPrivilege(device_name),
            /*has_secure_protocol=*/false, std::move(cb)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  VLOG(1) << "Getting printer capabilities in-process for " << device_name;
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&FetchCapabilitiesAsync, device_name,
                     g_browser_process->GetApplicationLocale()),
      std::move(cb));
}

void LocalPrinterHandlerDefault::StartPrint(
    const std::u16string& job_title,
    base::Value settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrintCallback callback) {
  StartLocalPrint(std::move(settings), std::move(print_data),
                  preview_web_contents_, std::move(callback));
}

}  // namespace printing
