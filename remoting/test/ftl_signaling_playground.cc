// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/ftl_signaling_playground.h"

#include <inttypes.h>
#include <string>
#include <utility>
#include <vector>

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
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/signaling/ftl_client.h"
#include "remoting/test/test_oauth_token_factory.h"
#include "remoting/test/test_token_storage.h"

namespace {

constexpr char kSwitchNameHelp[] = "help";
constexpr char kSwitchNameAuthCode[] = "code";
constexpr char kSwitchNameUsername[] = "username";
constexpr char kSwitchNameStoragePath[] = "storage-path";

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

}  // namespace

namespace remoting {

FtlSignalingPlayground::FtlSignalingPlayground() : weak_factory_(this) {}

FtlSignalingPlayground::~FtlSignalingPlayground() = default;

bool FtlSignalingPlayground::ShouldPrintHelp() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kSwitchNameHelp);
}

void FtlSignalingPlayground::PrintHelp() {
  printf(
      "Usage: %s [--code=<auth-code>] [--storage-path=<storage-path>] "
      "[--username=<example@gmail.com>]\n",
      base::CommandLine::ForCurrentProcess()
          ->GetProgram()
          .MaybeAsASCII()
          .c_str());
}

void FtlSignalingPlayground::StartAndAuthenticate() {
  DCHECK(!storage_);
  DCHECK(!token_getter_factory_);
  DCHECK(!token_getter_);
  DCHECK(!client_);

  token_getter_factory_ = std::make_unique<TestOAuthTokenGetterFactory>();

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  std::string username = cmd_line->GetSwitchValueASCII(kSwitchNameUsername);
  base::FilePath storage_path =
      cmd_line->GetSwitchValuePath(kSwitchNameStoragePath);
  storage_ = test::TestTokenStorage::OnDisk(username, storage_path);

  std::string access_token = storage_->FetchAccessToken();
  if (access_token.empty()) {
    AuthenticateAndResetClient();
  } else {
    VLOG(0) << "Reusing access token: " << access_token;
    token_getter_ = std::make_unique<FakeOAuthTokenGetter>(
        OAuthTokenGetter::Status::SUCCESS, "fake_email@gmail.com",
        access_token);
    client_ = std::make_unique<FtlClient>(token_getter_.get());
  }

  StartLoop();
}

void FtlSignalingPlayground::StartLoop() {
  while (true) {
    printf(
        "\nOptions:\n"
        "  1. GetIceServer\n"
        "  2. SignInGaia\n"
        "  3. PullMessages\n"
        "  4. Quit\n\n"
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
        PullMessages(run_loop.QuitClosure());
        break;
      case 4:
        return;
      default:
        fprintf(stderr, "Unknown option\n");
        continue;
    }
    run_loop.Run();
  }
}

void FtlSignalingPlayground::AuthenticateAndResetClient() {
  static const std::string read_auth_code_prompt = base::StringPrintf(
      "Please authenticate at:\n\n"
      "  %s\n\n"
      "Enter the auth code: ",
      TestOAuthTokenGetterFactory::GetAuthorizationCodeUri().c_str());
  std::string auth_code = ReadStringFromCommandLineOrStdin(
      kSwitchNameAuthCode, read_auth_code_prompt);

  // Make sure we don't try to reuse an auth code.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(kSwitchNameAuthCode);

  // We can't get back the refresh token since we have first-party scope, so
  // we are not trying to store it.
  token_getter_ = token_getter_factory_->CreateFromIntermediateCredentials(
      auth_code,
      base::DoNothing::Repeatedly<const std::string&, const std::string&>());

  // Get the access token so that we can reuse it for next time.
  base::OnceClosure on_access_token_done = base::DoNothing::Once();
  base::RunLoop run_loop;
  if (!base::RunLoop::IsRunningOnCurrentThread()) {
    on_access_token_done = run_loop.QuitClosure();
  }
  token_getter_->CallWithToken(base::BindOnce(
      &FtlSignalingPlayground::OnAccessToken, weak_factory_.GetWeakPtr(),
      std::move(on_access_token_done)));
  if (!base::RunLoop::IsRunningOnCurrentThread()) {
    run_loop.Run();
  }

  client_ = std::make_unique<FtlClient>(token_getter_.get());
}

void FtlSignalingPlayground::OnAccessToken(base::OnceClosure on_done,
                                           OAuthTokenGetter::Status status,
                                           const std::string& user_email,
                                           const std::string& access_token) {
  DCHECK(status == OAuthTokenGetter::Status::SUCCESS);
  VLOG(0) << "Received access_token: " << access_token;
  storage_->StoreAccessToken(access_token);
  std::move(on_done).Run();
}

