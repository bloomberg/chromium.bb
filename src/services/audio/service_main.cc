// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "services/audio/service.h"
#include "services/audio/service_factory.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/mojom/service.mojom.h"

MojoResult ServiceMain(MojoHandle service_request_handle) {
  base::MessageLoop message_loop;
  base::RunLoop run_loop;
  std::unique_ptr<audio::Service> service = audio::CreateStandaloneService(
      std::make_unique<service_manager::BinderRegistry>(),
      service_manager::mojom::ServiceRequest(mojo::ScopedMessagePipeHandle(
          mojo::MessagePipeHandle(service_request_handle))));
  service->set_termination_closure(run_loop.QuitClosure());
  run_loop.Run();
  return MOJO_RESULT_OK;
}
