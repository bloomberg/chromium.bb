// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_HID_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_HID_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/observer_list_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/hid_chooser.h"
#include "services/device/public/mojom/hid.mojom-forward.h"
#include "third_party/blink/public/mojom/hid/hid.mojom-forward.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class RenderFrameHost;

class CONTENT_EXPORT HidDelegate {
 public:
  virtual ~HidDelegate() = default;

  class Observer : public base::CheckedObserver {
   public:
    // Events forwarded from HidChooserContext::DeviceObserver:
    virtual void OnDeviceAdded(const device::mojom::HidDeviceInfo&) = 0;
    virtual void OnDeviceRemoved(const device::mojom::HidDeviceInfo&) = 0;
    virtual void OnDeviceChanged(const device::mojom::HidDeviceInfo&) = 0;
    virtual void OnHidManagerConnectionError() = 0;

    // Event forwarded from permissions::ChooserContextBase::PermissionObserver:
    virtual void OnPermissionRevoked(const url::Origin& origin) = 0;
  };

  // Shows a chooser for the user to select a HID device. |callback| will be
  // run when the prompt is closed. Deleting the returned object will cancel the
  // prompt.
  virtual std::unique_ptr<HidChooser> RunChooser(
      RenderFrameHost* render_frame_host,
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      HidChooser::Callback callback) = 0;

  // Returns whether the main frame of |render_frame_host| has permission to
  // request access to a device.
  virtual bool CanRequestDevicePermission(
      RenderFrameHost* render_frame_host) = 0;

  // Returns whether the main frame of |render_frame_host| has permission to
  // access |device|.
  virtual bool HasDevicePermission(
      RenderFrameHost* render_frame_host,
      const device::mojom::HidDeviceInfo& device) = 0;

  // Revoke `device` access permission to the main frame of `render_frame_host`.
  virtual void RevokeDevicePermission(
      RenderFrameHost* render_frame_host,
      const device::mojom::HidDeviceInfo& device) = 0;

  // Returns an open connection to the HidManager interface owned by the
  // embedder and being used to serve requests from |render_frame_host|.
  //
  // Content and the embedder must use the same connection so that the embedder
  // can process connect/disconnect events for permissions management purposes
  // before they are delivered to content. Otherwise race conditions are
  // possible.
  virtual device::mojom::HidManager* GetHidManager(
      RenderFrameHost* render_frame_host) = 0;

  // Functions to manage the set of Observer instances registered to this
  // object.
  virtual void AddObserver(RenderFrameHost* render_frame_host,
                           Observer* observer) = 0;
  virtual void RemoveObserver(RenderFrameHost* render_frame_host,
                              Observer* observer) = 0;

  // Returns true if |origin| is allowed to bypass the HID blocklist and
  // access reports contained in FIDO collections.
  virtual bool IsFidoAllowedForOrigin(RenderFrameHost* render_frame_host,
                                      const url::Origin& origin) = 0;

  // Gets the device info for a particular device, identified by its guid.
  virtual const device::mojom::HidDeviceInfo* GetDeviceInfo(
      RenderFrameHost* render_frame_host,
      const std::string& guid) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_HID_DELEGATE_H_
