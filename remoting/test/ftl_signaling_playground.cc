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
#include "remoting/signaling/ftl_grpc_context.h"
#include "remoting/signaling/ftl_services.grpc.pb.h"
#include "remoting/signaling/grpc_support/grpc_async_unary_request.h"
#include "remoting/test/test_oauth_token_factory.h"
#include "remoting/test/test_token_storage.h"

namespace {

constexpr char kSwitchNameHelp[] = "help";
constexpr char kSwitchNameAuthCode[] = "code";
constexpr char kSwitchNameUsername[] = "username";
constexpr char kSwitchNameStoragePath[] = "storage-path";
constexpr char kSwitchNameNoAutoSignin[] = "no-auto-signin";

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

bool NeedsManualSignin() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kSwitchNameNoAutoSignin);
}

struct CommandOption {
 public:
  std::string name;
  base::RepeatingClosure callback;
};

}  // namespace

namespace remoting {

FtlSignalingPlayground::FtlSignalingPlayground() : weak_factory_(this) {}

FtlSignalingPlayground::~FtlSignalingPlayground() = default;

bool FtlSignalingPlayground::ShouldPrintHelp() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kSwitchNameHelp);
}

void FtlSignalingPlayground::PrintHelp() {
  printf(
      "Usage: %s [--no-auto-signin] [--code=<auth-code>] "
      "[--storage-path=<storage-path>] [--username=<example@gmail.com>]\n",
      base::CommandLine::ForCurrentProcess()
          ->GetProgram()
          .MaybeAsASCII()
          .c_str());
}

void FtlSignalingPlayground::StartAndAuthenticate() {
  DCHECK(!storage_);
  DCHECK(!token_getter_factory_);
  DCHECK(!token_getter_);
  DCHECK(!executor_);

  token_getter_factory_ = std::make_unique<TestOAuthTokenGetterFactory>();

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  std::string username = cmd_line->GetSwitchValueASCII(kSwitchNameUsername);
  base::FilePath storage_path =
      cmd_line->GetSwitchValuePath(kSwitchNameStoragePath);
  storage_ = test::TestTokenStorage::OnDisk(username, storage_path);

  std::string access_token = storage_->FetchAccessToken();
  base::RunLoop run_loop;
  if (access_token.empty()) {
    AuthenticateAndResetServices(run_loop.QuitClosure());
  } else {
    VLOG(0) << "Reusing access token: " << access_token;
    token_getter_ = std::make_unique<FakeOAuthTokenGetter>(
        OAuthTokenGetter::Status::SUCCESS, "fake_email@gmail.com",
        access_token);
    ResetServices(run_loop.QuitClosure());
  }
  run_loop.Run();

  StartLoop();
}

void FtlSignalingPlayground::StartLoop() {
  while (true) {
    base::RunLoop run_loop;
    std::vector<CommandOption> options{
        {"GetIceServer",
         base::BindRepeating(&FtlSignalingPlayground::GetIceServer,
                             weak_factory_.GetWeakPtr(),
                             run_loop.QuitClosure())},
        {"PullMessages",
         base::BindRepeating(&FtlSignalingPlayground::PullMessages,
                             weak_factory_.GetWeakPtr(),
                             run_loop.QuitClosure())},
        {"ReceiveMessages",
         base::BindRepeating(&FtlSignalingPlayground::StartReceivingMessages,
                             weak_factory_.GetWeakPtr(),
                             run_loop.QuitWhenIdleClosure())},
        {"SendMessage",
         base::BindRepeating(&FtlSignalingPlayground::SendMessage,
                             weak_factory_.GetWeakPtr(),
                             run_loop.QuitClosure())},
        {"Quit", base::NullCallback()}};

    if (NeedsManualSignin()) {
      options.insert(options.begin(),
                     {"SignInGaia",
                      base::BindRepeating(&FtlSignalingPlayground::SignInGaia,
                                          weak_factory_.GetWeakPtr(),
                                          run_loop.QuitClosure())});
    }

    printf("\nOptions:\n");
    int print_option_number = 1;
    for (const auto& option : options) {
      printf("  %d. %s\n", print_option_number, option.name.c_str());
      print_option_number++;
    }
    printf("\nYour choice [number]: ");
    int choice = 0;
    base::StringToInt(ReadString(), &choice);
    if (choice < 1 || static_cast<size_t>(choice) > options.size()) {
      fprintf(stderr, "Unknown option\n");
      continue;
    }
    auto& callback = options[choice - 1].callback;
    if (callback.is_null()) {
      // Quit
      return;
    }
    callback.Run();
    run_loop.Run();
  }
}

