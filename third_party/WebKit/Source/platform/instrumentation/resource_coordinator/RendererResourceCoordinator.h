// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RendererResourceCoordinator_h
#define RendererResourceCoordinator_h

#include "platform/instrumentation/resource_coordinator/BlinkResourceCoordinatorBase.h"

#include "platform/wtf/Noncopyable.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace blink {

class PLATFORM_EXPORT RendererResourceCoordinator
    : public BlinkResourceCoordinatorBase {
  WTF_MAKE_NONCOPYABLE(RendererResourceCoordinator);

 public:
  static void Initialize();
  static RendererResourceCoordinator& Get();

  // Used to switch the current renderer resource coordinator only for testing.
  static void SetCurrentRendererResourceCoordinatorForTesting(
      RendererResourceCoordinator*);

  ~RendererResourceCoordinator() override;

 protected:
  RendererResourceCoordinator();

 private:
  RendererResourceCoordinator(service_manager::Connector*,
                              const std::string& service_name);
};

}  // namespace blink

#endif  // RendererResourceCoordinator_h
