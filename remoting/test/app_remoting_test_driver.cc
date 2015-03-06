// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "remoting/test/app_remoting_test_driver_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace switches {
const char kAuthCodeSwitchName[] = "authcode";
const char kHelpSwitchName[] = "help";
const char kLoggingLevelSwitchName[] = "verbosity";
const char kServiceEnvironmentSwitchName[] = "environment";
const char kSingleProcessTestsSwitchName[] = "single-process-tests";
const char kUserNameSwitchName[] = "username";
}

namespace {

// Requested permissions needed for App Remoting tests.  The spaces in between
// scope fragments are necessary and will be escaped properly before use.
const char kAppRemotingAuthScopeValues[] =
    "https://www.googleapis.com/auth/appremoting.runapplication"
    " https://www.googleapis.com/auth/googletalk"
    " https://www.googleapis.com/auth/userinfo.email"
    " https://docs.google.com/feeds"
    " https://www.googleapis.com/auth/drive";

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
      net::EscapeUrlEncodedData(kAppRemotingAuthScopeValues, use_plus).c_str(),
      net::EscapeUrlEncodedData(google_apis::GetOAuth2ClientID(
          google_apis::CLIENT_REMOTING), use_plus).c_str());
}

void PrintUsage() {
  printf("\n**************************************\n");
  printf("*** App Remoting Test Driver Usage ***\n");
  printf("**************************************\n");

  printf("\nUsage:\n");
  printf("  ar_test_driver --username=<example@gmail.com> [options]\n");
  printf("\nRequired Parameters:\n");
  printf("  %s: Specifies which account to use when running tests\n",
         switches::kUserNameSwitchName);
  printf("\nOptional Parameters:\n");
  printf("  %s: Exchanged for a refresh and access token for authentication\n",
         switches::kAuthCodeSwitchName);
  printf("  %s: Displays additional usage information\n",
         switches::kHelpSwitchName);
  printf("  %s: Specifies the service api to use (dev|test) [default: dev]\n",
         switches::kServiceEnvironmentSwitchName);
  printf(
      "  %s: Specifies the optional logging level of the tool (0-3)."
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
  printf("ar_test_driver --%s=example@gmail.com --%s=4/AKtf...\n\n",
         switches::kUserNameSwitchName,
         switches::kAuthCodeSwitchName);
}

}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  TestSuite test_suite(argc, argv);

  // The pointer returned here refers to a singleton, since we don't own the
  // lifetime of the object, don't wrap in a scoped_ptr construct or release it.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  DCHECK(command_line);

  // We do not want to retry failures as a failed test should signify an error
  // to be investigated.
  command_line->AppendSwitchASCII(switches::kTestLauncherRetryLimit, "0");

  // We do not want to run the tests in parallel and we do not want to retry
  // failures.  The reason for running in a single process is that some tests
  // may share the same remoting host and they cannot be run concurrently, also
  // the test output gets spammed with test launcher messages which reduces the
  // readability of the results.
  command_line->AppendSwitch(switches::kSingleProcessTestsSwitchName);

  // If the user passed in the help flag, then show the help info for this tool
  // and 'run' the tests which will print the gtest specific help and then exit.
  // NOTE: We do this check after updating the switches as otherwise the gtest
  //       help is written in parallel with our text and can appear interleaved.
  if (command_line->HasSwitch(switches::kHelpSwitchName)) {
    PrintUsage();
    PrintAuthCodeInfo();
    return base::LaunchUnitTestsSerially(
        argc,
        argv,
        base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
  }

  // Verify we received the required input from the command line.
  if (!command_line->HasSwitch(switches::kUserNameSwitchName)) {
    LOG(ERROR) << "No user name passed in, can't authenticate without that!";
    PrintUsage();
    return -1;
  }

  std::string user_name;
  user_name = command_line->GetSwitchValueASCII(switches::kUserNameSwitchName);
  DVLOG(1) << "Running tests as: " << user_name;

  std::string auth_code;
  // Check to see if the user passed in a one time use auth_code for
  // refreshing their credentials.
  auth_code = command_line->GetSwitchValueASCII(switches::kAuthCodeSwitchName);

  // If the user passed in a service environment, use it, otherwise set a
  // default value.
  remoting::test::ServiceEnvironment service_environment;
  std::string service_environment_switch = command_line->GetSwitchValueASCII(
      switches::kServiceEnvironmentSwitchName);
  if (service_environment_switch.empty() ||
      service_environment_switch == "dev") {
    service_environment =
        remoting::test::ServiceEnvironment::kDeveloperEnvironment;
  } else if (service_environment_switch == "test") {
    service_environment =
        remoting::test::ServiceEnvironment::kTestingEnvironment;
  } else {
    LOG(ERROR) << "Invalid " << switches::kServiceEnvironmentSwitchName
               << " argument passed in.";
    PrintUsage();
    return -1;
  }

  // Update the logging verbosity level is user specified one.
  std::string verbosity_level;
  verbosity_level =
        command_line->GetSwitchValueASCII(switches::kLoggingLevelSwitchName);
  if (!verbosity_level.empty()) {
    // Turn on logging for the test_driver and remoting components.
    // This switch is parsed during logging::InitLogging.
    command_line->AppendSwitchASCII("vmodule",
                                    "*/remoting/*=" + verbosity_level);
    logging::LoggingSettings logging_settings;
    logging::InitLogging(logging_settings);
  }

  // Create and register our global test data object.  It will handle
  // retrieving an access token for the user and spinning up VMs.
  // The GTest framework will own the lifetime of this object once
  // it is registered below.
  scoped_ptr<remoting::test::AppRemotingTestDriverEnvironment> shared_data;

  shared_data.reset(new remoting::test::AppRemotingTestDriverEnvironment(
      user_name, service_environment));

  if (!shared_data->Initialize(auth_code)) {
    // If we failed to initialize our shared data object, then bail.
    return -1;
  }

  // Since we've successfully set up our shared_data object, we'll assign the
  // value to our global* and transfer ownership to the framework.
  remoting::test::AppRemotingSharedData = shared_data.release();
  testing::AddGlobalTestEnvironment(remoting::test::AppRemotingSharedData);

  // Because many tests may access the same remoting host(s), we need to run
  // the tests sequentially so they do not interfere with each other.
  return base::LaunchUnitTestsSerially(
      argc,
      argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
