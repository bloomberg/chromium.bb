// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/printing/print_dialog_linux.h"

#include <string>
#include <vector>

#include "libcef/browser/extensions/browser_extensions_util.h"
#include "libcef/browser/print_settings_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/frame_util.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/global_routing_id.h"
#include "printing/metafile.h"
#include "printing/mojom/print.mojom-shared.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"

using content::BrowserThread;
using printing::PageRanges;
using printing::PrintSettings;

namespace {

printing::PrintingContextLinux::CreatePrintDialogFunctionPtr
    g_default_create_print_dialog_func = nullptr;
printing::PrintingContextLinux::PdfPaperSizeFunctionPtr
    g_default_pdf_paper_size_func = nullptr;

CefRefPtr<CefBrowserHostBase> GetBrowserForContext(
    printing::PrintingContextLinux* context) {
  return extensions::GetOwnerBrowserForGlobalId(
      frame_util::MakeGlobalId(context->render_process_id(),
                               context->render_frame_id()),
      nullptr);
}

CefRefPtr<CefPrintHandler> GetPrintHandlerForBrowser(
    CefRefPtr<CefBrowserHostBase> browser) {
  if (browser) {
    if (auto client = browser->GetClient()) {
      return client->GetPrintHandler();
    }
  }
  return nullptr;
}

}  // namespace

class CefPrintDialogCallbackImpl : public CefPrintDialogCallback {
 public:
  explicit CefPrintDialogCallbackImpl(CefRefPtr<CefPrintDialogLinux> dialog)
      : dialog_(dialog) {}

  CefPrintDialogCallbackImpl(const CefPrintDialogCallbackImpl&) = delete;
  CefPrintDialogCallbackImpl& operator=(const CefPrintDialogCallbackImpl&) =
      delete;

  void Continue(CefRefPtr<CefPrintSettings> settings) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (dialog_.get()) {
        dialog_->OnPrintContinue(settings);
        dialog_ = nullptr;
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefPrintDialogCallbackImpl::Continue, this,
                                   settings));
    }
  }

  void Cancel() override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (dialog_.get()) {
        dialog_->OnPrintCancel();
        dialog_ = nullptr;
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefPrintDialogCallbackImpl::Cancel, this));
    }
  }

  void Disconnect() { dialog_ = nullptr; }

 private:
  CefRefPtr<CefPrintDialogLinux> dialog_;

  IMPLEMENT_REFCOUNTING(CefPrintDialogCallbackImpl);
};

class CefPrintJobCallbackImpl : public CefPrintJobCallback {
 public:
  explicit CefPrintJobCallbackImpl(CefRefPtr<CefPrintDialogLinux> dialog)
      : dialog_(dialog) {}

  CefPrintJobCallbackImpl(const CefPrintJobCallbackImpl&) = delete;
  CefPrintJobCallbackImpl& operator=(const CefPrintJobCallbackImpl&) = delete;

  void Continue() override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (dialog_.get()) {
        dialog_->OnJobCompleted();
        dialog_ = nullptr;
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefPrintJobCallbackImpl::Continue, this));
    }
  }

  void Disconnect() { dialog_ = nullptr; }

 private:
  CefRefPtr<CefPrintDialogLinux> dialog_;

  IMPLEMENT_REFCOUNTING(CefPrintJobCallbackImpl);
};

// static
printing::PrintDialogGtkInterface* CefPrintDialogLinux::CreatePrintDialog(
    PrintingContextLinux* context) {
  CEF_REQUIRE_UIT();

  printing::PrintDialogGtkInterface* interface = nullptr;

  auto browser = GetBrowserForContext(context);
  if (!browser) {
    LOG(ERROR) << "No associated browser in CreatePrintDialog; using default "
                  "printing implementation.";
  }

  auto handler = GetPrintHandlerForBrowser(browser);
  if (!handler) {
    if (g_default_create_print_dialog_func) {
      interface = g_default_create_print_dialog_func(context);
      DCHECK(interface);
    }
  } else {
    interface = new CefPrintDialogLinux(context, browser, handler);
  }

  if (!interface) {
    LOG(ERROR) << "Null interface in CreatePrintDialog; printing will fail.";
  }

  return interface;
}

// static
gfx::Size CefPrintDialogLinux::GetPdfPaperSize(
    printing::PrintingContextLinux* context) {
  CEF_REQUIRE_UIT();

  gfx::Size size;

  auto browser = GetBrowserForContext(context);
  if (!browser) {
    LOG(ERROR) << "No associated browser in GetPdfPaperSize; using default "
                  "printing implementation.";
  }

  auto handler = GetPrintHandlerForBrowser(browser);
  if (!handler) {
    if (g_default_pdf_paper_size_func) {
      size = g_default_pdf_paper_size_func(context);
      DCHECK(!size.IsEmpty());
    }
  } else {
    const printing::PrintSettings& settings = context->settings();
    CefSize cef_size = handler->GetPdfPaperSize(
        browser.get(), settings.device_units_per_inch());
    size.SetSize(cef_size.width, cef_size.height);
  }

  if (size.IsEmpty()) {
    LOG(ERROR) << "Empty size value returned in GetPdfPaperSize; PDF printing "
                  "will fail.";
  }
  return size;
}

