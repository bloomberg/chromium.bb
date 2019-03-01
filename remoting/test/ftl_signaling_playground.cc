// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/ftl_signaling_playground.h"

#include <inttypes.h>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/signaling/ftl_client.h"
#include "remoting/test/test_oauth_token_factory.h"

namespace {

constexpr char kSwitchNameHelp[] = "help";
constexpr char kSwitchNameAuthCode[] = "code";

// Reads a newline-terminated string from stdin.
std::string ReadString() {
  const int kMaxLen = 1024;
  std::string str(kMaxLen, 0);
  char* result = fgets(&str[0], kMaxLen, stdin);
  if (!result)
    return std::string();
  size_t newline_index = str.find('\n');
  if (newline_index != std::string::npos)
    str[newline_index] = '\0';
  str.resize(strlen(&str[0]));
  return str;
}

// Read the value of |switch_name| from command line if it exists, otherwise
// read from stdin.
std::string ReadStringFromCommandLineOrStdin(const std::string& switch_name,
                                             const std::string& read_prompt) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switch_name)) {
    return command_line->GetSwitchValueASCII(switch_name);
  }
  printf("%s", read_prompt.c_str());
  return ReadString();
}

void PrintGrpcStatusError(const grpc::Status& status) {
  DCHECK(!status.ok());
  LOG(ERROR) << "RPC failed. Code=" << status.error_code() << ", "
             << "Message=" << status.error_message();
  if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
    VLOG(0)
        << "Set the GRPC_DEFAULT_SSL_ROOTS_FILE_PATH environment variable "
        << "to third_party/grpc/src/etc/roots.pem if gRPC cannot locate the "
        << "root certificates.";
  }
}

}  // namespace

namespace remoting {

FtlSignalingPlayground::FtlSignalingPlayground() = default;

FtlSignalingPlayground::~FtlSignalingPlayground() = default;

bool FtlSignalingPlayground::ShouldPrintHelp() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kSwitchNameHelp);
}

void FtlSignalingPlayground::PrintHelp() {
  printf("Usage: %s [--code=<auth-code>]\n",
         base::CommandLine::ForCurrentProcess()
             ->GetProgram()
             .MaybeAsASCII()
             .c_str());
}

void FtlSignalingPlayground::StartAndAuthenticate() {
  DCHECK(!token_getter_factory_);
  DCHECK(!token_getter_);
  DCHECK(!client_);

  static const std::string read_auth_code_prompt = base::StringPrintf(
      "Please authenticate at:\n\n"
      "  %s\n\n"
      "Enter the auth code: ",
      TestOAuthTokenGetterFactory::GetAuthorizationCodeUri().c_str());
  std::string auth_code = ReadStringFromCommandLineOrStdin(
      kSwitchNameAuthCode, read_auth_code_prompt);

  token_getter_factory_ = std::make_unique<TestOAuthTokenGetterFactory>();

  // We can't get back the refresh token since we have first-party scope, so
  // we are not trying to store it.
  // TODO(yuweih): Consider storing the access token and reuse it until it is
  // expired.
  token_getter_ = token_getter_factory_->CreateFromIntermediateCredentials(
      auth_code,
      base::DoNothing::Repeatedly<const std::string&, const std::string&>());
  client_ = std::make_unique<FtlClient>(token_getter_.get());

  StartLoop();
}

void FtlSignalingPlayground::StartLoop() {
  while (true) {
    printf(
        "\nOptions:\n"
        "  1. GetIceServer\n"
        "  2. SignInGaia\n"
        "  3. Quit\n\n"
        "Your choice [number]: ");
    int choice = 0;
    base::StringToInt(ReadString(), &choice);
    base::RunLoop run_loop;
    switch (choice) {
      case 1:
        GetIceServer(run_loop.QuitClosure());
        break;
      case 2:
        SignInGaia(run_loop.QuitClosure());
        break;
      case 3:
        return;
      default:
        fprintf(stderr, "Unknown option\n");
        continue;
    }
    run_loop.Run();
  }
}

void FtlSignalingPlayground::GetIceServer(base::OnceClosure on_done) {
  DCHECK(client_);
  client_->GetIceServer(base::BindOnce(
      &FtlSignalingPlayground::OnGetIceServerResponse, std::move(on_done)));
  VLOG(0) << "Running GetIceServer...";
}

// static
void FtlSignalingPlayground::OnGetIceServerResponse(
    base::OnceClosure on_done,
    grpc::Status status,
    const ftl::GetICEServerResponse& response) {
  if (status.ok()) {
    printf("Ice transport policy: %s\n",
           response.ice_config().ice_transport_policy().c_str());
    for (const ftl::ICEServerList& server :
         response.ice_config().ice_servers()) {
      printf(
          "ICE server:\n"
          "  hostname=%s\n"
          "  username=%s\n"
          "  credential=%s\n"
          "  max_rate_kbps=%" PRId64 "\n",
          server.hostname().c_str(), server.username().c_str(),
          server.credential().c_str(), server.max_rate_kbps());
      for (const std::string& url : server.urls()) {
        printf("  url=%s\n", url.c_str());
      }
    }
  } else {
    PrintGrpcStatusError(status);
  }
  std::move(on_done).Run();
}

void FtlSignalingPlayground::SignInGaia(base::OnceClosure on_done) {
  DCHECK(client_);
  VLOG(0) << "Running SignInGaia...";
  // TODO(yuweih): Store generated GUID and reuse them when possible.
  std::string device_id = "crd-web-" + base::GenerateGUID();
  VLOG(0) << "Using device_id: " << device_id;
  VLOG(0) << "Using sign_in_gaia_mode: DEFAULT_CREATE_ACCOUNT";
  client_->SignInGaia(
      device_id, ftl::SignInGaiaMode_Value_DEFAULT_CREATE_ACCOUNT,
      base::BindOnce(&FtlSignalingPlayground::OnSignInGaiaResponse,
                     std::move(on_done)));
}

// static
void FtlSignalingPlayground::OnSignInGaiaResponse(
    base::OnceClosure on_done,
    grpc::Status status,
    const ftl::SignInGaiaResponse& response) {
  if (status.ok()) {
    // TODO(yuweih): Allow loading auth token directly from command line.
    std::string registration_id_base64;
    std::string auth_token_base64;
    base::Base64Encode(response.registration_id(), &registration_id_base64);
    base::Base64Encode(response.auth_token().payload(), &auth_token_base64);
    printf(
        "registration_id(base64)=%s\n"
        "auth_token.payload(base64)=%s\n"
        "auth_token.expires_in=%" PRId64 "\n",
        registration_id_base64.c_str(), auth_token_base64.c_str(),
        response.auth_token().expires_in());
  } else {
    PrintGrpcStatusError(status);
  }
  std::move(on_done).Run();
}

}  // namespace remoting
