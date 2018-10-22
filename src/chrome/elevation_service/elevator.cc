// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/elevator.h"

#include "base/files/file_util.h"
#include "base/process/process.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "chrome/elevation_service/service_main.h"
#include "chrome/install_static/install_util.h"

namespace elevation_service {

IFACEMETHODIMP Elevator::GetElevatorFactory(const base::char16* elevator_id,
                                            IClassFactory** factory) {
  DCHECK(elevator_id);
  DCHECK(factory);

  *factory = nullptr;

  elevation_service::ServiceMain* service =
      elevation_service::ServiceMain::GetInstance();
  Microsoft::WRL::ComPtr<IClassFactory> f =
      service->GetElevatorFactory(elevator_id);
  f.CopyTo(factory);

  return *factory ? S_OK : E_INVALIDARG;
}

Elevator::~Elevator() = default;

}  // namespace elevation_service
