// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_IME_TEST_IME_DRIVER_TEST_IME_DRIVER_H_
#define SERVICES_WS_IME_TEST_IME_DRIVER_TEST_IME_DRIVER_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "services/ws/public/mojom/ime/ime.mojom.h"

namespace ws {
namespace test {

class TestIMEDriver : public mojom::IMEDriver {
 public:
  TestIMEDriver();
  ~TestIMEDriver() override;

 private:
  // mojom::IMEDriver:
  void StartSession(mojom::InputMethodRequest input_method_request,
                    mojom::TextInputClientPtr client,
                    mojom::SessionDetailsPtr details) override;

  DISALLOW_COPY_AND_ASSIGN(TestIMEDriver);
};

}  // namespace test
}  // namespace ws

#endif  // SERVICES_WS_IME_TEST_IME_DRIVER_TEST_IME_DRIVER_H_
