// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_IME_TEST_IME_DRIVER_TEST_IME_APPLICATION_H_
#define SERVICES_WS_IME_TEST_IME_DRIVER_TEST_IME_APPLICATION_H_

#include "base/macros.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace ws {
namespace test {

class TestIMEApplication : public service_manager::Service {
 public:
  explicit TestIMEApplication(service_manager::mojom::ServiceRequest request);
  ~TestIMEApplication() override;

 private:
  // service_manager::Service:
  void OnStart() override;

  service_manager::ServiceBinding service_binding_;

  DISALLOW_COPY_AND_ASSIGN(TestIMEApplication);
};

}  // namespace test
}  // namespace ws

#endif  // SERVICES_WS_IME_TEST_IME_DRIVER_TEST_IME_APPLICATION_H_
