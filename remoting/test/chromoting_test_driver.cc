// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "remoting/test/access_token_fetcher.h"
#include "remoting/test/host_info.h"
#include "remoting/test/host_list_fetcher.h"
#include "remoting/test/refresh_token_store.h"
#include "remoting/test/test_chromoting_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace switches {
const char kAuthCodeSwitchName[] = "authcode";
const char kPinSwitchName[] = "pin";
const char kHelpSwitchName[] = "help";
const char kHostNameSwitchName[] = "hostname";
const char kLoggingLevelSwitchName[] = "verbosity";
const char kRefreshTokenPathSwitchName[] = "refresh-token-path";
const char kSingleProcessTestsSwitchName[] = "single-process-tests";
const char kUserNameSwitchName[] = "username";
}

namespace {
const char kChromotingAuthScopeValues[] =
    "https://www.googleapis.com/auth/chromoting "
    "https://www.googleapis.com/auth/googletalk "
    "https://www.googleapis.com/auth/userinfo.email";

const unsigned int kConnectionTimeoutSeconds = 10;

std::string GetAuthorizationCodeUri() {
  // Replace space characters with a '+' sign when formatting.
  bool use_plus = true;
  return base::StringPrintf(
      "https://accounts.google.com/o/oauth2/auth"
      "?scope=%s"
      "&redirect_uri=https://chromoting-oauth.talkgadget.google.com/"
      "talkgadget/oauth/chrome-remote-desktop/dev"
      "&response_type=code"
      "&client_id=%s"
      "&access_type=offline"
      "&approval_prompt=force",
      net::EscapeUrlEncodedData(kChromotingAuthScopeValues, use_plus).c_str(),
      net::EscapeUrlEncodedData(
          google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING),
          use_plus).c_str());
}

void PrintUsage() {
  printf("\n************************************\n");
  printf("*** Chromoting Test Driver Usage ***\n");
  printf("************************************\n");

  printf("\nUsage:\n");
  printf("  chromoting_test_driver --username=<example@gmail.com> [options]"
         " --hostname=<example hostname>\n");
  printf("\nRequired Parameters:\n");
  printf("  %s: Specifies which account to use when running tests\n",
         switches::kUserNameSwitchName);
  printf("  %s: Specifies which host to connect to when running tests\n",
         switches::kHostNameSwitchName);
  printf("\nOptional Parameters:\n");
  printf("  %s: Exchanged for a refresh and access token for authentication\n",
         switches::kAuthCodeSwitchName);
  printf("  %s: Displays additional usage information\n",
         switches::kHelpSwitchName);
  printf("  %s: Path to a JSON file containing username/refresh_token KVPs\n",
         switches::kRefreshTokenPathSwitchName);
  printf("  %s: Used to authenticate a chromoting connection with the host\n",
         switches::kPinSwitchName);
  printf("  %s: Specifies the optional logging level of the tool (0-3)."
         " [default: off]\n",
         switches::kLoggingLevelSwitchName);
}

void PrintAuthCodeInfo() {
  printf("\n*******************************\n");
  printf("*** Auth Code Example Usage ***\n");
  printf("*******************************\n\n");

  printf("If this is the first time you are running the tool,\n");
  printf("you will need to provide an authorization code.\n");
  printf("This code will be exchanged for a long term refresh token which\n");
  printf("will be stored locally and used to acquire a short lived access\n");
  printf("token to connect to the remoting service apis and establish a\n");
  printf("remote host connection.\n\n");

  printf("Note: You may need to repeat this step if the stored refresh token");
  printf("\n      has been revoked or expired.\n");
  printf("      Passing in the same auth code twice will result in an error\n");

  printf("\nFollow these steps to produce an auth code:\n"
         " - Open the Authorization URL link shown below in your browser\n"
         " - Approve the requested permissions for the tool\n"
         " - Copy the 'code' value in the redirected URL\n"
         " - Run the tool and pass in copied auth code as a parameter\n");

  printf("\nAuthorization URL:\n");
  printf("%s\n", GetAuthorizationCodeUri().c_str());

  printf("\nRedirected URL Example:\n");
  printf("https://chromoting-oauth.talkgadget.google.com/talkgadget/oauth/"
         "chrome-remote-desktop/dev?code=4/AKtf...\n");

  printf("\nTool usage example with the newly created auth code:\n");
  printf("chromoting_test_driver --%s=example@gmail.com --%s=example_host_name"
         " --%s=4/AKtf...\n\n",
         switches::kUserNameSwitchName,
         switches::kHostNameSwitchName,
         switches::kAuthCodeSwitchName);
}

