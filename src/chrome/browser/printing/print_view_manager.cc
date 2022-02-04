// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#endif

using content::BrowserThread;

namespace printing {

namespace {

PrintManager* g_receiver_for_testing = nullptr;

// Keeps track of pending scripted print preview closures.
// No locking, only access on the UI thread.
base::LazyInstance<std::map<content::RenderProcessHost*, base::OnceClosure>>::
    Leaky g_scripted_print_preview_closure_map = LAZY_INSTANCE_INITIALIZER;

content::WebContents* GetPrintPreviewDialog(
    content::WebContents* web_contents) {
  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return nullptr;
  return dialog_controller->GetPrintPreviewForContents(web_contents);
}

}  // namespace

PrintViewManager::PrintViewManager(content::WebContents* web_contents)
    : PrintViewManagerBase(web_contents),
      content::WebContentsUserData<PrintViewManager>(*web_contents) {}

PrintViewManager::~PrintViewManager() {
  DCHECK_EQ(NOT_PREVIEWING, print_preview_state_);
}

// static
void PrintViewManager::BindPrintManagerHost(
    mojo::PendingAssociatedReceiver<mojom::PrintManagerHost> receiver,
    content::RenderFrameHost* rfh) {
  if (g_receiver_for_testing) {
    g_receiver_for_testing->BindReceiver(std::move(receiver), rfh);
    return;
  }

  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* print_manager = PrintViewManager::FromWebContents(web_contents);
  if (!print_manager)
    return;
  print_manager->BindReceiver(std::move(receiver), rfh);
}

bool PrintViewManager::PrintForSystemDialogNow(
    base::OnceClosure dialog_shown_callback) {
  DCHECK(dialog_shown_callback);
  DCHECK(!on_print_dialog_shown_callback_);
  on_print_dialog_shown_callback_ = std::move(dialog_shown_callback);
  is_switching_to_system_dialog_ = true;

  // Remember the ID for `print_preview_rfh_`, to enable checking that the
  // `RenderFrameHost` is still valid after a possible inner message loop runs
  // in `DisconnectFromCurrentPrintJob()`.
  content::GlobalRenderFrameHostId rfh_id = print_preview_rfh_->GetGlobalId();

  auto weak_this = weak_factory_.GetWeakPtr();
  DisconnectFromCurrentPrintJob();
  if (!weak_this)
    return false;

  // Don't print / print preview crashed tabs.
  if (IsCrashed())
    return false;

  // Don't print if `print_preview_rfh_` is no longer live.
  if (!content::RenderFrameHost::FromID(rfh_id) ||
      !print_preview_rfh_->IsRenderFrameLive()) {
    return false;
  }

  // TODO(crbug.com/809738)  Register with `PrintBackendServiceManager` when
  // system print is enabled out-of-process.

  SetPrintingRFH(print_preview_rfh_);
  GetPrintRenderFrame(print_preview_rfh_)->PrintForSystemDialog();
  return true;
}

bool PrintViewManager::BasicPrint(content::RenderFrameHost* rfh) {
  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return false;

  content::WebContents* print_preview_dialog =
      dialog_controller->GetPrintPreviewForContents(web_contents());
  if (!print_preview_dialog)
    return PrintNow(rfh);

  return !!print_preview_dialog->GetWebUI();
}

bool PrintViewManager::PrintPreviewNow(content::RenderFrameHost* rfh,
                                       bool has_selection) {
  return PrintPreview(rfh, mojo::NullAssociatedRemote(), has_selection);
}

bool PrintViewManager::PrintPreviewWithPrintRenderer(
    content::RenderFrameHost* rfh,
    mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer) {
  return PrintPreview(rfh, std::move(print_renderer), false);
}

void PrintViewManager::PrintPreviewForWebNode(content::RenderFrameHost* rfh) {
  if (print_preview_state_ != NOT_PREVIEWING)
    return;

  DCHECK(rfh);
  DCHECK(IsPrintRenderFrameConnected(rfh));
  // All callers should already ensure this condition holds; CHECK to
  // aggressively protect against future unsafety.
  CHECK(rfh->IsRenderFrameLive());
  DCHECK(!print_preview_rfh_);
  print_preview_rfh_ = rfh;
  print_preview_state_ = USER_INITIATED_PREVIEW;

  for (auto& observer : GetObservers())
    observer.OnPrintPreview(print_preview_rfh_);
}

void PrintViewManager::PrintPreviewAlmostDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (print_preview_state_ != SCRIPTED_PREVIEW)
    return;

  MaybeUnblockScriptedPreviewRPH();
}

void PrintViewManager::PrintPreviewDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (print_preview_state_ == NOT_PREVIEWING)
    return;

// Send OnPrintPreviewDialogClosed message for 'afterprint' event.
#if BUILDFLAG(IS_WIN)
  // On Windows, we always send OnPrintPreviewDialogClosed. It's ok to dispatch
  // 'afterprint' at this timing because system dialog printing on
  // Windows doesn't need the original frame.
  bool send_message = true;
