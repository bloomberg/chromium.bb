// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_CAST_CHANNEL_BINDINGS_H_
#define FUCHSIA_RUNNERS_CAST_CAST_CHANNEL_BINDINGS_H_

#include <deque>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/named_message_port_connector.h"

// Handles the injection of cast.__platform__.channel bindings into pages'
// scripting context, and establishes a bidirectional message pipe over
// which the two communicate.
class CastChannelBindings {
 public:
  // Attaches CastChannel bindings and port to a |frame|.
  // |frame|: The frame to be provided with a CastChannel.
  // |connector|: The NamedMessagePortConnector to use for establishing
  // transport.
  // |on_error_closure|: Invoked in the event of an unrecoverable error (e.g.
  //                     lost connection to the Agent). The callback must
  //                     remain valid for the entire lifetime of |this|.
  // |channel_consumer|: A FIDL service which receives opened Cast Channels.
  // Both |frame| and |connector| must outlive |this|.
  CastChannelBindings(chromium::web::Frame* frame,
                      NamedMessagePortConnector* connector,
                      chromium::cast::CastChannelPtr channel_consumer,
                      base::OnceClosure on_error_closure);
  ~CastChannelBindings();

 private:
  // Receives a port used for receiving new Cast Channel ports.
  void OnMasterPortReceived(chromium::web::MessagePortPtr port);

  // Receives a message containing a newly opened Cast Channel from
  // |master_port_|.
  void OnCastChannelMessageReceived(chromium::web::WebMessage message);

  // Indicates that |channel_consumer_| is ready to take another port.
  void OnConsumerReadyForPort();

  // Sends or enqueues a Cast Channel for sending to |channel_consumer_|.
  void SendChannelToConsumer(chromium::web::MessagePortPtr channel);

  // Handles error conditions on |master_port_|.
  void OnMasterPortError();

  chromium::web::Frame* const frame_;
  NamedMessagePortConnector* const connector_;

  // A queue of channels waiting to be sent the Cast Channel FIDL service.
  std::deque<chromium::web::MessagePortPtr> connected_channel_queue_;

  // A long-lived port, used to receive new Cast Channel ports when they are
  // opened. Should be automatically  populated by the
  // NamedMessagePortConnector whenever |frame| loads a new page.
  chromium::web::MessagePortPtr master_port_;

  fuchsia::mem::Buffer bindings_script_;

  base::OnceClosure on_error_closure_;

  // The service which will receive connected Cast Channels.
  chromium::cast::CastChannelPtr channel_consumer_;

  // If set, indicates that |channel_consumer_| is ready to accept another Cast
  // Channel.
  bool consumer_ready_for_port_ = true;

  DISALLOW_COPY_AND_ASSIGN(CastChannelBindings);
};

#endif  // FUCHSIA_RUNNERS_CAST_CAST_CHANNEL_BINDINGS_H_
