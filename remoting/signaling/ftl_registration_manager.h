// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_REGISTRATION_MANAGER_H_
#define REMOTING_SIGNALING_FTL_REGISTRATION_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "remoting/signaling/ftl_services.grpc.pb.h"

namespace remoting {

class FtlDeviceIdProvider;
class FtlGrpcContext;

// Class for registering the user with FTL service.
// TODO(yuweih): Add unittest
class FtlRegistrationManager final {
 public:
  using DoneCallback = base::OnceCallback<void(const grpc::Status& status)>;

  FtlRegistrationManager(
      FtlGrpcContext* context,
      std::unique_ptr<FtlDeviceIdProvider> device_id_provider);
  ~FtlRegistrationManager();

  // Performs a SignInGaia call for this device. |on_done| is called once it has
  // successfully signed in or failed to sign in.
  void SignInGaia(DoneCallback on_done);

  bool IsSignedIn() const;

  // Returns empty string if user hasn't been signed in.
  const std::string& registration_id() const { return registration_id_; }

 private:
  using Registration =
      google::internal::communications::instantmessaging::v1::Registration;

  void OnSignInGaiaResponse(DoneCallback on_done,
                            const grpc::Status& status,
                            const ftl::SignInGaiaResponse& response);

  FtlGrpcContext* context_;
  std::unique_ptr<FtlDeviceIdProvider> device_id_provider_;
  std::unique_ptr<Registration::Stub> registration_stub_;
  base::OneShotTimer sign_in_refresh_timer_;
  std::string registration_id_;

  base::WeakPtrFactory<FtlRegistrationManager> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(FtlRegistrationManager);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_REGISTRATION_MANAGER_H_
