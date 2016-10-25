// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DEMO_MUS_DEMO_SERVICE_H_
#define SERVICES_UI_DEMO_MUS_DEMO_SERVICE_H_

#include "base/macros.h"
#include "services/service_manager/public/cpp/service.h"

namespace ui {
namespace demo {

class MusDemo;

// A simple MUS Demo mojo app. See the MusDemo class for more details.
class MusDemoService : public service_manager::Service {
 public:
  MusDemoService();
  ~MusDemoService() override;

 private:
  // Overrides shell::Service:
  void OnStart(const service_manager::ServiceInfo& info) override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  std::unique_ptr<MusDemo> mus_demo_;

  DISALLOW_COPY_AND_ASSIGN(MusDemoService);
};

}  // namespace demo
}  // namespace ui

#endif  // SERVICES_UI_DEMO_MUS_DEMO_SERVICE_H_
