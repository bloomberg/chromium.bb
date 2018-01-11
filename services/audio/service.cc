// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/service.h"

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/audio/audio_manager.h"
#include "services/audio/system_info.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace audio {

Service::Service(std::unique_ptr<AudioManagerAccessor> audio_manager_accessor)
    : audio_manager_accessor_(std::move(audio_manager_accessor)) {
  DCHECK(audio_manager_accessor_);
}

Service::~Service() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void Service::OnStart() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  registry_.AddInterface<mojom::SystemInfo>(base::BindRepeating(
      &Service::BindSystemInfoRequest, base::Unretained(this)));
}

void Service::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

bool Service::OnServiceManagerConnectionLost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_manager_accessor_->Shutdown();
  return true;
}

void Service::BindSystemInfoRequest(mojom::SystemInfoRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!system_info_) {
    system_info_ = std::make_unique<SystemInfo>(
        audio_manager_accessor_->GetAudioManager());
  }
  system_info_->Bind(std::move(request));
}

}  // namespace audio
