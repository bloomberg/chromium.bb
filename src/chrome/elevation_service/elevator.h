// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELEVATION_SERVICE_ELEVATOR_H_
#define CHROME_ELEVATION_SERVICE_ELEVATOR_H_

#include <windows.h>

#include <wrl/implements.h>
#include <wrl/module.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/elevation_service/elevation_service_idl.h"

namespace elevation_service {

class Elevator
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IElevator> {
 public:
  Elevator() = default;

  IFACEMETHOD(GetElevatorFactory)(const base::char16* elevator_id,
                                  IClassFactory** factory);

 private:
  ~Elevator() override;

  DISALLOW_COPY_AND_ASSIGN(Elevator);
};

}  // namespace elevation_service

#endif  // CHROME_ELEVATION_SERVICE_ELEVATOR_H_
