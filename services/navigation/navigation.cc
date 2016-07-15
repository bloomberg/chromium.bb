// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/navigation/navigation.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/navigation/view_impl.h"
#include "services/shell/public/cpp/connector.h"

namespace navigation {

namespace {

void CreateViewOnViewTaskRunner(
    std::unique_ptr<shell::Connector> connector,
    const std::string& client_user_id,
    mojom::ViewClientPtr client,
    mojom::ViewRequest request,
    std::unique_ptr<shell::ServiceContextRef> context_ref) {
  // Owns itself.
  new ViewImpl(std::move(connector), client_user_id, std::move(client),
               std::move(request), std::move(context_ref));
}

}  // namespace

Navigation::Navigation()
    : view_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      ref_factory_(base::MessageLoop::QuitWhenIdleClosure()) {
  bindings_.set_connection_error_handler(
      base::Bind(&Navigation::ViewFactoryLost, base::Unretained(this)));
}
Navigation::~Navigation() {}

bool Navigation::OnConnect(shell::Connection* connection,
                           shell::Connector* connector) {
  std::string remote_user_id = connection->GetRemoteIdentity().user_id();
  if (!client_user_id_.empty() && client_user_id_ != remote_user_id) {
    LOG(ERROR) << "Must have a separate Navigation service instance for "
               << "different BrowserContexts.";
    return false;
  }
  client_user_id_ = remote_user_id;

  connection->AddInterface<mojom::ViewFactory>(this);
  return true;
}

void Navigation::Create(shell::Connection* connection,
                        mojom::ViewFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
  refs_.insert(ref_factory_.CreateRef());
}

void Navigation::CreateView(mojom::ViewClientPtr client,
                            mojom::ViewRequest request) {
  std::unique_ptr<shell::Connector> new_connector = connector_->Clone();
  std::unique_ptr<shell::ServiceContextRef> context_ref =
      ref_factory_.CreateRef();
  view_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CreateViewOnViewTaskRunner, base::Passed(&new_connector),
                 client_user_id_, base::Passed(&client), base::Passed(&request),
                 base::Passed(&context_ref)));
}

void Navigation::ViewFactoryLost() {
  refs_.erase(refs_.begin());
}

}  // navigation
