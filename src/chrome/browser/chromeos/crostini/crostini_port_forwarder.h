// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PORT_FORWARDER_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PORT_FORWARDER_H_

#include <string>

#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace crostini {

extern const char kDefaultInterfaceToForward[];
extern const char kWlanInterface[];
extern const char kPortNumberKey[];
extern const char kPortProtocolKey[];
extern const char kPortInterfaceKey[];
extern const char kPortLabelKey[];
extern const char kPortVmNameKey[];
extern const char kPortContainerNameKey[];

class CrostiniPortForwarder : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a port's active state changes.
    virtual void OnActivePortsChanged(const base::ListValue& activePorts) = 0;
  };

  enum class Protocol {
    TCP = 0,
    UDP = 1,
  };

  struct PortRuleKey {
    uint16_t port_number;
    Protocol protocol_type;
    std::string input_ifname;
    ContainerId container_id;

    bool operator==(const PortRuleKey& other) const {
      return port_number == other.port_number &&
             protocol_type == other.protocol_type &&
             input_ifname == other.input_ifname;
    }
  };

  // Helper for using PortRuleKey as key entries in std::unordered_maps.
  struct PortRuleKeyHasher {
    std::size_t operator()(const PortRuleKey& k) const {
      return ((std::hash<uint16_t>()(k.port_number) ^
               (std::hash<Protocol>()(k.protocol_type) << 1)) >>
              1) ^
             (std::hash<std::string>()(k.input_ifname) << 1);
    }
  };

  using ResultCallback = base::OnceCallback<void(bool)>;

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // The result_callback will only be called with success=true IF all conditions
  // pass. This means a port setting has been successfully updated in the
  // iptables and the profile preference setting has also been successfully
  // updated.
  void ActivatePort(const ContainerId& container_id,
                    uint16_t port_number,
                    const Protocol& protocol_type,
                    ResultCallback result_callback);
  void AddPort(const ContainerId& container_id,
               uint16_t port_number,
               const Protocol& protocol_type,
               const std::string& label,
               ResultCallback result_callback);
  void DeactivatePort(const ContainerId& container_id,
                      uint16_t port_number,
                      const Protocol& protocol_type,
                      ResultCallback result_callback);
  void RemovePort(const ContainerId& container_id,
                  uint16_t port_number,
                  const Protocol& protocol_type,
                  ResultCallback result_callback);

  // TODO(matterchen): For the two following methods, implement callback
  // results.

  // Deactivate all ports belonging to the container_id and removes them from
  // the preferences.
  void RemoveAllPorts(const ContainerId& container_id);

  // Deactivate all active ports belonging to the container_id and set their
  // preference to inactive such that these ports will not be automatically
  // re-forwarded on re-startup. This is called on container shutdown.
  void DeactivateAllActivePorts(const ContainerId& container_id);

  base::ListValue GetActivePorts();

  size_t GetNumberOfForwardedPortsForTesting();
  base::Optional<base::Value> ReadPortPreferenceForTesting(
      const PortRuleKey& key);

  static CrostiniPortForwarder* GetForProfile(Profile* profile);

  explicit CrostiniPortForwarder(Profile* profile);
  ~CrostiniPortForwarder() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CrostiniPortForwarderTest,
                           TryActivatePortPermissionBrokerClientFail);
  FRIEND_TEST_ALL_PREFIXES(CrostiniPortForwarderTest, GetActivePortsForUI);

  void SignalActivePortsChanged();
  bool MatchPortRuleDict(const base::Value& dict, const PortRuleKey& key);
  bool MatchPortRuleContainerId(const base::Value& dict,
                                const ContainerId& container_id);
  void AddNewPortPreference(const PortRuleKey& key, const std::string& label);
  bool RemovePortPreference(const PortRuleKey& key);
  base::Optional<base::Value> ReadPortPreference(const PortRuleKey& key);

  void OnAddOrActivatePortCompleted(ResultCallback result_callback,
                                    PortRuleKey key,
                                    bool success);
  void OnRemoveOrDeactivatePortCompleted(ResultCallback result_callback,
                                         PortRuleKey key,
                                         bool success);
  void TryDeactivatePort(const PortRuleKey& key,
                         const ContainerId& container_id,
                         base::OnceCallback<void(bool)> result_callback);
  void TryActivatePort(const PortRuleKey& key,
                       const ContainerId& container_id,
                       base::OnceCallback<void(bool)> result_callback);

  // For each port rule (protocol, port, interface), keep track of the fd which
  // requested it so we can release it on removal / deactivate.
  std::unordered_map<PortRuleKey, base::ScopedFD, PortRuleKeyHasher>
      forwarded_ports_;

  base::ObserverList<Observer> observers_;

  Profile* profile_;

  base::WeakPtrFactory<CrostiniPortForwarder> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniPortForwarder);

};  // class

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PORT_FORWARDER_H_
