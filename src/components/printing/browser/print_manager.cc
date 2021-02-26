// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_manager.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace printing {

struct PrintManager::FrameDispatchHelper {
  PrintManager* manager;
  content::RenderFrameHost* render_frame_host;

  bool Send(IPC::Message* msg) { return render_frame_host->Send(msg); }

  void OnScriptedPrint(const mojom::ScriptedPrintParams& scripted_params,
                       IPC::Message* reply_msg) {
    manager->OnScriptedPrint(render_frame_host, scripted_params, reply_msg);
  }

  void OnDidPrintDocument(const mojom::DidPrintDocumentParams& params,
                          IPC::Message* reply_msg) {
    // If DidPrintDocument message was received then need to transition from
    // a variable allocated on stack (which has efficient memory management
    // when dealing with any other incoming message) to a persistent variable
    // on the heap that can be referenced by the asynchronous processing which
    // occurs beyond the scope of PrintViewManagerBase::OnMessageReceived().
    manager->OnDidPrintDocument(
        render_frame_host, params,
        std::make_unique<DelayedFrameDispatchHelper>(
            manager->web_contents(), render_frame_host, reply_msg));
  }
};

PrintManager::DelayedFrameDispatchHelper::DelayedFrameDispatchHelper(
    content::WebContents* contents,
    content::RenderFrameHost* render_frame_host,
    IPC::Message* reply_msg)
    : content::WebContentsObserver(contents),
      render_frame_host_(render_frame_host),
      reply_msg_(reply_msg) {}

PrintManager::DelayedFrameDispatchHelper::~DelayedFrameDispatchHelper() {
  if (reply_msg_) {
    PrintHostMsg_DidPrintDocument::WriteReplyParams(reply_msg_, false);
    render_frame_host_->Send(reply_msg_);
  }
}

void PrintManager::DelayedFrameDispatchHelper::SendCompleted() {
  if (!reply_msg_)
    return;

  PrintHostMsg_DidPrintDocument::WriteReplyParams(reply_msg_, true);
  render_frame_host_->Send(reply_msg_);

  // This wraps up the one allowed reply for the message.
  reply_msg_ = nullptr;
}

void PrintManager::DelayedFrameDispatchHelper::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == render_frame_host_)
    reply_msg_ = nullptr;
}

PrintManager::PrintManager(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      print_manager_host_receivers_(contents, this) {}

PrintManager::~PrintManager() = default;

bool PrintManager::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  FrameDispatchHelper helper = {this, render_frame_host};
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintManager, message)
    IPC_MESSAGE_FORWARD_DELAY_REPLY(PrintHostMsg_ScriptedPrint, &helper,
                                    FrameDispatchHelper::OnScriptedPrint)
    IPC_MESSAGE_FORWARD_DELAY_REPLY(PrintHostMsg_DidPrintDocument, &helper,
                                    FrameDispatchHelper::OnDidPrintDocument);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PrintManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  print_render_frames_.erase(render_frame_host);
}

void PrintManager::DidGetPrintedPagesCount(int32_t cookie,
                                           uint32_t number_pages) {
  DCHECK_GT(cookie, 0);
  DCHECK_GT(number_pages, 0u);
  number_pages_ = number_pages;
}

void PrintManager::DidGetDocumentCookie(int32_t cookie) {
  cookie_ = cookie;
}

#if BUILDFLAG(ENABLE_TAGGED_PDF)
void PrintManager::SetAccessibilityTree(
    int32_t cookie,
    const ui::AXTreeUpdate& accessibility_tree) {}
#endif

void PrintManager::UpdatePrintSettings(int32_t cookie,
                                       base::Value job_settings,
                                       UpdatePrintSettingsCallback callback) {
  auto params = mojom::PrintPagesParams::New();
  params->params = mojom::PrintParams::New();
  std::move(callback).Run(std::move(params), false);
}

void PrintManager::DidShowPrintDialog() {}

void PrintManager::ShowInvalidPrinterSettingsError() {}

void PrintManager::PrintingFailed(int32_t cookie) {
  if (cookie != cookie_) {
    NOTREACHED();
    return;
  }
#if defined(OS_ANDROID)
  PdfWritingDone(0);
#endif
}

bool PrintManager::IsPrintRenderFrameConnected(content::RenderFrameHost* rfh) {
  auto it = print_render_frames_.find(rfh);
  if (it == print_render_frames_.end())
    return false;

  return it->second.is_bound() && it->second.is_connected();
}

const mojo::AssociatedRemote<printing::mojom::PrintRenderFrame>&
PrintManager::GetPrintRenderFrame(content::RenderFrameHost* rfh) {
  auto it = print_render_frames_.find(rfh);
  if (it == print_render_frames_.end()) {
    mojo::AssociatedRemote<printing::mojom::PrintRenderFrame> remote;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&remote);
    it = print_render_frames_.insert({rfh, std::move(remote)}).first;
  } else if (it->second.is_bound() && !it->second.is_connected()) {
    // When print preview is closed, the remote is disconnected from the
    // receiver. Reset and bind the remote before using it again.
    it->second.reset();
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&it->second);
  }

  return it->second;
}

void PrintManager::PrintingRenderFrameDeleted() {
#if defined(OS_ANDROID)
  PdfWritingDone(0);
#endif
}

}  // namespace printing
