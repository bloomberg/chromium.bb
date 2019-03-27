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
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/signaling/ftl_device_id_provider.h"
#include "remoting/signaling/ftl_services.grpc.pb.h"
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

// Wait for the user to press enter key on an anonymous thread and calls
// |on_done| once it is done.
void WaitForEnterKey(base::OnceClosure on_done) {
  base::PostTaskWithTraitsAndReply(FROM_HERE, {base::MayBlock()},
                                   base::BindOnce([]() { getchar(); }),
                                   std::move(on_done));
}

void PrintGrpcStatusError(const grpc::Status& status) {
  DCHECK(!status.ok());
  fprintf(stderr, "RPC failed. Code=%d, Message=%s\n", status.error_code(),
          status.error_message().c_str());
  switch (status.error_code()) {
    case grpc::StatusCode::UNAVAILABLE:
      fprintf(stderr,
              "Set the GRPC_DEFAULT_SSL_ROOTS_FILE_PATH environment variable "
              "to third_party/grpc/src/etc/roots.pem if gRPC cannot locate the "
              "root certificates.\n");
      break;
    case grpc::StatusCode::UNAUTHENTICATED:
      fprintf(
          stderr,
          "Request is unauthenticated. Make sure you have run SignInGaia.\n");
      break;
    default:
      break;
  }
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
  DCHECK(!ftl_context_);

  token_getter_factory_ = std::make_unique<TestOAuthTokenGetterFactory>();

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  std::string username = cmd_line->GetSwitchValueASCII(kSwitchNameUsername);
  base::FilePath storage_path =
      cmd_line->GetSwitchValuePath(kSwitchNameStoragePath);
  storage_ = test::TestTokenStorage::OnDisk(username, storage_path);

  std::string access_token = storage_->FetchAccessToken();
  if (access_token.empty()) {
    AuthenticateAndResetServices();
  } else {
    VLOG(0) << "Reusing access token: " << access_token;
    token_getter_ = std::make_unique<FakeOAuthTokenGetter>(
        OAuthTokenGetter::Status::SUCCESS, "fake_email@gmail.com",
        access_token);
    ResetServices();
  }

  StartLoop();
}

void FtlSignalingPlayground::StartLoop() {
  while (true) {
    printf(
        "\nOptions:\n"
        "  1. SignInGaia\n"
        "  2. GetIceServer\n"
        "  3. PullMessages\n"
        "  4. ReceiveMessages\n"
        "  5. SendMessage\n"
        "  6. Quit\n\n"
        "Your choice [number]: ");
    int choice = 0;
    base::StringToInt(ReadString(), &choice);
    base::RunLoop run_loop;
    switch (choice) {
      case 1:
        SignInGaia(run_loop.QuitClosure());
        break;
      case 2:
        GetIceServer(run_loop.QuitClosure());
        break;
      case 3:
        PullMessages(run_loop.QuitClosure());
        break;
      case 4:
        StartReceivingMessages(run_loop.QuitWhenIdleClosure());
        break;
      case 5:
        SendMessage(run_loop.QuitClosure());
        break;
      case 6:
        return;
      default:
        fprintf(stderr, "Unknown option\n");
        continue;
    }
    run_loop.Run();
  }
}

void FtlSignalingPlayground::ResetServices() {
  ftl_context_ = std::make_unique<FtlGrpcContext>(token_getter_.get());
  peer_to_peer_stub_ = PeerToPeer::NewStub(ftl_context_->channel());

  message_subscription_.reset();
  messaging_client_ = std::make_unique<FtlMessagingClient>(ftl_context_.get());
  message_subscription_ = messaging_client_->RegisterMessageCallback(
      base::BindRepeating(&FtlSignalingPlayground::OnMessageReceived,
                          weak_factory_.GetWeakPtr()));
  registration_manager_ = std::make_unique<FtlRegistrationManager>(
      ftl_context_.get(),
      std::make_unique<FtlDeviceIdProvider>(storage_.get()));
}

