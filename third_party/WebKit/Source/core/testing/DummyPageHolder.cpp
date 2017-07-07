/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/testing/DummyPageHolder.h"

#include <memory>
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/loader/EmptyClients.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

std::unique_ptr<DummyPageHolder> DummyPageHolder::Create(
    const IntSize& initial_view_size,
    Page::PageClients* page_clients,
    LocalFrameClient* local_frame_client,
    FrameSettingOverrideFunction setting_overrider) {
  return WTF::WrapUnique(new DummyPageHolder(
      initial_view_size, page_clients, local_frame_client, setting_overrider));
}

DummyPageHolder::DummyPageHolder(
    const IntSize& initial_view_size,
    Page::PageClients* page_clients_argument,
    LocalFrameClient* local_frame_client,
    FrameSettingOverrideFunction setting_overrider) {
  Page::PageClients page_clients;
  if (!page_clients_argument) {
    FillWithEmptyClients(page_clients);
  } else {
    page_clients.chrome_client = page_clients_argument->chrome_client;
    page_clients.context_menu_client =
        page_clients_argument->context_menu_client;
    page_clients.editor_client = page_clients_argument->editor_client;
    page_clients.spell_checker_client =
        page_clients_argument->spell_checker_client;
  }
  page_ = Page::Create(page_clients);
  Settings& settings = page_->GetSettings();
  // FIXME: http://crbug.com/363843. This needs to find a better way to
  // not create graphics layers.
  settings.SetAcceleratedCompositingEnabled(false);
  if (setting_overrider)
    (*setting_overrider)(settings);

  local_frame_client_ = local_frame_client;
  if (!local_frame_client_)
    local_frame_client_ = EmptyLocalFrameClient::Create();

  frame_ = LocalFrame::Create(local_frame_client_.Get(), *page_, nullptr);
  frame_->SetView(LocalFrameView::Create(*frame_, initial_view_size));
  frame_->View()->GetPage()->GetVisualViewport().SetSize(initial_view_size);
  frame_->Init();
}

DummyPageHolder::~DummyPageHolder() {
  page_->WillBeDestroyed();
  page_.Clear();
  frame_.Clear();
}

Page& DummyPageHolder::GetPage() const {
  return *page_;
}

LocalFrame& DummyPageHolder::GetFrame() const {
  DCHECK(frame_);
  return *frame_;
}

LocalFrameView& DummyPageHolder::GetFrameView() const {
  return *frame_->View();
}

Document& DummyPageHolder::GetDocument() const {
  return *frame_->DomWindow()->document();
}

}  // namespace blink