#else
  // On non-Windows, we don't need to send OnPrintPreviewDialogClosed when we
  // are switching to system dialog. PrintRenderFrameHelper is responsible to
  // dispatch 'afterprint' event.
  bool send_message = !is_switching_to_system_dialog_;
#endif
  if (send_message) {
    // Only send a message about having closed if the RenderFrame is live and
    // PrintRenderFrame is connected. Normally IsPrintRenderFrameConnected()
    // implies  IsRenderFrameLive(). However, when a renderer process exits
    // (e.g. due to a crash), RenderFrameDeleted() and PrintPreviewDone() are
    // triggered by independent observers. Since there is no guarantee which
    // observer will run first, both conditions are explicitly checked here.
    if (print_preview_rfh_->IsRenderFrameLive() &&
        IsPrintRenderFrameConnected(print_preview_rfh_)) {
      GetPrintRenderFrame(print_preview_rfh_)->OnPrintPreviewDialogClosed();
    }
  }
  is_switching_to_system_dialog_ = false;

  if (print_preview_state_ == SCRIPTED_PREVIEW) {
    auto& map = g_scripted_print_preview_closure_map.Get();
    auto it = map.find(scripted_print_preview_rph_);
    CHECK(it != map.end());
    std::move(it->second).Run();
    map.erase(it);

    // PrintPreviewAlmostDone() usually already calls this. Calling it again
    // will likely be a no-op, but do it anyway to reset the state for sure.
    MaybeUnblockScriptedPreviewRPH();
    scripted_print_preview_rph_ = nullptr;
  }
  print_preview_state_ = NOT_PREVIEWING;
  print_preview_rfh_ = nullptr;
}

void PrintViewManager::RejectPrintPreviewRequestIfRestricted(
    base::OnceCallback<void(bool should_proceed)> callback) {
#if BUILDFLAG(IS_CHROMEOS)
  // Don't print DLP restricted content on Chrome OS.
  policy::DlpContentManager::Get()->CheckPrintingRestriction(
      web_contents(), std::move(callback));
#else
  std::move(callback).Run(true);
#endif
}

void PrintViewManager::OnPrintPreviewRequestRejected(int render_process_id,
                                                     int render_frame_id) {
  auto* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!rfh) {
    return;
  }
  PrintPreviewDone();
  PrintPreviewRejectedForTesting();
}

void PrintViewManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == print_preview_rfh_)
    PrintPreviewDone();
  PrintViewManagerBase::RenderFrameDeleted(render_frame_host);
}

// static
void PrintViewManager::SetReceiverImplForTesting(PrintManager* impl) {
  g_receiver_for_testing = impl;
}

bool PrintViewManager::PrintPreview(
    content::RenderFrameHost* rfh,
    mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer,
    bool has_selection) {
  // Users can send print commands all they want and it is beyond
  // PrintViewManager's control. Just ignore the extra commands.
  // See http://crbug.com/136842 for example.
  if (print_preview_state_ != NOT_PREVIEWING)
    return false;

  // Don't print / print preview crashed tabs.
  if (IsCrashed() || !rfh->IsRenderFrameLive())
    return false;

  GetPrintRenderFrame(rfh)->InitiatePrintPreview(std::move(print_renderer),
                                                 has_selection);

  DCHECK(!print_preview_rfh_);
  print_preview_rfh_ = rfh;
  print_preview_state_ = USER_INITIATED_PREVIEW;

  for (auto& observer : GetObservers())
    observer.OnPrintPreview(print_preview_rfh_);

  return true;
}

void PrintViewManager::DidShowPrintDialog() {
  if (GetCurrentTargetFrame() != print_preview_rfh_)
    return;

  if (on_print_dialog_shown_callback_)
    std::move(on_print_dialog_shown_callback_).Run();
}

void PrintViewManager::SetupScriptedPrintPreview(
    SetupScriptedPrintPreviewCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderFrameHost* rfh = GetCurrentTargetFrame();
  // The Mojo receiver endpoint is owned by a RenderFrameHostReceiverSet, so
  // this DCHECK should always hold.
  DCHECK(rfh->IsRenderFrameLive());
  content::RenderProcessHost* rph = rfh->GetProcess();

  if (rfh->IsNestedWithinFencedFrame()) {
    // The renderer should have checked and disallowed the request for fenced
    // frames in ChromeClient. Ignore the request and mark it as bad if it
    // didn't happen for some reason.
    bad_message::ReceivedBadMessage(
        rph, bad_message::PVM_SCRIPTED_PRINT_FENCED_FRAME);
    std::move(callback).Run();
    return;
  }

  auto& map = g_scripted_print_preview_closure_map.Get();
  if (base::Contains(map, rph)) {
    // Renderer already handling window.print(). Abort this attempt to prevent
    // the renderer from having multiple nested loops. If multiple nested loops
    // existed, then they have to exit in the right order and that is messy.
    std::move(callback).Run();
    return;
  }

  if (print_preview_state_ != NOT_PREVIEWING) {
    // If a print dialog is already open for this tab, ignore the scripted print
    // message.
    std::move(callback).Run();
    return;
  }

  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  if (!dialog_controller) {
    std::move(callback).Run();
    return;
  }

  DCHECK(!print_preview_rfh_);
  print_preview_rfh_ = rfh;
  print_preview_state_ = SCRIPTED_PREVIEW;
  map[rph] = base::BindOnce(&PrintViewManager::OnScriptedPrintPreviewReply,
                            base::Unretained(this), std::move(callback));
  scripted_print_preview_rph_ = rph;
  DCHECK(!scripted_print_preview_rph_set_blocked_);
  if (!scripted_print_preview_rph_->IsBlocked()) {
    scripted_print_preview_rph_->SetBlocked(true);
    scripted_print_preview_rph_set_blocked_ = true;
  }
}

