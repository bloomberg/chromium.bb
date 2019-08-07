// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_BRIDGE_FACTORY_HOST_H_
#define UI_VIEWS_COCOA_BRIDGE_FACTORY_HOST_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/remote_cocoa/common/bridge_factory.mojom.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT BridgeFactoryHost {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnBridgeFactoryHostDestroying(BridgeFactoryHost* host) = 0;

   protected:
    ~Observer() override {}
  };

  BridgeFactoryHost(
      remote_cocoa::mojom::BridgeFactoryAssociatedRequest* request);
  ~BridgeFactoryHost();

  remote_cocoa::mojom::BridgeFactory* GetFactory();

  void AddObserver(Observer* observer);
  void RemoveObserver(const Observer* observer);

 private:
  remote_cocoa::mojom::BridgeFactoryAssociatedPtr bridge_factory_ptr_;
  base::ObserverList<Observer> observers_;
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_BRIDGE_FACTORY_HOST_H_
