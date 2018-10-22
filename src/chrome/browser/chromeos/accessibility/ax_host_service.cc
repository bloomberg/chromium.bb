// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/ax_host_service.h"

#include "base/bind.h"
#include "chrome/browser/extensions/api/automation_internal/automation_event_router.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "ui/accessibility/ax_event.h"
#include "ui/aura/env.h"
#include "ui/views/mus/ax_remote_host.h"

AXHostService* AXHostService::instance_ = nullptr;

bool AXHostService::automation_enabled_ = false;

AXHostService::AXHostService()
    : ui::AXHostDelegate(views::AXRemoteHost::kRemoteAXTreeID) {
  DCHECK(!instance_);
  instance_ = this;
  registry_.AddInterface<ax::mojom::AXHost>(
      base::BindRepeating(&AXHostService::AddBinding, base::Unretained(this)));
}

AXHostService::~AXHostService() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

// static
void AXHostService::SetAutomationEnabled(bool enabled) {
  automation_enabled_ = enabled;
  if (instance_)
    instance_->NotifyAutomationEnabled();
}

void AXHostService::OnBindInterface(
    const service_manager::BindSourceInfo& remote_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void AXHostService::SetRemoteHost(ax::mojom::AXRemoteHostPtr remote) {
  remote_host_ = std::move(remote);

  // Handle both clean and unclean shutdown.
  remote_host_.set_connection_error_handler(base::BindOnce(
      &AXHostService::OnRemoteHostDisconnected, base::Unretained(this)));

  // Ensure remote host knows the initial state.
  remote_host_->OnAutomationEnabled(automation_enabled_);
}

void AXHostService::HandleAccessibilityEvent(
    int32_t tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const ui::AXEvent& event) {
  DCHECK_EQ(tree_id, views::AXRemoteHost::kRemoteAXTreeID);
  ExtensionMsg_AccessibilityEventBundleParams event_bundle;
  event_bundle.tree_id = tree_id;
  for (const ui::AXTreeUpdate& update : updates)
    event_bundle.updates.push_back(update);
  event_bundle.events.push_back(event);
  event_bundle.mouse_location = aura::Env::GetInstance()->last_mouse_location();

  // Forward the tree updates and the event to the accessibility extension.
  extensions::AutomationEventRouter::GetInstance()->DispatchAccessibilityEvents(
      event_bundle);
}

void AXHostService::PerformAction(const ui::AXActionData& data) {
  // TODO(jamescook): This assumes a single remote host. Need to have one
  // AXHostDelegate per remote host and only send to the appropriate one.
  if (remote_host_)
    remote_host_->PerformAction(data);
}

void AXHostService::FlushForTesting() {
  remote_host_.FlushForTesting();
}

void AXHostService::AddBinding(ax::mojom::AXHostRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AXHostService::NotifyAutomationEnabled() {
  if (remote_host_)
    remote_host_->OnAutomationEnabled(automation_enabled_);
}

void AXHostService::OnRemoteHostDisconnected() {
  extensions::AutomationEventRouter::GetInstance()->DispatchTreeDestroyedEvent(
      views::AXRemoteHost::kRemoteAXTreeID, nullptr /* browser_context */);
}
