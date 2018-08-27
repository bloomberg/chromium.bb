// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_IME_TEST_IME_DRIVER_TEST_IME_DRIVER_H_
#define SERVICES_WS_IME_TEST_IME_DRIVER_TEST_IME_DRIVER_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "services/ws/public/mojom/ime/ime.mojom.h"

namespace ui {
namespace test {

class TestIMEDriver : public ws::mojom::IMEDriver {
 public:
  TestIMEDriver();
  ~TestIMEDriver() override;

 private:
  // ws::mojom::IMEDriver:
  void StartSession(ws::mojom::StartSessionDetailsPtr details) override;

  DISALLOW_COPY_AND_ASSIGN(TestIMEDriver);
};

}  // namespace test
}  // namespace ui

#endif  // SERVICES_WS_IME_TEST_IME_DRIVER_TEST_IME_DRIVER_H_
