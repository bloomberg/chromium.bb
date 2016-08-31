// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VALIDATING_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_VALIDATING_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/authenticator.h"

namespace remoting {
namespace protocol {

// This authenticator class provides a way to check the validity of a connection
// as it is being established through an asynchronous callback.  The validation
// logic supplied by the caller is run when the first message is received from
// the client.  If the connection details are valid (e.g. conform to the current
// policies), then the initial message, and all subsequent messages, are passed
// to the underlying authenticator instance for processing.
class ValidatingAuthenticator : public Authenticator {
 public:
  enum class Result {
    SUCCESS,
    ERROR_INVALID_CREDENTIALS,
    ERROR_INVALID_ACCOUNT,
    ERROR_REJECTED_BY_USER
  };

  typedef base::Callback<void(Result validation_result)> ResultCallback;

  typedef base::Callback<void(const std::string& remote_jid,
                              const ResultCallback& callback)>
      ValidationCallback;

  ValidatingAuthenticator(const std::string& remote_jid,
                          const ValidationCallback& validation_callback,
                          std::unique_ptr<Authenticator> current_authenticator);
  ~ValidatingAuthenticator() override;

  // Authenticator interface.
  State state() const override;
  bool started() const override;
  RejectionReason rejection_reason() const override;
  const std::string& GetAuthKey() const override;
  std::unique_ptr<ChannelAuthenticator> CreateChannelAuthenticator()
      const override;
  void ProcessMessage(const buzz::XmlElement* message,
                      const base::Closure& resume_callback) override;
  std::unique_ptr<buzz::XmlElement> GetNextMessage() override;

 private:
  // Checks |validation_result|.  On success, |message| and |resume_callback|
  // are passed on to |current_authenticator_|.  If the connection was rejected,
  // |state_| and |rejection_reason_| are updated and |resume_callback| is run.
  void OnValidateComplete(const buzz::XmlElement* message,
                          const base::Closure& resume_callback,
                          Result validation_result);

  // Updates |state_| to reflect the current underlying authenticator state.
  // |resume_callback| is called after the state is updated.
  void UpdateState(const base::Closure& resume_callback);

  bool first_message_received_ = false;

  // The JID of the remote user.
  std::string remote_jid_;

  ValidationCallback validation_callback_;

  // Returns the current state of the authenticator.
  State state_ = Authenticator::WAITING_MESSAGE;

  // Returns the rejection reason. Can be called only when in REJECTED state.
  RejectionReason rejection_reason_ = Authenticator::INVALID_CREDENTIALS;

  std::unique_ptr<Authenticator> current_authenticator_;

  base::WeakPtrFactory<ValidatingAuthenticator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ValidatingAuthenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VALIDATING_AUTHENTICATOR_H_
