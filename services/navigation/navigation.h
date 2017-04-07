// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NAVIGATION_NAVIGATION_H_
#define SERVICES_NAVIGATION_NAVIGATION_H_

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "content/public/common/connection_filter.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/navigation/public/interfaces/view.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace navigation {

std::unique_ptr<service_manager::Service> CreateNavigationService();

class Navigation
    : public service_manager::Service,
      public mojom::ViewFactory,
      public service_manager::InterfaceFactory<mojom::ViewFactory> {
 public:
  Navigation();
  ~Navigation() override;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::ServiceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // mojom::ViewFactory:
  void CreateView(mojom::ViewClientPtr client,
                  mojom::ViewRequest request) override;

  void Create(const service_manager::Identity& remote_identity,
              mojom::ViewFactoryRequest request) override;

  void ViewFactoryLost();

  scoped_refptr<base::SequencedTaskRunner> view_task_runner_;

  std::string client_user_id_;

  service_manager::ServiceContextRefFactory ref_factory_;
  std::set<std::unique_ptr<service_manager::ServiceContextRef>> refs_;

  service_manager::BinderRegistry registry_;
  mojo::BindingSet<mojom::ViewFactory> bindings_;

  DISALLOW_COPY_AND_ASSIGN(Navigation);
};

}  // navigation

#endif  // SERVICES_NAVIGATION_NAVIGATION_H_
