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

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class DictionaryValue;
}

class SerialChooserContext : public ChooserContextBase {
 public:
  explicit SerialChooserContext(Profile* profile);
  ~SerialChooserContext() override;

  // ChooserContextBase implementation.
  bool IsValidObject(const base::DictionaryValue& object) override;
  std::string GetObjectName(const base::DictionaryValue& object) override;

  // In addition these methods from ChooserContextBase are overridden in order
  // to expose ephemeral devices through the public interface.
  std::vector<std::unique_ptr<Object>> GetGrantedObjects(
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  std::vector<std::unique_ptr<Object>> GetAllGrantedObjects() override;
  void RevokeObjectPermission(const GURL& requesting_origin,
                              const GURL& embedding_origin,
                              const base::DictionaryValue& object) override;

  // Serial-specific interface for granting and checking permissions.
  void GrantPortPermission(const url::Origin& requesting_origin,
                           const url::Origin& embedding_origin,
                           const device::mojom::SerialPortInfo& port);
  bool HasPortPermission(const url::Origin& requesting_origin,
                         const url::Origin& embedding_origin,
                         const device::mojom::SerialPortInfo& port);

  device::mojom::SerialPortManager* GetPortManager();

  void SetPortManagerForTesting(device::mojom::SerialPortManagerPtr manager);
  base::WeakPtr<SerialChooserContext> AsWeakPtr();

 private:
  void EnsurePortManagerConnection();
  void SetUpPortManagerConnection(device::mojom::SerialPortManagerPtr manager);
  void OnPortManagerConnectionError();
  void OnGetPorts(const url::Origin& requesting_origin,
                  const url::Origin& embedding_origin,
                  blink::mojom::SerialService::GetPortsCallback callback,
                  std::vector<device::mojom::SerialPortInfoPtr> ports);

  const bool is_incognito_;

  // Tracks the set of ports to which an origin (potentially embedded in another
  // origin) has access to. Key is (requesting_origin, embedding_origin).
  std::map<std::pair<url::Origin, url::Origin>,
           std::set<base::UnguessableToken>>
      ephemeral_ports_;

  // Holds information about ports in |ephemeral_ports_|.
  std::map<base::UnguessableToken, base::Value> port_info_;

  device::mojom::SerialPortManagerPtr port_manager_;

  base::WeakPtrFactory<SerialChooserContext> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SerialChooserContext);
};

#endif  // CHROME_BROWSER_SERIAL_SERIAL_CHOOSER_CONTEXT_H_
