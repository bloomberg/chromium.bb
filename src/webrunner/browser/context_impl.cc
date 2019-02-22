// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/browser/context_impl.h"

#include <lib/zx/object.h>
#include <memory>
#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "content/public/browser/web_contents.h"
#include "webrunner/browser/frame_impl.h"

namespace webrunner {

ContextImpl::ContextImpl(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ContextImpl::~ContextImpl() = default;

void ContextImpl::CreateFrame(
    fidl::InterfaceRequest<chromium::web::Frame> frame_request) {
  auto web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(browser_context_, nullptr));
  frames_.insert(std::make_unique<FrameImpl>(std::move(web_contents), this,
                                             std::move(frame_request)));
}

void ContextImpl::DestroyFrame(FrameImpl* frame) {
  DCHECK(frames_.find(frame) != frames_.end());
  frames_.erase(frames_.find(frame));
}

FrameImpl* ContextImpl::GetFrameImplForTest(
    chromium::web::FramePtr* frame_ptr) {
  DCHECK(frame_ptr);

  // Find the FrameImpl whose channel is connected to |frame_ptr| by inspecting
  // the related_koids of active FrameImpls.
  zx_info_handle_basic_t handle_info;
  zx_status_t status = frame_ptr->channel().get_info(
      ZX_INFO_HANDLE_BASIC, &handle_info, sizeof(zx_info_handle_basic_t),
      nullptr, nullptr);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_get_info";
  zx_handle_t client_handle_koid = handle_info.koid;

  for (const std::unique_ptr<FrameImpl>& frame : frames_) {
    status = frame->GetBindingChannelForTest()->get_info(
        ZX_INFO_HANDLE_BASIC, &handle_info, sizeof(zx_info_handle_basic_t),
        nullptr, nullptr);
    ZX_CHECK(status == ZX_OK, status) << "zx_object_get_info";

    if (client_handle_koid == handle_info.related_koid)
      return frame.get();
  }

  return nullptr;
}

}  // namespace webrunner