void FtlSignalingPlayground::ResetServices(base::OnceClosure on_done) {
  executor_ = std::make_unique<GrpcAuthenticatedExecutor>(token_getter_.get());
  peer_to_peer_stub_ = PeerToPeer::NewStub(FtlGrpcContext::CreateChannel());

  registration_manager_ = std::make_unique<FtlRegistrationManager>(
      token_getter_.get(),
      std::make_unique<FtlDeviceIdProvider>(storage_.get()));

  message_subscription_.reset();
  messaging_client_ = std::make_unique<FtlMessagingClient>(
      token_getter_.get(), registration_manager_.get());
  message_subscription_ = messaging_client_->RegisterMessageCallback(
      base::BindRepeating(&FtlSignalingPlayground::OnMessageReceived,
                          weak_factory_.GetWeakPtr()));

  if (NeedsManualSignin()) {
    std::move(on_done).Run();
  } else {
    SignInGaia(std::move(on_done));
  }
}

void FtlSignalingPlayground::AuthenticateAndResetServices(
    base::OnceClosure on_done) {
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
  base::OnceClosure on_access_token_done =
      base::BindOnce(&FtlSignalingPlayground::ResetServices,
                     weak_factory_.GetWeakPtr(), std::move(on_done));
  token_getter_->CallWithToken(base::BindOnce(
      &FtlSignalingPlayground::OnAccessToken, weak_factory_.GetWeakPtr(),
      std::move(on_access_token_done)));
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
  ftl::GetICEServerRequest request;
  *request.mutable_header() = FtlGrpcContext::CreateRequestHeader();
  auto grpc_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&PeerToPeer::Stub::AsyncGetICEServer,
                     base::Unretained(peer_to_peer_stub_.get())),
      FtlGrpcContext::CreateClientContext(), request,
      base::BindOnce(&FtlSignalingPlayground::OnGetIceServerResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
  executor_->ExecuteRpc(std::move(grpc_request));
}

void FtlSignalingPlayground::OnGetIceServerResponse(
    base::OnceClosure on_done,
    const grpc::Status& status,
    const ftl::GetICEServerResponse& response) {
  if (!status.ok()) {
    HandleGrpcStatusError(std::move(on_done), status);
    return;
  }

  printf("Ice transport policy: %s\n",
         response.ice_config().ice_transport_policy().c_str());
  for (const ftl::ICEServerList& server : response.ice_config().ice_servers()) {
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
  if (!status.ok()) {
    HandleGrpcStatusError(std::move(on_done), status);
    return;
  }

  std::string registration_id_base64;
  base::Base64Encode(registration_manager_->GetRegistrationId(),
                     &registration_id_base64);
  printf("Service signed in. registration_id(base64)=%s\n",
         registration_id_base64.c_str());
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
    HandleGrpcStatusError(std::move(on_done), status);
    return;
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
    HandleGrpcStatusError(base::BindOnce(std::move(on_continue), false),
                          status);
    return;
  }

  printf("Message successfully sent.\n");
  std::move(on_continue).Run(true);
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
    HandleGrpcStatusError(std::move(on_done), status);
    return;
  }
  printf("Started receiving messages. Press enter to stop streaming...\n");
  WaitForEnterKey(base::BindOnce(&FtlSignalingPlayground::StopReceivingMessages,
                                 weak_factory_.GetWeakPtr(),
                                 std::move(on_done)));
}

void FtlSignalingPlayground::HandleGrpcStatusError(base::OnceClosure on_done,
                                                   const grpc::Status& status) {
  DCHECK(!status.ok());
  if (status.error_code() == grpc::StatusCode::UNAUTHENTICATED) {
    if (NeedsManualSignin()) {
      printf(
          "Request is unauthenticated. You should run SignInGaia first if "
          "you haven't done so, otherwise your OAuth token might be expired. \n"
          "Request for new OAuth token? [y/N]: ");
      std::string result = ReadString();
      if (result != "y" && result != "Y") {
        std::move(on_done).Run();
        return;
      }
    }
    VLOG(0) << "Grpc request failed to authenticate. "
            << "Trying to reauthenticate...";
    AuthenticateAndResetServices(std::move(on_done));
    return;
  }

  fprintf(stderr, "RPC failed. Code=%d, Message=%s\n", status.error_code(),
          status.error_message().c_str());
  std::move(on_done).Run();
}

}  // namespace remoting
