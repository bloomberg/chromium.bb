// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WS_WINDOW_SERVICE_OWNER_H_
#define ASH_WS_WINDOW_SERVICE_OWNER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/shell_init_params.h"
#include "base/memory/scoped_refptr.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/ws/window_service.h"

namespace ws {
class GpuInterfaceProvider;
}  // namespace ws

namespace ash {

class WindowServiceDelegateImpl;

// WindowServiceOwner indirectly owns the WindowService. This class is
// responsible for responding to the ServiceRequest for the WindowService. When
// BindWindowService() is called the WindowService is created.
class ASH_EXPORT WindowServiceOwner {
 public:
  explicit WindowServiceOwner(
      std::unique_ptr<ws::GpuInterfaceProvider> gpu_interface_provider);
  ~WindowServiceOwner();

  // Called from the ServiceManager when a request is made for the
  // WindowService.
  void BindWindowService(service_manager::mojom::ServiceRequest request);

  ws::WindowService* window_service() { return &window_service_; }

 private:
  friend class AshTestHelper;

  std::unique_ptr<WindowServiceDelegateImpl> window_service_delegate_;
  ws::WindowService window_service_;

  DISALLOW_COPY_AND_ASSIGN(WindowServiceOwner);
};

}  // namespace ash

#endif  // ASH_WS_WINDOW_SERVICE_OWNER_H_
