// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_frame_container.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/pickle.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

class MimeHandlerViewFrameContainer::RenderFrameLifetimeObserver
    : public content::RenderFrameObserver {
 public:
  RenderFrameLifetimeObserver(content::RenderFrame* render_frame,
                              MimeHandlerViewFrameContainer* container);
  ~RenderFrameLifetimeObserver() override;

  // content:RenderFrameObserver override.
  void OnDestruct() final;

 private:
  MimeHandlerViewFrameContainer* const container_;
};

MimeHandlerViewFrameContainer::RenderFrameLifetimeObserver::
    RenderFrameLifetimeObserver(content::RenderFrame* render_frame,
                                MimeHandlerViewFrameContainer* container)
    : content::RenderFrameObserver(render_frame), container_(container) {}

MimeHandlerViewFrameContainer::RenderFrameLifetimeObserver::
    ~RenderFrameLifetimeObserver() {}

void MimeHandlerViewFrameContainer::RenderFrameLifetimeObserver::OnDestruct() {
  container_->DestroyFrameContainer();
}

// static.
bool MimeHandlerViewFrameContainer::IsSupportedMimeType(
    const std::string& mime_type) {
  return mime_type == "text/pdf" || mime_type == "application/pdf" ||
         mime_type == "text/csv";
}

// static
bool MimeHandlerViewFrameContainer::Create(
    const blink::WebElement& plugin_element,
    const GURL& resource_url,
    const std::string& mime_type,
    const content::WebPluginInfo& plugin_info) {
  if (plugin_info.type != content::WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN) {
    // TODO(ekaramad): Rename this plugin type once https://crbug.com/659750 is
    // fixed. We only create a MHVFC for the plugin types of BrowserPlugin
    // (which used to create a MimeHandlerViewContainer).
    return false;
  }

  if (!IsSupportedMimeType(mime_type))
    return false;
  // Life time is managed by the class itself: when the MimeHandlerViewGuest
  // is destroyed an IPC is sent to renderer to cleanup this instance.
  return new MimeHandlerViewFrameContainer(plugin_element, resource_url,
                                           mime_type, plugin_info);
}

v8::Local<v8::Object> MimeHandlerViewFrameContainer::GetScriptableObject(
    const blink::WebElement& plugin_element,
    v8::Isolate* isolate) {
  // TODO(ekaramad): Implement.
  return v8::Local<v8::Object>();
}

// static
void MimeHandlerViewFrameContainer::CreateWithFrame(
    blink::WebLocalFrame* web_frame,
    const GURL& resource_url,
    const std::string& mime_type,
    const std::string& view_id) {
  new MimeHandlerViewFrameContainer(web_frame, resource_url, mime_type,
                                    view_id);
}

MimeHandlerViewFrameContainer::MimeHandlerViewFrameContainer(
    const blink::WebElement& plugin_element,
    const GURL& resource_url,
    const std::string& mime_type,
    const content::WebPluginInfo& plugin_info)
    : MimeHandlerViewContainerBase(content::RenderFrame::FromWebFrame(
                                       plugin_element.GetDocument().GetFrame()),
                                   plugin_info,
                                   mime_type,
                                   resource_url),
      plugin_element_(plugin_element),
      element_instance_id_(content::RenderThread::Get()->GenerateRoutingID()),
      render_frame_lifetime_observer_(
          new RenderFrameLifetimeObserver(GetEmbedderRenderFrame(), this)) {
  is_embedded_ = true;
  SendResourceRequest();
}

MimeHandlerViewFrameContainer::MimeHandlerViewFrameContainer(
    blink::WebLocalFrame* web_local_frame,
    const GURL& resource_url,
    const std::string& mime_type,
    const std::string& view_id)
    : MimeHandlerViewContainerBase(
          content::RenderFrame::FromWebFrame(
              web_local_frame->Parent()->ToWebLocalFrame()),
          content::WebPluginInfo(),
          mime_type,
          resource_url),
      element_instance_id_(content::RenderThread::Get()->GenerateRoutingID()) {
  is_embedded_ = false;
  view_id_ = view_id;
  plugin_frame_routing_id_ =
      content::RenderFrame::FromWebFrame(web_local_frame)->GetRoutingID();
  MimeHandlerViewContainerBase::CreateMimeHandlerViewGuestIfNecessary();
}

MimeHandlerViewFrameContainer::~MimeHandlerViewFrameContainer() {}

void MimeHandlerViewFrameContainer::CreateMimeHandlerViewGuestIfNecessary() {
  if (auto* frame = GetContentFrame()) {
    plugin_frame_routing_id_ =
        content::RenderFrame::GetRoutingIdForWebFrame(frame);
  }
  if (plugin_frame_routing_id_ == MSG_ROUTING_NONE) {
    DestroyFrameContainer();
    return;
  }
  MimeHandlerViewContainerBase::CreateMimeHandlerViewGuestIfNecessary();
}

void MimeHandlerViewFrameContainer::RetryCreatingMimeHandlerViewGuest() {
  CreateMimeHandlerViewGuestIfNecessary();
}

void MimeHandlerViewFrameContainer::DidLoad() {
  DidLoadInternal();
}

void MimeHandlerViewFrameContainer::DestroyFrameContainer() {
  delete this;
}

blink::WebRemoteFrame* MimeHandlerViewFrameContainer::GetGuestProxyFrame()
    const {
  return GetContentFrame()->ToWebRemoteFrame();
}

int32_t MimeHandlerViewFrameContainer::GetInstanceId() const {
  return element_instance_id_;
}

gfx::Size MimeHandlerViewFrameContainer::GetElementSize() const {
  return gfx::Size();
}

blink::WebFrame* MimeHandlerViewFrameContainer::GetContentFrame() const {
  if (is_embedded_)
    return blink::WebFrame::FromFrameOwnerElement(plugin_element_);

  return GetEmbedderRenderFrame()->GetWebFrame()->FirstChild();
}

// mime_handler::BeforeUnloadControl implementation.
void MimeHandlerViewFrameContainer::SetShowBeforeUnloadDialog(
    bool show_dialog,
    SetShowBeforeUnloadDialogCallback callback) {
  // TODO(ekaramad): Implement.
}

}  // namespace extensions