void PrintViewManager::ShowScriptedPrintPreview(bool source_is_modifiable) {
  if (print_preview_state_ != SCRIPTED_PREVIEW)
    return;

  DCHECK(print_preview_rfh_);
  if (GetCurrentTargetFrame() != print_preview_rfh_)
    return;

  int render_process_id = print_preview_rfh_->GetProcess()->GetID();
  int render_frame_id = print_preview_rfh_->GetRoutingID();

  RejectPrintPreviewRequestIfRestricted(
      base::BindOnce(&PrintViewManager::OnScriptedPrintPreviewCallback,
                     weak_factory_.GetWeakPtr(), source_is_modifiable,
                     render_process_id, render_frame_id));
}

void PrintViewManager::OnScriptedPrintPreviewCallback(bool source_is_modifiable,
                                                      int render_process_id,
                                                      int render_frame_id,
                                                      bool should_proceed) {
  if (!should_proceed) {
    OnPrintPreviewRequestRejected(render_process_id, render_frame_id);
    return;
  }

  if (print_preview_state_ != SCRIPTED_PREVIEW)
    return;

  DCHECK(print_preview_rfh_);

  auto* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!rfh || rfh != print_preview_rfh_) {
    return;
  }

  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  if (!dialog_controller) {
    PrintPreviewDone();
    return;
  }

  // Running a dialog causes an exit to webpage-initiated fullscreen.
  // http://crbug.com/728276
  if (web_contents()->IsFullscreen())
    web_contents()->ExitFullscreen(true);

  dialog_controller->PrintPreview(web_contents());
  mojom::RequestPrintPreviewParams params;
  params.is_modifiable = source_is_modifiable;
  PrintPreviewUI::SetInitialParams(
      dialog_controller->GetPrintPreviewForContents(web_contents()), params);
  PrintPreviewAllowedForTesting();
}

void PrintViewManager::RequestPrintPreview(
    mojom::RequestPrintPreviewParamsPtr params) {
  content::RenderFrameHost* render_frame_host = GetCurrentTargetFrame();
  content::RenderProcessHost* render_process_host =
      render_frame_host->GetProcess();

  RejectPrintPreviewRequestIfRestricted(base::BindOnce(
      &PrintViewManager::OnRequestPrintPreviewCallback,
      weak_factory_.GetWeakPtr(), std::move(params),
      render_process_host->GetID(), render_frame_host->GetRoutingID()));
}

void PrintViewManager::OnRequestPrintPreviewCallback(
    mojom::RequestPrintPreviewParamsPtr params,
    int render_process_id,
    int render_frame_id,
    bool should_proceed) {
  if (!should_proceed) {
    OnPrintPreviewRequestRejected(render_process_id, render_frame_id);
    return;
  }
  // Double-check that the RenderFrameHost is still alive and has a live
  // RenderFrame, since the DLP check is potentially asynchronous.
  auto* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host || !render_frame_host->IsRenderFrameLive()) {
    return;
  }
  if (params->webnode_only) {
    PrintPreviewForWebNode(render_frame_host);
  }
  PrintPreviewDialogController::PrintPreview(web_contents());
  PrintPreviewUI::SetInitialParams(GetPrintPreviewDialog(web_contents()),
                                   *params);
  PrintPreviewAllowedForTesting();
}

void PrintViewManager::CheckForCancel(int32_t preview_ui_id,
                                      int32_t request_id,
                                      CheckForCancelCallback callback) {
  std::move(callback).Run(
      PrintPreviewUI::ShouldCancelRequest(preview_ui_id, request_id));
}

void PrintViewManager::OnScriptedPrintPreviewReply(
    SetupScriptedPrintPreviewCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run();
}

void PrintViewManager::MaybeUnblockScriptedPreviewRPH() {
  if (scripted_print_preview_rph_set_blocked_) {
    scripted_print_preview_rph_->SetBlocked(false);
    scripted_print_preview_rph_set_blocked_ = false;
  }
}

void PrintViewManager::PrintPreviewRejectedForTesting() {
  // Note: This is only used for testing.
}

void PrintViewManager::PrintPreviewAllowedForTesting() {
  // Note: This is only used for testing.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintViewManager);

}  // namespace printing