// static
void CefPrintDialogLinux::SetDefaultPrintingContextFuncs(
    printing::PrintingContextLinux::CreatePrintDialogFunctionPtr
        create_print_dialog_func,
    printing::PrintingContextLinux::PdfPaperSizeFunctionPtr
        pdf_paper_size_func) {
  g_default_create_print_dialog_func = create_print_dialog_func;
  g_default_pdf_paper_size_func = pdf_paper_size_func;
}

// static
void CefPrintDialogLinux::OnPrintStart(CefRefPtr<CefBrowserHostBase> browser) {
  CEF_REQUIRE_UIT();
  DCHECK(browser);
  if (browser && browser->GetClient()) {
    if (auto handler = browser->GetClient()->GetPrintHandler()) {
      handler->OnPrintStart(browser.get());
    }
  }
}

CefPrintDialogLinux::CefPrintDialogLinux(PrintingContextLinux* context,
                                         CefRefPtr<CefBrowserHostBase> browser,
                                         CefRefPtr<CefPrintHandler> handler)
    : context_(context), browser_(browser), handler_(handler) {
  DCHECK(context_);
  DCHECK(browser_);
  DCHECK(handler_);

  // Paired with the ReleaseDialog() call.
  AddRef();
}

CefPrintDialogLinux::~CefPrintDialogLinux() {
  // It's not safe to dereference |context_| during the destruction of this
  // object because the PrintJobWorker which owns |context_| may already have
  // been deleted.
  CEF_REQUIRE_UIT();
  handler_->OnPrintReset(browser_.get());
}

void CefPrintDialogLinux::UseDefaultSettings() {
  UpdateSettings(std::make_unique<PrintSettings>(), true);
}

void CefPrintDialogLinux::UpdateSettings(
    std::unique_ptr<PrintSettings> settings) {
  UpdateSettings(std::move(settings), false);
}

void CefPrintDialogLinux::ShowDialog(
    gfx::NativeView parent_view,
    bool has_selection,
    PrintingContextLinux::PrintSettingsCallback callback) {
  CEF_REQUIRE_UIT();

  callback_ = std::move(callback);

  CefRefPtr<CefPrintDialogCallbackImpl> callback_impl(
      new CefPrintDialogCallbackImpl(this));

  if (!handler_->OnPrintDialog(browser_.get(), has_selection,
                               callback_impl.get())) {
    callback_impl->Disconnect();
    OnPrintCancel();
  }
}

void CefPrintDialogLinux::PrintDocument(
    const printing::MetafilePlayer& metafile,
    const std::u16string& document_name) {
  // This runs on the print worker thread, does not block the UI thread.
  DCHECK(!CEF_CURRENTLY_ON_UIT());

  // The document printing tasks can outlive the PrintingContext that created
  // this dialog.
  AddRef();

  bool success = base::CreateTemporaryFile(&path_to_pdf_);

  if (success) {
    base::File file;
    file.Initialize(path_to_pdf_,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    success = metafile.SaveTo(&file);
    file.Close();
    if (!success)
      base::DeleteFile(path_to_pdf_);
  }

  if (!success) {
    LOG(ERROR) << "Saving metafile failed";
    // Matches AddRef() above.
    Release();
    return;
  }

  // No errors, continue printing.
  CEF_POST_TASK(
      CEF_UIT, base::BindOnce(&CefPrintDialogLinux::SendDocumentToPrinter, this,
                              document_name));
}

void CefPrintDialogLinux::ReleaseDialog() {
  context_ = nullptr;
  Release();
}

bool CefPrintDialogLinux::UpdateSettings(
    std::unique_ptr<PrintSettings> settings,
    bool get_defaults) {
  CEF_REQUIRE_UIT();

  CefRefPtr<CefPrintSettingsImpl> settings_impl(
      new CefPrintSettingsImpl(std::move(settings), false));
  handler_->OnPrintSettings(browser_.get(), settings_impl.get(), get_defaults);

  context_->InitWithSettings(settings_impl->TakeOwnership());
  return true;
}

void CefPrintDialogLinux::SendDocumentToPrinter(
    const std::u16string& document_name) {
  CEF_REQUIRE_UIT();

  if (!handler_.get()) {
    OnJobCompleted();
    return;
  }

  CefRefPtr<CefPrintJobCallbackImpl> callback_impl(
      new CefPrintJobCallbackImpl(this));

  if (!handler_->OnPrintJob(browser_.get(), document_name, path_to_pdf_.value(),
                            callback_impl.get())) {
    callback_impl->Disconnect();
    OnJobCompleted();
  }
}

void CefPrintDialogLinux::OnPrintContinue(
    CefRefPtr<CefPrintSettings> settings) {
  CefPrintSettingsImpl* impl =
      static_cast<CefPrintSettingsImpl*>(settings.get());
  context_->InitWithSettings(impl->TakeOwnership());
  std::move(callback_).Run(printing::mojom::ResultCode::kSuccess);
}

void CefPrintDialogLinux::OnPrintCancel() {
  std::move(callback_).Run(printing::mojom::ResultCode::kCanceled);
}

void CefPrintDialogLinux::OnJobCompleted() {
  CEF_POST_BACKGROUND_TASK(base::GetDeleteFileCallback(path_to_pdf_));

  // Printing finished. Matches AddRef() in PrintDocument();
  Release();
}