void FtlSignalingPlayground::HandleGrpcStatusError(const grpc::Status& status) {
  DCHECK(!status.ok());
  LOG(ERROR) << "RPC failed. Code=" << status.error_code() << ", "
             << "Message=" << status.error_message();
  switch (status.error_code()) {
    case grpc::StatusCode::UNAVAILABLE:
      VLOG(0)
          << "Set the GRPC_DEFAULT_SSL_ROOTS_FILE_PATH environment variable "
          << "to third_party/grpc/src/etc/roots.pem if gRPC cannot locate the "
          << "root certificates.";
      break;
    case grpc::StatusCode::UNAUTHENTICATED:
      VLOG(0) << "Grpc request failed to authenticate. "
              << "Trying to reauthenticate...";
      AuthenticateAndResetClient();
      break;
    default:
      break;
  }
}

void FtlSignalingPlayground::GetIceServer(base::OnceClosure on_done) {
  DCHECK(client_);
  client_->GetIceServer(
      base::BindOnce(&FtlSignalingPlayground::OnGetIceServerResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
  VLOG(0) << "Running GetIceServer...";
}

void FtlSignalingPlayground::OnGetIceServerResponse(
    base::OnceClosure on_done,
    const grpc::Status& status,
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
    HandleGrpcStatusError(status);
  }
  std::move(on_done).Run();
}

void FtlSignalingPlayground::SignInGaia(base::OnceClosure on_done) {
  DCHECK(client_);
  VLOG(0) << "Running SignInGaia...";
  // TODO(yuweih): Logic should be cleaned up and moved out of the playground.
  std::string device_id = storage_->FetchDeviceId();
  if (device_id.empty()) {
    device_id = "crd-web-" + base::GenerateGUID();
    VLOG(0) << "Generated new device_id: " << device_id;
    storage_->StoreDeviceId(device_id);
  } else {
    VLOG(0) << "Read device_id: " << device_id;
  }
  VLOG(0) << "Using sign_in_gaia_mode: DEFAULT_CREATE_ACCOUNT";
  client_->SignInGaia(
      device_id, ftl::SignInGaiaMode_Value_DEFAULT_CREATE_ACCOUNT,
      base::BindOnce(&FtlSignalingPlayground::OnSignInGaiaResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
}

void FtlSignalingPlayground::OnSignInGaiaResponse(
    base::OnceClosure on_done,
    const grpc::Status& status,
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
    client_->SetAuthToken(response.auth_token().payload());
    VLOG(0) << "Auth token set on FtlClient";
  } else {
    HandleGrpcStatusError(status);
  }
  std::move(on_done).Run();
}

void FtlSignalingPlayground::PullMessages(base::OnceClosure on_done) {
  DCHECK(client_);
  VLOG(0) << "Running PullMessages...";
  client_->PullMessages(
      base::BindOnce(&FtlSignalingPlayground::OnPullMessagesResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
}

void FtlSignalingPlayground::OnPullMessagesResponse(
    base::OnceClosure on_done,
    const grpc::Status& status,
    const ftl::PullMessagesResponse& response) {
  if (!status.ok()) {
    if (status.error_code() == grpc::StatusCode::UNAUTHENTICATED) {
      fprintf(stderr, "Please run SignInGaia first\n");
    } else {
      HandleGrpcStatusError(status);
    }
    std::move(on_done).Run();
    return;
  }

  std::vector<ftl::ReceiverMessage> receiver_messages;
  printf("pull_all=%d\n", response.pulled_all());
  for (const auto& message : response.messages()) {
    printf(
        "Message:\n"
        "  message_type=%d\n"
        "  message_id=%s\n"
        "  sender_id.id=%s\n"
        "  receiver_id.id=%s\n",
        message.message_type(), message.message_id().c_str(),
        message.sender_id().id().c_str(), message.receiver_id().id().c_str());

    if (message.message_type() ==
        ftl::InboxMessage_MessageType_CHROMOTING_MESSAGE) {
      ftl::ChromotingMessage chromoting_message;
      chromoting_message.ParseFromString(message.message());
      printf("  message(ChromotingMessage deserialized)=%s\n",
             chromoting_message.message().c_str());
    } else {
      std::string message_base64;
      base::Base64Encode(message.message(), &message_base64);
      printf("  message(base64)=%s\n", message_base64.c_str());
    }

    ftl::ReceiverMessage receiver_message;
    receiver_message.set_message_id(message.message_id());
    receiver_message.set_allocated_receiver_id(
        new ftl::Id(message.receiver_id()));
    receiver_messages.push_back(std::move(receiver_message));
  }

  if (receiver_messages.empty()) {
    VLOG(0) << "No message has been received";
    std::move(on_done).Run();
    return;
  }

  // TODO(yuweih): Might need retry logic.
  VLOG(0) << "Acking " << receiver_messages.size() << " messages";
  client_->AckMessages(
      receiver_messages,
      base::BindOnce(&FtlSignalingPlayground::OnAckMessagesResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
}

void FtlSignalingPlayground::OnAckMessagesResponse(
    base::OnceClosure on_done,
    const grpc::Status& status,
    const ftl::AckMessagesResponse& response) {
  if (status.ok()) {
    VLOG(0) << "Messages acked";
  } else {
    HandleGrpcStatusError(status);
  }
  std::move(on_done).Run();
}

}  // namespace remoting
