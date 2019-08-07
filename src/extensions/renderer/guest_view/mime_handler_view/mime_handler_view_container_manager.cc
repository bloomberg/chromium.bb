// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_manager.h"

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "content/public/common/mime_handler_view_mode.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_frame_container.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/url_constants.h"

namespace extensions {

namespace {

using RenderFrameMap =
    base::flat_map<int32_t, std::unique_ptr<MimeHandlerViewContainerManager>>;

RenderFrameMap* GetRenderFrameMap() {
  static base::NoDestructor<RenderFrameMap> instance;
  return instance.get();
}

}  // namespace

// static
void MimeHandlerViewContainerManager::BindRequest(
    int32_t routing_id,
    mojom::MimeHandlerViewContainerManagerRequest request) {
  CHECK(content::MimeHandlerViewMode::UsesCrossProcessFrame());
  auto* render_frame = content::RenderFrame::FromRoutingID(routing_id);
  if (!render_frame)
    return;
  auto* manager = Get(render_frame, true /* create_if_does_not_exist */);
  manager->bindings_.AddBinding(manager, std::move(request));
}

// static
MimeHandlerViewContainerManager* MimeHandlerViewContainerManager::Get(
    content::RenderFrame* render_frame,
    bool create_if_does_not_exits) {
  int32_t routing_id = render_frame->GetRoutingID();
  auto& map = *GetRenderFrameMap();
  if (base::ContainsKey(map, routing_id))
    return map[routing_id].get();
  if (create_if_does_not_exits) {
    map[routing_id] =
        std::make_unique<MimeHandlerViewContainerManager>(render_frame);
    return map[routing_id].get();
  }
  return nullptr;
}

MimeHandlerViewContainerManager::MimeHandlerViewContainerManager(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      before_unload_control_binding_(this) {}

MimeHandlerViewContainerManager::~MimeHandlerViewContainerManager() {}

void MimeHandlerViewContainerManager::OnDestruct() {
  bindings_.CloseAllBindings();
  // This will delete the class.
  GetRenderFrameMap()->erase(routing_id());
}

void MimeHandlerViewContainerManager::CreateBeforeUnloadControl(
    CreateBeforeUnloadControlCallback callback) {
  mime_handler::BeforeUnloadControlPtr before_unload_control;
  before_unload_control_binding_.Bind(
      mojo::MakeRequest(&before_unload_control));
  std::move(callback).Run(std::move(before_unload_control));
}

void MimeHandlerViewContainerManager::DestroyFrameContainer(
    int32_t element_instance_id) {
  if (auto* frame_container = GetFrameContainer(element_instance_id))
    frame_container->DestroyFrameContainer();
}

void MimeHandlerViewContainerManager::RetryCreatingMimeHandlerViewGuest(
    int32_t element_instance_id) {
  if (auto* frame_container = GetFrameContainer(element_instance_id))
    frame_container->RetryCreatingMimeHandlerViewGuest();
}

void MimeHandlerViewContainerManager::DidLoad(int32_t element_instance_id) {
  if (auto* frame_container = GetFrameContainer(element_instance_id))
    frame_container->DidLoad();
}

MimeHandlerViewFrameContainer*
MimeHandlerViewContainerManager::GetFrameContainer(int32_t instance_id) {
  for (auto* container_base :
       MimeHandlerViewContainerBase::FromRenderFrame(render_frame())) {
    auto* frame_container =
        static_cast<MimeHandlerViewFrameContainer*>(container_base);
    if (frame_container->element_instance_id() == instance_id)
      return frame_container;
  }
  return nullptr;
}

void MimeHandlerViewContainerManager::SetShowBeforeUnloadDialog(
    bool show_dialog,
    SetShowBeforeUnloadDialogCallback callback) {
  render_frame()->GetWebFrame()->GetDocument().SetShowBeforeUnloadDialog(
      show_dialog);
  std::move(callback).Run();
}

}  // namespace extensions
