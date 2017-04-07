// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/navigation/navigation.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/navigation/view_impl.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace navigation {

namespace {

void CreateViewOnViewTaskRunner(
    std::unique_ptr<service_manager::Connector> connector,
    const std::string& client_user_id,
    mojom::ViewClientPtr client,
    mojom::ViewRequest request,
    std::unique_ptr<service_manager::ServiceContextRef> context_ref) {
  mojo::MakeStrongBinding(
      base::MakeUnique<ViewImpl>(std::move(connector), client_user_id,
                                 std::move(client), std::move(context_ref)),
      std::move(request));
}

}  // namespace

std::unique_ptr<service_manager::Service> CreateNavigationService() {
  return base::MakeUnique<Navigation>();
}

Navigation::Navigation()
    : view_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      ref_factory_(base::MessageLoop::QuitWhenIdleClosure()) {
  bindings_.set_connection_error_handler(
      base::Bind(&Navigation::ViewFactoryLost, base::Unretained(this)));
  registry_.AddInterface<mojom::ViewFactory>(this);
}
Navigation::~Navigation() {}

void Navigation::OnBindInterface(
    const service_manager::ServiceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  std::string remote_user_id = source_info.identity.user_id();
  if (!client_user_id_.empty() && client_user_id_ != remote_user_id) {
    LOG(ERROR) << "Must have a separate Navigation service instance for "
               << "different BrowserContexts.";
    return;
  }
  client_user_id_ = remote_user_id;
  registry_.BindInterface(source_info.identity, interface_name,
                          std::move(interface_pipe));
}

void Navigation::CreateView(mojom::ViewClientPtr client,
                            mojom::ViewRequest request) {
  std::unique_ptr<service_manager::Connector> new_connector =
      context()->connector()->Clone();
  std::unique_ptr<service_manager::ServiceContextRef> context_ref =
      ref_factory_.CreateRef();
  view_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CreateViewOnViewTaskRunner, base::Passed(&new_connector),
                 client_user_id_, base::Passed(&client), base::Passed(&request),
                 base::Passed(&context_ref)));
}

void Navigation::Create(const service_manager::Identity& remote_identity,
                        mojom::ViewFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
  refs_.insert(ref_factory_.CreateRef());
}

void Navigation::ViewFactoryLost() {
  refs_.erase(refs_.begin());
}

}  // navigation
