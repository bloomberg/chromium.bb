// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_AX_HOST_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_AX_HOST_SERVICE_H_

#include <memory>

#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "ui/accessibility/ax_host_delegate.h"
#include "ui/accessibility/mojom/ax_host.mojom.h"

// Forwards accessibility events from clients in other processes that use aura
// and views (e.g. the Chrome OS keyboard shortcut_viewer) to accessibility
// extensions. Renderers, PDF, etc. use a different path. Created when the first
// client connects over mojo. Implements AXHostDelegate by routing actions over
// mojo to the remote process.
class AXHostService : public service_manager::Service,
                      public ax::mojom::AXHost,
                      public ui::AXHostDelegate {
 public:
  AXHostService();
  ~AXHostService() override;

  // Requests AX node trees from remote clients and starts listening for remote
  // AX events. Static because the mojo service_manager creates and owns the
  // service object, but automation may be enabled before a client connects and
  // the service starts.
  static void SetAutomationEnabled(bool enabled);

  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // ax::mojom::AXHost:
  void SetRemoteHost(ax::mojom::AXRemoteHostPtr remote) override;
  void HandleAccessibilityEvent(int32_t tree_id,
                                const std::vector<ui::AXTreeUpdate>& updates,
                                const ui::AXEvent& event) override;

  // ui::AXHostDelegate:
  void PerformAction(const ui::AXActionData& data) override;

  void FlushForTesting();

 private:
  void AddBinding(ax::mojom::AXHostRequest request);
  void NotifyAutomationEnabled();
  void OnRemoteHostDisconnected();

  static AXHostService* instance_;
  static bool automation_enabled_;

  service_manager::BinderRegistry registry_;
  mojo::BindingSet<ax::mojom::AXHost> bindings_;
  // TODO(jamescook): Support multiple remote hosts.
  mojo::InterfacePtr<ax::mojom::AXRemoteHost> remote_host_;

  DISALLOW_COPY_AND_ASSIGN(AXHostService);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_AX_HOST_SERVICE_H_