void FtlSignalingPlayground::AuthenticateAndResetServices() {
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

  ResetServices();
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

void FtlSignalingPlayground::GetIceServer(base::OnceClosure on_done) {
  DCHECK(peer_to_peer_stub_);
  VLOG(0) << "Running GetIceServer...";
  ftl_context_->ExecuteRpc(
      base::BindOnce(&PeerToPeer::Stub::AsyncGetICEServer,
                     base::Unretained(peer_to_peer_stub_.get())),
      ftl::GetICEServerRequest(),
      base::BindOnce(&FtlSignalingPlayground::OnGetIceServerResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
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
    PrintGrpcStatusError(status);
  }
  std::move(on_done).Run();
}

void FtlSignalingPlayground::SignInGaia(base::OnceClosure on_done) {
  DCHECK(registration_manager_);
  VLOG(0) << "Running SignInGaia...";
  registration_manager_->SignInGaia(
      base::BindOnce(&FtlSignalingPlayground::OnSignInGaiaResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
}

void FtlSignalingPlayground::OnSignInGaiaResponse(base::OnceClosure on_done,
                                                  const grpc::Status& status) {
  if (status.ok()) {
    std::string registration_id_base64;
    base::Base64Encode(registration_manager_->registration_id(),
                       &registration_id_base64);
    printf("Service signed in. registration_id(base64)=%s\n",
           registration_id_base64.c_str());
  } else {
    if (status.error_code() == grpc::StatusCode::UNAUTHENTICATED) {
      VLOG(0) << "Grpc request failed to authenticate. "
              << "Trying to reauthenticate...";
      AuthenticateAndResetServices();
    } else {
      PrintGrpcStatusError(status);
    }
  }
  std::move(on_done).Run();
}

void FtlSignalingPlayground::PullMessages(base::OnceClosure on_done) {
  DCHECK(messaging_client_);
  VLOG(0) << "Running PullMessages...";

  messaging_client_->PullMessages(
      base::BindOnce(&FtlSignalingPlayground::OnPullMessagesResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
}

void FtlSignalingPlayground::OnPullMessagesResponse(
    base::OnceClosure on_done,
    const grpc::Status& status) {
  if (!status.ok()) {
    PrintGrpcStatusError(status);
  }
  std::move(on_done).Run();
}

void FtlSignalingPlayground::SendMessage(base::OnceClosure on_done) {
  DCHECK(messaging_client_);
  VLOG(0) << "Running SendMessage...";

  printf("Receiver ID: ");
  std::string receiver_id = ReadString();

  printf("Receiver registration ID (base64, optional): ");
  std::string registration_id_base64 = ReadString();

  std::string registration_id;
  bool success = base::Base64Decode(registration_id_base64, &registration_id);
  if (!success) {
    fprintf(stderr, "Your input can't be base64 decoded.\n");
    std::move(on_done).Run();
    return;
  }
  DoSendMessage(receiver_id, registration_id, std::move(on_done), true);
}

void FtlSignalingPlayground::DoSendMessage(const std::string& receiver_id,
                                           const std::string& registration_id,
                                           base::OnceClosure on_done,
                                           bool should_keep_running) {
  if (!should_keep_running) {
    std::move(on_done).Run();
    return;
  }

  printf("Message (enter nothing to quit): ");
  std::string message = ReadString();

  if (message.empty()) {
    std::move(on_done).Run();
    return;
  }

  auto on_continue = base::BindOnce(&FtlSignalingPlayground::DoSendMessage,
                                    weak_factory_.GetWeakPtr(), receiver_id,
                                    registration_id, std::move(on_done));

  messaging_client_->SendMessage(
      receiver_id, registration_id, message,
      base::BindOnce(&FtlSignalingPlayground::OnSendMessageResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_continue)));
}

void FtlSignalingPlayground::OnSendMessageResponse(
    base::OnceCallback<void(bool)> on_continue,
    const grpc::Status& status) {
  if (!status.ok()) {
    PrintGrpcStatusError(status);
  } else {
    printf("Message successfully sent.\n");
  }
  std::move(on_continue).Run(status.ok());
}

void FtlSignalingPlayground::StartReceivingMessages(base::OnceClosure on_done) {
  VLOG(0) << "Running StartReceivingMessages...";
  messaging_client_->StartReceivingMessages(
      base::BindOnce(&FtlSignalingPlayground::OnStartReceivingMessagesDone,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
}

void FtlSignalingPlayground::StopReceivingMessages(base::OnceClosure on_done) {
  messaging_client_->StopReceivingMessages();
  std::move(on_done).Run();
}

void FtlSignalingPlayground::OnMessageReceived(const std::string& sender_id,
                                               const std::string& message) {
  printf(
      "Received message:\n"
      "  Sender ID=%s\n"
      "  Message=%s\n",
      sender_id.c_str(), message.c_str());
}

void FtlSignalingPlayground::OnStartReceivingMessagesDone(
    base::OnceClosure on_done,
    const grpc::Status& status) {
  if (status.error_code() == grpc::StatusCode::CANCELLED) {
    printf("ReceiveMessages stream canceled by client.\n");
    std::move(on_done).Run();
    return;
  }
  if (!status.ok()) {
    PrintGrpcStatusError(status);
    std::move(on_done).Run();
    return;
  }
  printf("Started receiving messages. Press enter to stop streaming...\n");
  WaitForEnterKey(base::BindOnce(&FtlSignalingPlayground::StopReceivingMessages,
                                 weak_factory_.GetWeakPtr(),
                                 std::move(on_done)));
}

}  // namespace remoting
