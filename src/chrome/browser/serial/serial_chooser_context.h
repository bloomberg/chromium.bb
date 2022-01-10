// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERIAL_SERIAL_CHOOSER_CONTEXT_H_
#define CHROME_BROWSER_SERIAL_SERIAL_CHOOSER_CONTEXT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/permissions/object_permission_context_base.h"
#include "content/public/browser/serial_delegate.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/serial.mojom-forward.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;

namespace base {
class Value;
}

class SerialChooserContext : public permissions::ObjectPermissionContextBase,
                             public device::mojom::SerialPortManagerClient {
 public:
  using PortObserver = content::SerialDelegate::Observer;

  explicit SerialChooserContext(Profile* profile);

  SerialChooserContext(const SerialChooserContext&) = delete;
  SerialChooserContext& operator=(const SerialChooserContext&) = delete;

  ~SerialChooserContext() override;

  // ObjectPermissionContextBase:
  std::string GetKeyForObject(const base::Value& object) override;
  bool IsValidObject(const base::Value& object) override;
  std::u16string GetObjectDisplayName(const base::Value& object) override;

  // In addition these methods from ObjectPermissionContextBase are overridden
  // in order to expose ephemeral devices through the public interface.
  std::vector<std::unique_ptr<Object>> GetGrantedObjects(
      const url::Origin& origin) override;
  std::vector<std::unique_ptr<Object>> GetAllGrantedObjects() override;
  void RevokeObjectPermission(const url::Origin& origin,
                              const base::Value& object) override;

  // Serial-specific interface for granting and checking permissions.
  void GrantPortPermission(const url::Origin& origin,
                           const device::mojom::SerialPortInfo& port);
  bool HasPortPermission(const url::Origin& origin,
                         const device::mojom::SerialPortInfo& port);
  static bool CanStorePersistentEntry(
      const device::mojom::SerialPortInfo& port);

  device::mojom::SerialPortManager* GetPortManager();

  void AddPortObserver(PortObserver* observer);
  void RemovePortObserver(PortObserver* observer);

  void SetPortManagerForTesting(
      mojo::PendingRemote<device::mojom::SerialPortManager> manager);
  void FlushPortManagerConnectionForTesting();
  base::WeakPtr<SerialChooserContext> AsWeakPtr();

  // SerialPortManagerClient implementation.
  void OnPortAdded(device::mojom::SerialPortInfoPtr port) override;
  void OnPortRemoved(device::mojom::SerialPortInfoPtr port) override;

 private:
  void EnsurePortManagerConnection();
  void SetUpPortManagerConnection(
      mojo::PendingRemote<device::mojom::SerialPortManager> manager);
  void OnPortManagerConnectionError();
  bool CanApplyPortSpecificPolicy();

  // This raw pointer is safe because instances of this class are created by
  // SerialChooserContextFactory as KeyedServices that will be destroyed when
  // the Profile object is destroyed.
  const raw_ptr<Profile> profile_;

  // Tracks the set of ports to which an origin has access to.
  std::map<url::Origin, std::set<base::UnguessableToken>> ephemeral_ports_;

  // Holds information about ports in |ephemeral_ports_|.
  std::map<base::UnguessableToken, base::Value> port_info_;

  mojo::Remote<device::mojom::SerialPortManager> port_manager_;
  mojo::Receiver<device::mojom::SerialPortManagerClient> client_receiver_{this};
  base::ObserverList<PortObserver> port_observer_list_;

  base::WeakPtrFactory<SerialChooserContext> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SERIAL_SERIAL_CHOOSER_CONTEXT_H_
