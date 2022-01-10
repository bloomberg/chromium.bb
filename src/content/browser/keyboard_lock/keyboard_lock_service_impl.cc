// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/keyboard_lock/keyboard_lock_service_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/keyboard_lock/keyboard_lock_metrics.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/content_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

using blink::mojom::GetKeyboardLayoutMapResult;
using blink::mojom::KeyboardLockRequestResult;

namespace content {

namespace {

void LogKeyboardLockMethodCalled(KeyboardLockMethods method) {
  UMA_HISTOGRAM_ENUMERATION(kKeyboardLockMethodCalledHistogramName, method,
                            KeyboardLockMethods::kCount);
}

}  // namespace

KeyboardLockServiceImpl::KeyboardLockServiceImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::KeyboardLockService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)),
      render_frame_host_(static_cast<RenderFrameHostImpl*>(render_frame_host)) {
  DCHECK(render_frame_host_);
}

// static
void KeyboardLockServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::KeyboardLockService> receiver) {
  DCHECK(render_frame_host);

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new KeyboardLockServiceImpl(render_frame_host, std::move(receiver));
}

void KeyboardLockServiceImpl::RequestKeyboardLock(
    const std::vector<std::string>& key_codes,
    RequestKeyboardLockCallback callback) {
  if (key_codes.empty())
    LogKeyboardLockMethodCalled(KeyboardLockMethods::kRequestAllKeys);
  else
    LogKeyboardLockMethodCalled(KeyboardLockMethods::kRequestSomeKeys);

  if (render_frame_host_->GetParentOrOuterDocument()) {
    std::move(callback).Run(KeyboardLockRequestResult::kChildFrameError);
    return;
  }

  if (!render_frame_host_->IsActive()) {
    std::move(callback).Run(KeyboardLockRequestResult::kFrameDetachedError);
    return;
  }

  // Per base::flat_set usage notes, the proper way to init a flat_set is
  // inserting into a vector and using that to init the flat_set.
  std::vector<ui::DomCode> dom_codes;
  bool invalid_key_code_found = false;
  for (const std::string& code : key_codes) {
    ui::DomCode dom_code = ui::KeycodeConverter::CodeStringToDomCode(code);
    if (dom_code != ui::DomCode::NONE) {
      dom_codes.push_back(dom_code);
    } else {
      invalid_key_code_found = true;
      render_frame_host_->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "Invalid DOMString passed into keyboard.lock(): '" + code + "'");
    }
  }

  // If we are provided with a vector containing one or more invalid key codes,
  // then exit without enabling keyboard lock.  Also cancel any previous
  // keyboard lock request since the most recent request failed.
  if (invalid_key_code_found) {
    render_frame_host_->GetRenderWidgetHost()->CancelKeyboardLock();
    std::move(callback).Run(KeyboardLockRequestResult::kNoValidKeyCodesError);
    return;
  }

  absl::optional<base::flat_set<ui::DomCode>> dom_code_set;
  if (!dom_codes.empty())
    dom_code_set = std::move(dom_codes);

  if (render_frame_host_->GetRenderWidgetHost()->RequestKeyboardLock(
          std::move(dom_code_set))) {
    std::move(callback).Run(KeyboardLockRequestResult::kSuccess);
    feature_handle_ =
        static_cast<RenderFrameHostImpl*>(render_frame_host_)
            ->RegisterBackForwardCacheDisablingNonStickyFeature(
                blink::scheduler::WebSchedulerTrackedFeature::kKeyboardLock);
  } else {
    std::move(callback).Run(KeyboardLockRequestResult::kRequestFailedError);
  }
}

void KeyboardLockServiceImpl::CancelKeyboardLock() {
  LogKeyboardLockMethodCalled(KeyboardLockMethods::kCancelLock);
  render_frame_host_->GetRenderWidgetHost()->CancelKeyboardLock();
  feature_handle_.reset();
}

void KeyboardLockServiceImpl::GetKeyboardLayoutMap(
    GetKeyboardLayoutMapCallback callback) {
  auto response = GetKeyboardLayoutMapResult::New();
  // The keyboard layout map is only accessible from the outermost main frame or
  // with the permission policy enabled.
  if (render_frame_host_->GetParentOrOuterDocument() &&
      !render_frame_host_->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kKeyboardMap)) {
    response->status = blink::mojom::GetKeyboardLayoutMapStatus::kDenied;
    std::move(callback).Run(std::move(response));
    return;
  }
  response->status = blink::mojom::GetKeyboardLayoutMapStatus::kSuccess;
  response->layout_map = render_frame_host_->GetKeyboardLayoutMap();

  std::move(callback).Run(std::move(response));
}

KeyboardLockServiceImpl::~KeyboardLockServiceImpl() = default;

}  // namespace content
