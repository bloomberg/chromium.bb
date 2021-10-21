// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_STREAMING_RECEIVER_SESSION_CLIENT_H_
#define CHROMECAST_CAST_CORE_STREAMING_RECEIVER_SESSION_CLIENT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_web_contents.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast_streaming/browser/public/network_context_getter.h"
#include "components/cast_streaming/browser/public/receiver_session.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class NavigationHandle;
}  // namespace content

namespace chromecast {

// This class wraps all //components/cast_streaming functionality, only
// expecting the caller to supply a MessagePortFactory. Internally, it
// manages the lifetimes of cast streaming objects, and informs the caller
// of important events. Methods in this class may not be called in parallel.
class StreamingReceiverSessionClient
    : public CastWebContents::Observer,
      public cast_api_bindings::MessagePort::Receiver {
 public:
  class Handler {
   public:
    virtual ~Handler();

    //.Called when the streaming session as successfully been initialized,
    // following navigation of the observed CastWebContents to a cast-supporting
    // URL.
    virtual void OnStreamingSessionStarted() = 0;

    // Called when a nonrecoverable error occurs. Following this call, the
    // associated StreamingReceiverSessionClient instance will be placed in an
    // undefined state.
    virtual void OnError() = 0;

    // Called when an AV settings query must be started for |message_port|.
    virtual void StartAvSettingsQuery(
        std::unique_ptr<cast_api_bindings::MessagePort> message_port) = 0;
  };

  // Max time for which streaming may wait for AV Settings receipt before being
  // treated as a failure.
  static constexpr base::TimeDelta kMaxAVSettingsWaitTime = base::Seconds(5);

  // Creates a new instance of this class. |handler| must persist for the
  // lifetime of this instance.
  StreamingReceiverSessionClient(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      cast_streaming::NetworkContextGetter network_context_getter,
      std::unique_ptr<cast_api_bindings::MessagePort> message_port,
      Handler* handler);

  ~StreamingReceiverSessionClient() override;

  // Schedules starting the Streaming Receiver owned by this instance. May only
  // be called once. At time of calling, this instance will be set as the
  // observer of |cast_web_contents|, for which streaming will be started
  // following the latter of:
  // - Navigation to an associated URL by |cast_web_contents|.
  // - Receipt of supported AV Settings.
  // Following this call, the supported AV Settings are expected to remain
  // constant. If valid AV Settings have not been received within
  // |kMaxAVSettingsWaitTime| of this function call, it will be treated as an
  // unrecoverable error, and this instance will be placed in an undefined
  // state.
  void LaunchStreamingReceiverAsync(CastWebContents* cast_web_contents);

  bool has_streaming_launched() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return streaming_state_ == LaunchState::kLaunched;
  }

  bool is_streaming_launch_pending() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return streaming_state_ & LaunchState::kLaunchCalled;
  }

  bool has_received_av_settings() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return streaming_state_ & LaunchState::kAVSettingsReceived;
  }

  bool is_healthy() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !(streaming_state_ & LaunchState::kError);
  }

 private:
  friend class StreamingReceiverSessionClientTest;

  using ReceiverSessionFactory =
      base::OnceCallback<std::unique_ptr<cast_streaming::ReceiverSession>(
          cast_streaming::ReceiverSession::AVConstraints)>;

  enum LaunchState : int32_t {
    kStopped = 0x00,

    // The three conditions which must be met for streaming to run.
    kLaunchCalled = 0x01 << 0,
    kAVSettingsReceived = 0x01 << 1,
    kMojoHandleAcquired = 0x01 << 2,

    // Signifies that the above conditions have all been met.
    kReady = kAVSettingsReceived | kLaunchCalled | kMojoHandleAcquired,

    // Signifies that streaming has started.
    kLaunched = 0xFF,

    // Error state set after a Handler::OnError() call.
    kError = 0x100
  };

  // This second ctor is required for Unit Testing.
  StreamingReceiverSessionClient(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      cast_streaming::NetworkContextGetter network_context_getter,
      ReceiverSessionFactory factory,
      Handler* handler);

  friend inline LaunchState operator&(LaunchState first, LaunchState second) {
    return static_cast<LaunchState>(static_cast<int32_t>(first) &
                                    static_cast<int32_t>(second));
  }

  friend inline LaunchState operator|(LaunchState first, LaunchState second) {
    return static_cast<LaunchState>(static_cast<int32_t>(first) |
                                    static_cast<int32_t>(second));
  }

  friend inline LaunchState& operator|=(LaunchState& first,
                                        LaunchState second) {
    return first = first | second;
  }

  friend inline LaunchState& operator&=(LaunchState& first,
                                        LaunchState second) {
    return first = first & second;
  }

  bool TryStartStreamingSession();
  void VerifyAVSettingsReceived();
  void TriggerError();

  // CastWebContents::Observer overrides.
  void MainFrameReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

  // cast_api_bindings::MessagePort::Receiver overrides.
  bool OnMessage(base::StringPiece message,
                 std::vector<std::unique_ptr<cast_api_bindings::MessagePort>>
                     ports) override;
  void OnPipeError() override;

  // Handler for callbacks associated with this class. May be empty.
  Handler* const handler_;

  // Task runner on which waiting for the result of an AV Settings query should
  // occur.
  scoped_refptr<base::SequencedTaskRunner> const task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Most recently received AV Constraints, from bindings.
  absl::optional<cast_streaming::ReceiverSession::AVConstraints>
      av_constraints_;

  // The AssociatedRemote that must be provided when starting the
  // |receiver_session_|.
  mojo::AssociatedRemote<::mojom::CastStreamingReceiver>
      cast_streaming_receiver_;

  // Responsible for managing the streaming session.
  std::unique_ptr<cast_streaming::ReceiverSession> receiver_session_;

  // MessagePort responsible for receiving AV Settings Bindings Messages.
  std::unique_ptr<cast_api_bindings::MessagePort> message_port_;

  // Factory method used to create a receiver session.
  ReceiverSessionFactory receiver_session_factory_;

  // Current state in initialization of |receiver_session_|.
  LaunchState streaming_state_ = LaunchState::kStopped;

  base::WeakPtrFactory<StreamingReceiverSessionClient> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_STREAMING_RECEIVER_SESSION_CLIENT_H_
