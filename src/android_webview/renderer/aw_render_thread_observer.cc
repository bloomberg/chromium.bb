// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_render_thread_observer.h"

#include "components/power_scheduler/power_scheduler.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"

namespace android_webview {

AwRenderThreadObserver::AwRenderThreadObserver() {
}

AwRenderThreadObserver::~AwRenderThreadObserver() {
}

void AwRenderThreadObserver::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  // base::Unretained can be used here because the associated_interfaces
  // is owned by the RenderThread and will live for the duration of the
  // RenderThread.
  associated_interfaces->AddInterface(
      base::BindRepeating(&AwRenderThreadObserver::OnRendererAssociatedRequest,
                          base::Unretained(this)));
}

void AwRenderThreadObserver::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(mojom::Renderer::Name_);
}

void AwRenderThreadObserver::OnRendererAssociatedRequest(
    mojo::PendingAssociatedReceiver<mojom::Renderer> receiver) {
  receiver_.Bind(std::move(receiver));
}

void AwRenderThreadObserver::ClearCache() {
  blink::WebCache::Clear();
}

void AwRenderThreadObserver::SetJsOnlineProperty(bool network_up) {
  blink::WebNetworkStateNotifier::SetOnLine(network_up);
}

void AwRenderThreadObserver::SetCpuAffinityToLittleCores() {
  power_scheduler::PowerScheduler::GetInstance()->SetPolicy(
      power_scheduler::SchedulingPolicy::kLittleCoresOnly);
}

void AwRenderThreadObserver::EnableIdleThrottling(int32_t policy,
                                                  int32_t min_time_ms,
                                                  float min_cputime_ratio) {
  power_scheduler::SchedulingPolicyParams params{
      (power_scheduler::SchedulingPolicy)policy,
      base::TimeDelta::FromMilliseconds(min_time_ms), min_cputime_ratio};
  power_scheduler::PowerScheduler::GetInstance()->SetPolicy(params);
}

}  // namespace android_webview
