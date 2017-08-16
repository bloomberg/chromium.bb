// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkResourceCoordinatorBase_h
#define BlinkResourceCoordinatorBase_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Noncopyable.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom-blink.h"

namespace service_manager {
class InterfaceProvider;
class Connector;
}  // namespace service_manager

namespace blink {

class InterfaceProvider;

// Base class for Resource Coordinators in Blink.
class PLATFORM_EXPORT BlinkResourceCoordinatorBase {
  WTF_MAKE_NONCOPYABLE(BlinkResourceCoordinatorBase);

 public:
  static bool IsEnabled();
  virtual ~BlinkResourceCoordinatorBase();

  void SetProperty(const resource_coordinator::mojom::blink::PropertyType,
                   int64_t);

 protected:
  explicit BlinkResourceCoordinatorBase(service_manager::InterfaceProvider*);
  BlinkResourceCoordinatorBase(service_manager::Connector*,
                               const std::string& service_name);
  BlinkResourceCoordinatorBase();

 private:
  resource_coordinator::mojom::blink::CoordinationUnitPtr service_;
};

}  // namespace blink

#endif  // BlinkResourceCoordinatorBase_h
