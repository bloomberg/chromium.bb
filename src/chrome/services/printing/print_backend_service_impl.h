// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_
#define CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "printing/printing_context.h"
#include "ui/gfx/native_widget_types.h"

#if !BUILDFLAG(ENABLE_OOP_PRINTING)
#error "Out-of-process printing must be enabled."
#endif

namespace crash_keys {
class ScopedPrinterInfo;
}

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace printing {

class SandboxedPrintBackendHostImpl : public mojom::SandboxedPrintBackendHost {
 public:
  explicit SandboxedPrintBackendHostImpl(
      mojo::PendingReceiver<mojom::SandboxedPrintBackendHost> receiver);
  SandboxedPrintBackendHostImpl(const SandboxedPrintBackendHostImpl&) = delete;
  SandboxedPrintBackendHostImpl& operator=(
      const SandboxedPrintBackendHostImpl&) = delete;
  ~SandboxedPrintBackendHostImpl() override;

 private:
  // mojom::SandboxedPrintBackendHost
  void BindBackend(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver) override;

  mojo::Receiver<mojom::SandboxedPrintBackendHost> receiver_;
  std::unique_ptr<mojom::PrintBackendService> print_backend_service_;
};

class UnsandboxedPrintBackendHostImpl
    : public mojom::UnsandboxedPrintBackendHost {
 public:
  explicit UnsandboxedPrintBackendHostImpl(
      mojo::PendingReceiver<mojom::UnsandboxedPrintBackendHost> receiver);
  UnsandboxedPrintBackendHostImpl(const UnsandboxedPrintBackendHostImpl&) =
      delete;
  UnsandboxedPrintBackendHostImpl& operator=(
      const UnsandboxedPrintBackendHostImpl&) = delete;
  ~UnsandboxedPrintBackendHostImpl() override;

 private:
  // mojom::UnsandboxedPrintBackendHost
  void BindBackend(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver) override;

  mojo::Receiver<mojom::UnsandboxedPrintBackendHost> receiver_;
  std::unique_ptr<mojom::PrintBackendService> print_backend_service_;
};

class PrintBackendServiceImpl : public mojom::PrintBackendService {
 public:
  explicit PrintBackendServiceImpl(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver);
  PrintBackendServiceImpl(const PrintBackendServiceImpl&) = delete;
  PrintBackendServiceImpl& operator=(const PrintBackendServiceImpl&) = delete;
  ~PrintBackendServiceImpl() override;

 private:
  friend class PrintBackendServiceTestImpl;

  class DocumentHelper;

  class PrintingContextDelegate : public PrintingContext::Delegate {
   public:
    PrintingContextDelegate();
    PrintingContextDelegate(const PrintingContextDelegate&) = delete;
    PrintingContextDelegate& operator=(const PrintingContextDelegate&) = delete;
    ~PrintingContextDelegate() override;

    // PrintingContext::Delegate overrides:
    gfx::NativeView GetParentView() override;
    std::string GetAppLocale() override;

#if BUILDFLAG(IS_WIN)
    void SetParentWindow(uint32_t parent_window_id);
#endif
    void SetAppLocale(const std::string& locale);

   private:
#if BUILDFLAG(IS_WIN)
    gfx::NativeView parent_native_view_ = nullptr;
#endif
    std::string locale_;
  };

  // mojom::PrintBackendService implementation:
  void Init(const std::string& locale) override;
  void Poke() override;
  void EnumeratePrinters(
      mojom::PrintBackendService::EnumeratePrintersCallback callback) override;
  void GetDefaultPrinterName(
      mojom::PrintBackendService::GetDefaultPrinterNameCallback callback)
      override;
  void GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
          callback) override;
  void FetchCapabilities(
      const std::string& printer_name,
      mojom::PrintBackendService::FetchCapabilitiesCallback callback) override;
  void UseDefaultSettings(
      mojom::PrintBackendService::UseDefaultSettingsCallback callback) override;
#if BUILDFLAG(IS_WIN)
  void AskUserForSettings(
      uint32_t parent_window_id,
      int max_pages,
      bool has_selection,
      bool is_scripted,
      mojom::PrintBackendService::AskUserForSettingsCallback callback) override;
#endif
  void UpdatePrintSettings(
      base::Value::Dict job_settings,
      mojom::PrintBackendService::UpdatePrintSettingsCallback callback)
      override;
  void StartPrinting(
      int document_cookie,
      const std::u16string& document_name,
      mojom::PrintTargetType target_type,
      const PrintSettings& settings,
      mojom::PrintBackendService::StartPrintingCallback callback) override;
#if BUILDFLAG(IS_WIN)
  void RenderPrintedPage(
      int32_t document_cookie,
      uint32_t page_index,
      mojom::MetafileDataType page_data_type,
      base::ReadOnlySharedMemoryRegion serialized_page,
      const gfx::Size& page_size,
      const gfx::Rect& page_content_rect,
      float shrink_factor,
      mojom::PrintBackendService::RenderPrintedPageCallback callback) override;
#endif  // BUILDFLAG(IS_WIN)
  void RenderPrintedDocument(
      int32_t document_cookie,
      mojom::MetafileDataType data_type,
      base::ReadOnlySharedMemoryRegion serialized_doc,
      mojom::PrintBackendService::RenderPrintedDocumentCallback callback)
      override;
  void DocumentDone(
      int32_t document_cookie,
      mojom::PrintBackendService::DocumentDoneCallback callback) override;

  // Callbacks from worker functions.
  void OnDidStartPrintingReadyDocument(DocumentHelper& document_helper,
                                       mojom::ResultCode result);
#if BUILDFLAG(IS_WIN)
  void OnDidRenderPrintedPage(
      DocumentHelper& document_helper,
      mojom::PrintBackendService::RenderPrintedPageCallback callback,
      mojom::ResultCode result);
#endif
  void OnDidRenderPrintedDocument(
      DocumentHelper& document_helper,
      mojom::PrintBackendService::RenderPrintedDocumentCallback callback,
      mojom::ResultCode result);
  void OnDidDocumentDone(
      DocumentHelper& document_helper,
      mojom::PrintBackendService::DocumentDoneCallback callback,
      mojom::ResultCode result);

  // Utility helpers.
  DocumentHelper* GetDocumentHelper(int document_cookie);
  void RemoveDocumentHelper(DocumentHelper& document_helper);

  // Crash key is kept at class level so that we can obtain printer driver
  // information for a prior call should the process be terminated by the
  // remote.  This can happen in the case of Mojo message validation.
  std::unique_ptr<crash_keys::ScopedPrinterInfo> crash_keys_;

  scoped_refptr<PrintBackend> print_backend_;

  PrintingContextDelegate context_delegate_;

  // Want all callbacks and document helper sequence manipulations to be made
  // from main thread, not a thread runner.
  SEQUENCE_CHECKER(main_sequence_checker_);

  // Sequence of documents to be printed, in the order received.  Documents
  // could be removed from the list in any order, depending upon the speed
  // with which concurrent printing jobs are able to complete.  The
  // `DocumentHelper` objects are used and accessed only on the main thread.
  std::vector<std::unique_ptr<DocumentHelper>> documents_;

  mojo::Receiver<mojom::PrintBackendService> receiver_;
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_
