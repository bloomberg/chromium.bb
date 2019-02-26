// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_UNZIP_UNZIP_SERVICE_H_
#define COMPONENTS_SERVICES_UNZIP_UNZIP_SERVICE_H_

#include "base/macros.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_keepalive.h"

namespace unzip {

class UnzipService : public service_manager::Service {
 public:
  explicit UnzipService(service_manager::mojom::ServiceRequest request);
  ~UnzipService() override;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  service_manager::ServiceBinding binding_;
  service_manager::ServiceKeepalive keepalive_;

  DISALLOW_COPY_AND_ASSIGN(UnzipService);
};

}  // namespace unzip

#endif  // COMPONENTS_SERVICES_UNZIP_UNZIP_SERVICE_H_
