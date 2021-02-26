// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_SENDER_LOOPING_FILE_CAST_AGENT_H_
#define CAST_STANDALONE_SENDER_LOOPING_FILE_CAST_AGENT_H_

#include <openssl/x509.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "cast/common/channel/cast_socket_message_port.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "cast/common/channel/virtual_connection_router.h"
#include "cast/common/public/cast_socket.h"
#include "cast/sender/public/sender_socket_factory.h"
#include "cast/standalone_sender/looping_file_sender.h"
#include "cast/streaming/environment.h"
#include "cast/streaming/sender_session.h"
#include "platform/api/scoped_wake_lock.h"
#include "platform/api/serial_delete_ptr.h"
#include "platform/base/error.h"
#include "platform/base/interface_info.h"
#include "platform/impl/task_runner.h"

namespace openscreen {
namespace cast {

// This class manages sender connections, starting with listening over TLS for
// connection attempts, constructing SenderSessions when OFFER messages are
// received, and linking Senders to the output decoder and SDL visualizer.
class LoopingFileCastAgent final
    : public SenderSocketFactory::Client,
      public VirtualConnectionRouter::SocketErrorHandler,
      public SenderSession::Client {
 public:
  explicit LoopingFileCastAgent(TaskRunner* task_runner);
  ~LoopingFileCastAgent();

  struct ConnectionSettings {
    // The endpoint of the receiver we wish to connect to. Eventually this
    // will come from discovery, instead of an endpoint here.
    IPEndpoint receiver_endpoint;

    // The path to the file that we want to play.
    std::string path_to_file;

    // The maximum bitrate. Default value means a reasonable default will be
    // selected.
    int max_bitrate = 0;

    // Whether the stream should include video, or just be audio only.
    bool should_include_video = true;

    // Whether we should use the hacky RTP stream IDs for legacy android
    // receivers, or if we should use the proper values.
    bool use_android_rtp_hack = true;
  };

  void Connect(ConnectionSettings settings);
  void Stop();

  // SenderSocketFactory::Client overrides.
  void OnConnected(SenderSocketFactory* factory,
                   const IPEndpoint& endpoint,
                   std::unique_ptr<CastSocket> socket) override;
  void OnError(SenderSocketFactory* factory,
               const IPEndpoint& endpoint,
               Error error) override;

  // VirtualConnectionRouter::SocketErrorHandler overrides.
  void OnClose(CastSocket* cast_socket) override;
  void OnError(CastSocket* socket, Error error) override;

  // SenderSession::Client overrides.
  void OnNegotiated(const SenderSession* session,
                    SenderSession::ConfiguredSenders senders,
                    capture_recommendations::Recommendations
                        capture_recommendations) override;
  void OnError(const SenderSession* session, Error error) override;

 private:
  // Once we have a connection to the receiver we need to create and start
  // a sender session. This method results in the OFFER/ANSWER exchange
  // being completed and a session should be started.
  void CreateAndStartSession();

  // Helper for stopping the current session. This is useful for when we don't
  // want to completely stop (e.g. an issue with a specific Sender) but need
  // to terminate the current connection.
  void StopCurrentSession();

  // Member variables set as part of construction.
  VirtualConnectionManager connection_manager_;
  TaskRunner* const task_runner_;
  SerialDeletePtr<VirtualConnectionRouter> router_;
  SerialDeletePtr<CastSocketMessagePort> message_port_;
  SerialDeletePtr<SenderSocketFactory> socket_factory_;
  SerialDeletePtr<TlsConnectionFactory> connection_factory_;

  // Member variables set as part of starting up.
  std::unique_ptr<Environment> environment_;
  absl::optional<ConnectionSettings> connection_settings_;
  SerialDeletePtr<ScopedWakeLock> wake_lock_;

  // Member variables set as part of a sender connection.
  // NOTE: currently we only support a single sender connection and a
  // single streaming session.
  std::unique_ptr<SenderSession> current_session_;
  std::unique_ptr<LoopingFileSender> file_sender_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STANDALONE_SENDER_LOOPING_FILE_CAST_AGENT_H_
