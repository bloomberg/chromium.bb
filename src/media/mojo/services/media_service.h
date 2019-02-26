// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MEDIA_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "media/base/media_log.h"
#include "media/mojo/interfaces/interface_factory.mojom.h"
#include "media/mojo/interfaces/media_service.mojom.h"
#include "media/mojo/services/deferred_destroy_strong_binding_set.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace media {

class MojoMediaClient;

class MEDIA_MOJO_EXPORT MediaService : public service_manager::Service,
                                       public mojom::MediaService {
 public:
  MediaService(std::unique_ptr<MojoMediaClient> mojo_media_client,
               service_manager::mojom::ServiceRequest request);
  ~MediaService() final;

 private:
  // service_manager::Service implementation.
  void OnStart() final;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;
  void OnDisconnected() final;

  void Create(mojom::MediaServiceRequest request);

  void CreateInterfaceFactory(
      mojom::InterfaceFactoryRequest request,
      service_manager::mojom::InterfaceProviderPtr host_interfaces) final;

  MediaLog media_log_;
  service_manager::ServiceBinding service_binding_;
  service_manager::ServiceKeepalive keepalive_;

  // Note: Since each instance runs on a different thread, do not share a common
  // MojoMediaClient with other instances to avoid threading issues. Hence using
  // a unique_ptr here.
  //
  // Note: Since |*ref_factory_| is passed to |mojo_media_client_|,
  // |mojo_media_client_| must be destructed before |ref_factory_|.
  std::unique_ptr<MojoMediaClient> mojo_media_client_;

  // Note: Since |&media_log_| is passed to bindings, the bindings must be
  // destructed first.
  DeferredDestroyStrongBindingSet<mojom::InterfaceFactory>
      interface_factory_bindings_;

  service_manager::BinderRegistry registry_;
  mojo::BindingSet<mojom::MediaService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(MediaService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_SERVICE_H_
