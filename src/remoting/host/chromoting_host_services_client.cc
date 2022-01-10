// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_services_client.h"

#include "base/bind.h"
#include "base/environment.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"

#if defined(OS_WIN)
#include <windows.h>

#include "remoting/host/win/acl_util.h"
#endif

namespace remoting {

namespace {

#if defined(OS_LINUX)
constexpr char kChromeRemoteDesktopSessionEnvVar[] =
    "CHROME_REMOTE_DESKTOP_SESSION";
#endif

bool g_initialized = false;

}  // namespace

ChromotingHostServicesClient::ChromotingHostServicesClient()
    : environment_(base::Environment::Create()),
      server_name_(GetChromotingHostServicesServerName()) {
  DCHECK(g_initialized)
      << "ChromotingHostServicesClient::Initialize() has not been called.";
}

ChromotingHostServicesClient::~ChromotingHostServicesClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
bool ChromotingHostServicesClient::Initialize() {
  DCHECK(!g_initialized);
#if defined(OS_WIN)
  // The ChromotingHostServices server runs under the LocalService account,
  // which normally isn't allowed to query process info like session ID of a
  // process running under a different account, so we add an ACL to allow it.
  g_initialized = AddProcessAccessRightForWellKnownSid(
      WinLocalServiceSid, PROCESS_QUERY_LIMITED_INFORMATION);
#else
  // Other platforms don't need initialization.
  g_initialized = true;
#endif
  return g_initialized;
}

mojom::ChromotingSessionServices*
ChromotingHostServicesClient::GetSessionServices() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!const_cast<ChromotingHostServicesClient*>(this)
           ->EnsureSessionServicesBinding()) {
    return nullptr;
  }
  return session_services_remote_.get();
}

bool ChromotingHostServicesClient::EnsureConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (remote_.is_bound()) {
    return true;
  }

  auto endpoint = mojo::NamedPlatformChannel::ConnectToServer(server_name_);
  if (!endpoint.is_valid()) {
    LOG(WARNING) << "Cannot connect to IPC through server name " << server_name_
                 << ". Endpoint is invalid.";
    return false;
  }
  connection_ = std::make_unique<mojo::IsolatedConnection>();
  mojo::PendingRemote<mojom::ChromotingHostServices> pending_remote(
      connection_->Connect(std::move(endpoint)), /* version= */ 0);
  if (!pending_remote.is_valid()) {
    LOG(WARNING) << "Invalid message pipe.";
    connection_.reset();
    return false;
  }
  remote_.Bind(std::move(pending_remote));
  remote_.set_disconnect_handler(base::BindOnce(
      &ChromotingHostServicesClient::OnDisconnected, base::Unretained(this)));
  return true;
}

bool ChromotingHostServicesClient::EnsureSessionServicesBinding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_services_remote_.is_bound()) {
    return true;
  }
#if defined(OS_LINUX)
  if (!environment_->HasVar(kChromeRemoteDesktopSessionEnvVar)) {
    LOG(WARNING) << "Current desktop environment is not remotable.";
    return false;
  }
#endif
  if (!EnsureConnection()) {
    return false;
  }
  remote_->BindSessionServices(
      session_services_remote_.BindNewPipeAndPassReceiver());
  session_services_remote_.reset_on_disconnect();
  return true;
}

void ChromotingHostServicesClient::OnDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  remote_.reset();
  connection_.reset();
}

}  // namespace remoting
