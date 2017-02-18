// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_SHAPE_DETECTION_SERVICE_H_
#define SERVICES_SHAPE_DETECTION_SHAPE_DETECTION_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace shape_detection {

class ShapeDetectionService : public service_manager::Service {
 public:
  // Factory function for use as an embedded service.
  static std::unique_ptr<service_manager::Service> Create();

  ShapeDetectionService();
  ~ShapeDetectionService() override;

  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

 private:
  std::unique_ptr<service_manager::ServiceContextRefFactory> ref_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShapeDetectionService);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_SHAPE_DETECTION_SERVICE_H_
