// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BRIDGE_MAC_ALERT_H_
#define UI_VIEWS_BRIDGE_MAC_ALERT_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "components/remote_cocoa/common/alert.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/gfx/text_elider.h"
#include "ui/views_bridge_mac/views_bridge_mac_export.h"

@class AlertBridgeHelper;

namespace views_bridge_mac {

// Class that displays an NSAlert with associated UI as described by the mojo
// AlertBridge interface.
class VIEWS_BRIDGE_MAC_EXPORT AlertBridge
    : public views_bridge_mac::mojom::AlertBridge {
 public:
  // Creates a new alert which controls its own lifetime. It will destroy itself
  // once its NSAlert goes away.
  AlertBridge(mojom::AlertBridgeRequest bridge_request);

  // Send the specified disposition via the Show callback, then destroy |this|.
  void SendResultAndDestroy(mojom::AlertDisposition disposition);

  // Called by Cocoa to indicate when the NSAlert is visible (and can be
  // programmatically updated by Accept, Cancel, and Close).
  void SetAlertHasShown();

 private:
  // Private destructor is called only through SendResultAndDestroy.
  ~AlertBridge() override;

  // Handle being disconnected (e.g, because the alert was programmatically
  // dismissed).
  void OnConnectionError();

  // views_bridge_mac::mojom::Alert:
  void Show(mojom::AlertBridgeInitParamsPtr params,
            ShowCallback callback) override;

  // The NSAlert's owner and delegate.
  base::scoped_nsobject<AlertBridgeHelper> helper_;

  // Set once the alert window is showing (needed because showing is done in a
  // posted task).
  bool alert_shown_ = false;

  // The callback to make when the dialog has finished running.
  ShowCallback callback_;

  mojo::Binding<views_bridge_mac::mojom::AlertBridge> mojo_binding_;
  base::WeakPtrFactory<AlertBridge> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(AlertBridge);
};

}  // namespace views_bridge_mac

#endif  // UI_VIEWS_BRIDGE_MAC_ALERT_H_