void PrintJsonFileInfo() {
  printf("\n****************************************\n");
  printf("*** Refresh Token File Example Usage ***\n");
  printf("****************************************\n\n");

  printf("In order to use this option, a valid JSON file must exist, be\n");
  printf("properly formatted, and contain a username/token KVP.\n");
  printf("Contents of example_file.json\n");
  printf("{\n");
  printf("  \"username1@fauxdomain.com\": \"1/3798Gsdf898shksdvfyi8sshad\",\n");
  printf("  \"username2@fauxdomain.com\": \"1/8974sdf87asdgadfgaerhfRsAa\",\n");
  printf("}\n\n");

  printf("\nTool usage example:\n");
  printf("chromoting_test_driver --%s=%s --%s=example_host_name"
         " --%s=./example_file.json\n\n",
         switches::kUserNameSwitchName, "username1@fauxdomain.com",
         switches::kHostNameSwitchName, switches::kRefreshTokenPathSwitchName);
}

}  // namespace

void OnHostlistRetrieved(
    base::Closure done_closure,
    std::vector<remoting::test::HostInfo>* hostlist,
    const std::vector<remoting::test::HostInfo>& retrieved_hostlist) {

  VLOG(1) << "OnHostlistRetrieved() Called";

  DCHECK(hostlist);

  *hostlist = retrieved_hostlist;

  VLOG(1) << "There are " << hostlist->size() << " hosts in the hostlist";

  done_closure.Run();
}

