// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_BRIDGE_FACTORY_HOST_H_
#define UI_VIEWS_COCOA_BRIDGE_FACTORY_HOST_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/views/views_export.h"
#include "ui/views_bridge_mac/mojo/bridge_factory.mojom.h"

namespace views {

class VIEWS_EXPORT BridgeFactoryHost {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnBridgeFactoryHostDestroying(BridgeFactoryHost* host) = 0;

   protected:
    ~Observer() override {}
  };

  BridgeFactoryHost(views_bridge_mac::mojom::BridgeFactoryRequest* request);
  ~BridgeFactoryHost();
  views_bridge_mac::mojom::BridgeFactory* GetFactory();
  void AddObserver(Observer* observer);
  void RemoveObserver(const Observer* observer);

 private:
  views_bridge_mac::mojom::BridgeFactoryPtr bridge_factory_ptr_;
  base::ObserverList<Observer> observers_;
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_BRIDGE_FACTORY_HOST_H_
