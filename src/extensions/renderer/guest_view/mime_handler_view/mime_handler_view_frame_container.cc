// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_frame_container.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/pickle.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

namespace {

bool IsSupportedMimeType(const std::string& mime_type) {
  return mime_type == "text/pdf" || mime_type == "application/pdf" ||
         mime_type == "text/csv";
}

}  // namespace

// static
bool MimeHandlerViewFrameContainer::Create(
    const blink::WebElement& plugin_element,
    const GURL& resource_url,
    const std::string& mime_type,
    const content::WebPluginInfo& plugin_info,
    int32_t element_instance_id) {
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
                                           mime_type, plugin_info,
                                           element_instance_id);
}

MimeHandlerViewFrameContainer::MimeHandlerViewFrameContainer(
    const blink::WebElement& plugin_element,
    const GURL& resource_url,
    const std::string& mime_type,
    const content::WebPluginInfo& plugin_info,
    int32_t element_instance_id)
    : MimeHandlerViewContainerBase(content::RenderFrame::FromWebFrame(
                                       plugin_element.GetDocument().GetFrame()),
                                   plugin_info,
                                   mime_type,
                                   resource_url),
      plugin_element_(plugin_element),
      element_instance_id_(element_instance_id) {
  is_embedded_ = IsEmbedded();
  if (is_embedded_) {
    SendResourceRequest();
  } else {
    // For non-embedded MimeHandlerViewGuest the stream has already been
    // intercepted.
    // TODO(ekaramad): Update |view_id_| before sending this request.
    CreateMimeHandlerViewGuestIfNecessary();
  }
}

MimeHandlerViewFrameContainer::~MimeHandlerViewFrameContainer() {}

void MimeHandlerViewFrameContainer::CreateMimeHandlerViewGuestIfNecessary() {
  if (auto* frame = GetContentFrame()) {
    plugin_frame_routing_id_ =
        content::RenderFrame::GetRoutingIdForWebFrame(frame);
  }
  if (plugin_frame_routing_id_ == MSG_ROUTING_NONE) {
    // TODO(ekaramad): Destroy and cleanup.
    return;
  }
  MimeHandlerViewContainerBase::CreateMimeHandlerViewGuestIfNecessary();
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
  return blink::WebFrame::FromFrameOwnerElement(plugin_element_);
}
// mime_handler::BeforeUnloadControl implementation.
void MimeHandlerViewFrameContainer::SetShowBeforeUnloadDialog(
    bool show_dialog,
    SetShowBeforeUnloadDialogCallback callback) {
  // TODO(ekaramad): Implement.
}

bool MimeHandlerViewFrameContainer::IsEmbedded() const {
  // TODO(ekaramad): This is currently sending a request regardless of whether
  // or not this embed is due to frame navigation to resource. For such cases,
  // the renderer has already started a resource request and we should not send
  // twice. Find a way to get the intercepted stream and avoid sending an extra
  // request here.
  return true;
}

}  // namespace extensions