void OnAccessTokenRetrieved(
    base::Closure done_closure,
    std::string* access_token,
    const std::string& retrieved_access_token,
    const std::string& retrieved_refresh_token) {

  VLOG(1) << "OnAccessTokenRetrieved() Called";
  VLOG(1) << "Access Token: " << retrieved_access_token;

  *access_token = retrieved_access_token;

  done_closure.Run();
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  base::TestSuite test_suite(argc, argv);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  DCHECK(command_line);

  // Do not retry if tests fails.
  command_line->AppendSwitchASCII(switches::kTestLauncherRetryLimit, "0");

  // Different tests may require access to the same host if run in parallel.
  // To avoid shared resource contention, tests will be run one at a time.
  command_line->AppendSwitch(switches::kSingleProcessTestsSwitchName);

  if (command_line->HasSwitch(switches::kHelpSwitchName)) {
    PrintUsage();
    PrintJsonFileInfo();
    PrintAuthCodeInfo();
    return 0;
  }

  // Update the logging verbosity level is user specified one.
  std::string verbosity_level(
      command_line->GetSwitchValueASCII(switches::kLoggingLevelSwitchName));
  if (!verbosity_level.empty()) {
    // Turn on logging for the test_driver and remoting components.
    // This switch is parsed during logging::InitLogging.
    command_line->AppendSwitchASCII("vmodule",
                                    "*/remoting/*=" + verbosity_level);
    logging::LoggingSettings logging_settings;
    logging::InitLogging(logging_settings);
  }

  // The username is used to run the tests and determines which refresh token to
  // select in the refresh token file.
  const std::string username =
      command_line->GetSwitchValueASCII(switches::kUserNameSwitchName);
  if (username.empty()) {
    LOG(ERROR) << "No username passed in, can't authenticate or run tests!";
    return -1;
  }
  VLOG(1) << "Running chromoting tests as: " << username;

  // Check to see if the user passed in a one time use auth_code for
  // refreshing their credentials.
  std::string auth_code =
      command_line->GetSwitchValueASCII(switches::kAuthCodeSwitchName);

  const base::FilePath refresh_token_path =
     command_line->GetSwitchValuePath(switches::kRefreshTokenPathSwitchName);

  // The hostname determines which host to initiate a session with from the list
  // returned from the directory service.
  const std::string hostname =
      command_line->GetSwitchValueASCII(switches::kHostNameSwitchName);
  if (hostname.empty()) {
    LOG(ERROR) << "No hostname passed in, finding the host requires hostname!";
    return -1;
  }
  VLOG(1) << "Chromoting tests will connect to: " << hostname;

  const std::string pin =
      command_line->GetSwitchValueASCII(switches::kPinSwitchName);

  // TODO(TonyChun): Move this logic into a shared environment class.
  scoped_ptr<remoting::test::RefreshTokenStore> refresh_token_store =
      remoting::test::RefreshTokenStore::OnDisk(username, refresh_token_path);

  std::string refresh_token = refresh_token_store->FetchRefreshToken();
  if (auth_code.empty() && refresh_token.empty()) {
    // RefreshTokenStore already logs which specific error occured.
    return -1;
  }

  // Used for running network request tasks.
  // TODO(TonyChun): Move this logic into a shared environment class.
  base::MessageLoopForIO message_loop;

  // Uses the refresh token to get the access token from GAIA.
  remoting::test::AccessTokenFetcher access_token_fetcher;

  // A RunLoop that yields to the thread's MessageLoop.
  scoped_ptr<base::RunLoop> run_loop;

  // RunLoop to handle callback from GAIA.
  run_loop.reset(new base::RunLoop());

  std::string access_token;
  remoting::test::AccessTokenCallback access_token_callback =
      base::Bind(&OnAccessTokenRetrieved,
                 run_loop->QuitClosure(),
                 &access_token);

  if (!auth_code.empty()) {
    access_token_fetcher.GetAccessTokenFromAuthCode(auth_code,
                                                    access_token_callback);
  } else {
    DCHECK(!refresh_token.empty());
    access_token_fetcher.GetAccessTokenFromRefreshToken(refresh_token,
                                                        access_token_callback);
  }

  run_loop->Run();

  run_loop.reset(new base::RunLoop());

  std::vector<remoting::test::HostInfo> hostlist;
  remoting::test::HostListFetcher::HostlistCallback hostlist_request_callback =
      base::Bind(&OnHostlistRetrieved, run_loop->QuitClosure(), &hostlist);

  // Uses the access token to get the hostlist from the directory service.
  remoting::test::HostListFetcher hostlist_fetcher;
  hostlist_fetcher.RetrieveHostlist(access_token, hostlist_request_callback);

  run_loop->Run();

  run_loop.reset(new base::RunLoop());

  remoting::test::TestChromotingClient test_chromoting_client;

  // Check if requested host is online and ready to receive connections.
  auto it = std::find_if(hostlist.begin(), hostlist.end(),
                        [&hostname](const remoting::test::HostInfo& host_info) {
                          return host_info.host_name == hostname &&
                                 host_info.IsReadyForConnection();
                        });
  if (it != hostlist.end()) {
    // Host is online and ready, initiate a remote session.
    base::Timer timer(true, false);
    timer.Start(FROM_HERE,
                base::TimeDelta::FromSeconds(kConnectionTimeoutSeconds),
                run_loop->QuitClosure());
    test_chromoting_client.StartConnection(
        it->GenerateConnectionSetupInfo(access_token, username, pin));
    run_loop->Run();
    test_chromoting_client.EndConnection();
  } else {
    LOG(ERROR) << "Requested host not found or not ready to connect";
  }

  return 0;
}
