// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_channel_bindings.h"

#include <lib/fit/function.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/threading/thread_task_runner_handle.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/runners/cast/cast_platform_bindings_ids.h"
#include "fuchsia/runners/cast/named_message_port_connector.h"

// Unique identifier of the Cast Channel message port, used by the JavaScript
// API to connect to the port.
const char kMessagePortName[] = "cast.__platform__.channel";

CastChannelBindings::CastChannelBindings(
    fuchsia::web::Frame* frame,
    NamedMessagePortConnector* connector,
    chromium::cast::CastChannelPtr channel_consumer)
    : frame_(frame),
      connector_(connector),
      channel_consumer_(std::move(channel_consumer)) {
  DCHECK(connector_);
  DCHECK(frame_);

  channel_consumer_.set_error_handler([](zx_status_t status) mutable {
    ZX_LOG(ERROR, status) << "Cast Channel FIDL client disconnected";
  });

  connector->Register(
      kMessagePortName,
      base::BindRepeating(&CastChannelBindings::OnMasterPortReceived,
                          base::Unretained(this)));

  base::FilePath assets_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &assets_path));
  frame_->AddBeforeLoadJavaScript(
      static_cast<uint64_t>(CastPlatformBindingsId::CAST_CHANNEL), {"*"},
      cr_fuchsia::MemBufferFromFile(
          base::File(assets_path.AppendASCII(
                         "fuchsia/runners/cast/cast_channel_bindings.js"),
                     base::File::FLAG_OPEN | base::File::FLAG_READ)),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        CHECK(result.is_response()) << "JavaScript injection error.";
      });
}

CastChannelBindings::~CastChannelBindings() {
  connector_->Unregister(kMessagePortName);
}

void CastChannelBindings::OnMasterPortError() {
  master_port_.Unbind();
}

void CastChannelBindings::OnMasterPortReceived(
    fidl::InterfaceHandle<fuchsia::web::MessagePort> port) {
  DCHECK(port);

  master_port_ = port.Bind();
  master_port_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(WARNING, status != ZX_ERR_PEER_CLOSED, status)
        << "Cast Channel master port disconnected.";
    OnMasterPortError();
  });
  master_port_->ReceiveMessage(fit::bind_member(
      this, &CastChannelBindings::OnCastChannelMessageReceived));
}

void CastChannelBindings::OnCastChannelMessageReceived(
    fuchsia::web::WebMessage message) {
  if (!message.has_incoming_transfer() ||
      !(message.incoming_transfer().size() == 1) ||
      !message.incoming_transfer().at(0).is_message_port()) {
    LOG(WARNING) << "Received a CastChannel without a message port.";
    OnMasterPortError();
    return;
  }

  SendChannelToConsumer(
      message.mutable_incoming_transfer()->at(0).message_port().Bind());

  master_port_->ReceiveMessage(fit::bind_member(
      this, &CastChannelBindings::OnCastChannelMessageReceived));
}

void CastChannelBindings::SendChannelToConsumer(
    fuchsia::web::MessagePortPtr channel) {
  if (consumer_ready_for_port_) {
    consumer_ready_for_port_ = false;
    channel_consumer_->Open(
        std::move(channel),
        fit::bind_member(this, &CastChannelBindings::OnConsumerReadyForPort));
  } else {
    connected_channel_queue_.push_front(std::move(channel));
  }
}

void CastChannelBindings::OnConsumerReadyForPort() {
  DCHECK(!consumer_ready_for_port_);

  consumer_ready_for_port_ = true;
  if (!connected_channel_queue_.empty()) {
    // Deliver the next enqueued channel.
    fuchsia::web::MessagePortPtr next_port =
        std::move(connected_channel_queue_.back());
    SendChannelToConsumer(std::move(next_port));
    connected_channel_queue_.pop_back();
  }
}
