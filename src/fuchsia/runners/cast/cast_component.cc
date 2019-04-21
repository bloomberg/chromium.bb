// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_component.h"

#include <lib/fidl/cpp/binding.h>
#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/files/file_util.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/path_service.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/cast_platform_bindings_ids.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "fuchsia/runners/common/web_component.h"

namespace {

constexpr int kBindingsFailureExitCode = 129;

constexpr char kStubBindingsPath[] =
    FILE_PATH_LITERAL("fuchsia/runners/cast/not_implemented_api_bindings.js");

}  // namespace

CastComponent::CastComponent(
    CastRunner* runner,
    std::unique_ptr<base::fuchsia::StartupContext> context,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request,
    std::unique_ptr<cr_fuchsia::AgentManager> agent_manager)
    : WebComponent(runner, std::move(context), std::move(controller_request)),
      agent_manager_(std::move(agent_manager)),
      navigation_listener_binding_(this) {
  base::AutoReset<bool> constructor_active_reset(&constructor_active_, true);

  InitializeCastPlatformBindings();

  frame()->SetNavigationEventListener(
      navigation_listener_binding_.NewBinding());
}

CastComponent::~CastComponent() = default;

void CastComponent::DestroyComponent(int termination_exit_code,
                                     fuchsia::sys::TerminationReason reason) {
  DCHECK(!constructor_active_);

  WebComponent::DestroyComponent(termination_exit_code, reason);
}

void CastComponent::OnNavigationStateChanged(
    fuchsia::web::NavigationState change,
    OnNavigationStateChangedCallback callback) {
  if (change.has_url())
    connector_.NotifyPageLoad(frame());
  callback();
}

void CastComponent::InitializeCastPlatformBindings() {
  base::FilePath stub_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &stub_path));
  stub_path = stub_path.AppendASCII(kStubBindingsPath);
  DCHECK(base::PathExists(stub_path));
  fuchsia::mem::Buffer stub_buf = cr_fuchsia::MemBufferFromFile(
      base::File(stub_path, base::File::FLAG_OPEN | base::File::FLAG_READ));
  CHECK(stub_buf.vmo);
  frame()->AddBeforeLoadJavaScript(
      static_cast<uint64_t>(CastPlatformBindingsId::NOT_IMPLEMENTED_API), {"*"},
      std::move(stub_buf),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        CHECK(result.is_response()) << "Couldn't inject stub bindings.";
      });

  cast_channel_ = std::make_unique<CastChannelBindings>(
      frame(), &connector_,
      agent_manager_->ConnectToAgentService<chromium::cast::CastChannel>(
          CastRunner::kAgentComponentUrl),
      base::BindOnce(&CastComponent::DestroyComponent, base::Unretained(this),
                     kBindingsFailureExitCode,
                     fuchsia::sys::TerminationReason::INTERNAL_ERROR));

  queryable_data_ = std::make_unique<QueryableDataBindings>(
      frame(),
      agent_manager_->ConnectToAgentService<chromium::cast::QueryableData>(
          CastRunner::kAgentComponentUrl));
}
