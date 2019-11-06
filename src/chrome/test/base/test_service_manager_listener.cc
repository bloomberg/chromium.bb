// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_service_manager_listener.h"

#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"
#include "services/service_manager/public/cpp/connector.h"

TestServiceManagerListener::TestServiceManagerListener() : binding_(this) {}

TestServiceManagerListener::~TestServiceManagerListener() {}

void TestServiceManagerListener::Init() {
  DCHECK(!binding_.is_bound());
  // Register a listener on the ServiceManager to track when services are
  // started.
  mojo::InterfacePtr<service_manager::mojom::ServiceManager> service_manager;
  content::GetSystemConnector()->BindInterface(
      service_manager::mojom::kServiceName, &service_manager);
  service_manager::mojom::ServiceManagerListenerPtr listener_ptr;
  binding_.Bind(mojo::MakeRequest(&listener_ptr));
  service_manager->AddListener(std::move(listener_ptr));
}

void TestServiceManagerListener::WaitUntilServiceStarted(
    const std::string& service_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!on_service_event_loop_closure_);
  DCHECK(service_name_.empty());
  service_name_ = service_name;
  base::RunLoop run_loop;
  on_service_event_loop_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  on_service_event_loop_closure_.Reset();
}

uint32_t TestServiceManagerListener::GetServiceStartCount(
    const std::string& service_name) const {
  auto iter = service_start_counters_.find(service_name);
  return iter == service_start_counters_.end() ? 0 : iter->second;
}

void TestServiceManagerListener::OnInit(
    std::vector<service_manager::mojom::RunningServiceInfoPtr>
        running_services) {}

void TestServiceManagerListener::OnServiceCreated(
    service_manager::mojom::RunningServiceInfoPtr service) {}

void TestServiceManagerListener::OnServiceStarted(
    const service_manager::Identity& identity,
    uint32_t pid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  service_start_counters_[identity.name()]++;

  if (identity.name() != service_name_)
    return;

  service_name_.clear();
  std::move(on_service_event_loop_closure_).Run();
}

void TestServiceManagerListener::OnServicePIDReceived(
    const service_manager::Identity& identity,
    uint32_t pid) {}

void TestServiceManagerListener::OnServiceFailedToStart(
    const service_manager::Identity& identity) {}

void TestServiceManagerListener::OnServiceStopped(
    const service_manager::Identity& identity) {}
