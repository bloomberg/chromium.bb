// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/video_capture/service_impl.h"

MojoResult ServiceMain(MojoHandle service_request_handle) {
  base::MessageLoop message_loop;
  base::RunLoop run_loop;
  video_capture::ServiceImpl service{service_manager::mojom::ServiceRequest(
      mojo::MakeScopedHandle(mojo::MessagePipeHandle(service_request_handle)))};
  service.set_termination_closure(run_loop.QuitClosure());
  run_loop.Run();
  return MOJO_RESULT_OK;
}
