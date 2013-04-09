// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <termios.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/host/service_urls.h"
#include "remoting/host/setup/host_starter.h"
#include "remoting/host/setup/oauth_helper.h"
#include "remoting/host/setup/pin_validator.h"
#include "remoting/host/url_request_context.h"

// A simple command-line app that registers and starts a host.

using remoting::HostStarter;

// True if the host was started successfully.
bool g_started = false;

// The main message loop.
MessageLoop* g_message_loop = NULL;

// Lets us hide the PIN that a user types.
void SetEcho(bool echo) {
  termios term;
  tcgetattr(STDIN_FILENO, &term);
  if (echo) {
    term.c_lflag |= ECHO;
  } else {
    term.c_lflag &= ~ECHO;
  }
  tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// Reads a newline-terminated string from stdin.
std::string ReadString(bool no_echo) {
  if (no_echo)
    SetEcho(false);
  const int kMaxLen = 1024;
  std::string str(kMaxLen, 0);
  char* result = fgets(&str[0], kMaxLen, stdin);
  if (no_echo) {
    printf("\n");
    SetEcho(true);
  }
  if (!result)
    return "";
  size_t newline_index = str.find('\n');
  if (newline_index != std::string::npos)
    str[newline_index] = '\0';
  str.resize(strlen(&str[0]));
  return str;
}

// Called when the HostStarter has finished.
void OnDone(HostStarter::Result result) {
  if (MessageLoop::current() != g_message_loop) {
    g_message_loop->PostTask(FROM_HERE, base::Bind(&OnDone, result));
    return;
  }
  switch (result) {
    case HostStarter::START_COMPLETE:
      g_started = true;
      break;
    case HostStarter::NETWORK_ERROR:
      fprintf(stderr, "Couldn't start host: network error.\n");
      break;
    case HostStarter::OAUTH_ERROR:
      fprintf(stderr, "Couldn't start host: OAuth error.\n");
      break;
    case HostStarter::START_ERROR:
      fprintf(stderr, "Couldn't start host.\n");
      break;
  }

  g_message_loop->QuitNow();
}

int main(int argc, char** argv) {
  // google_apis::GetOAuth2ClientID/Secret need a static CommandLine.
  CommandLine::Init(argc, argv);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  std::string host_name = command_line->GetSwitchValueASCII("name");
  std::string host_pin = command_line->GetSwitchValueASCII("pin");
  std::string auth_code = command_line->GetSwitchValueASCII("code");
  std::string redirect_url = command_line->GetSwitchValueASCII("redirect-url");

  if (host_name.empty()) {
    fprintf(stderr,
            "Usage: %s --name=<hostname> [--code=<auth-code>] [--pin=<PIN>] "
            "[--redirect-url=<redirectURL>]\n",
            argv[0]);
    return 1;
  }

  if (host_pin.empty()) {
    while (true) {
      fprintf(stdout, "Enter a six-digit PIN: ");
      fflush(stdout);
      host_pin = ReadString(true);
      if (!remoting::IsPinValid(host_pin)) {
        fprintf(stdout,
                "Please use a PIN consisting of at least six digits.\n");
        fflush(stdout);
        continue;
      }
      std::string host_pin_confirm;
      fprintf(stdout, "Enter the same PIN again: ");
      fflush(stdout);
      host_pin_confirm = ReadString(true);
      if (host_pin != host_pin_confirm) {
        fprintf(stdout, "You entered different PINs.\n");
        fflush(stdout);
        continue;
      }
      break;
    }
  } else {
    if (!remoting::IsPinValid(host_pin)) {
      fprintf(stderr, "Please use a PIN consisting of at least six digits.\n");
      return 1;
    }
  }

  if (auth_code.empty()) {
    fprintf(stdout, "Enter an authorization code: ");
    fflush(stdout);
    auth_code = ReadString(true);
  }

  // This object instance is required by Chrome code (for example,
  // FilePath, LazyInstance, MessageLoop).
  base::AtExitManager exit_manager;

  // Provide message loops and threads for the URLRequestContextGetter.
  MessageLoop message_loop;
  g_message_loop = &message_loop;
  base::Thread io_thread("IO thread");
  base::Thread::Options io_thread_options(MessageLoop::TYPE_IO, 0);
  io_thread.StartWithOptions(io_thread_options);

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter(
      new remoting::URLRequestContextGetter(
          g_message_loop->message_loop_proxy(),
          io_thread.message_loop_proxy()));

  if (remoting::ServiceUrls::GetInstance()->ignore_urlfetcher_cert_requests()) {
    net::URLFetcher::SetIgnoreCertificateRequests(true);
  }

  // Start the host.
  scoped_ptr<HostStarter> host_starter(
      HostStarter::Create(
          GaiaUrls::GetInstance()->oauth2_token_url(),
          remoting::ServiceUrls::GetInstance()->directory_hosts_url(),
          url_request_context_getter));
  if (redirect_url.empty()) {
    redirect_url = remoting::GetDefaultOauthRedirectUrl();
  }
  host_starter->StartHost(host_name, host_pin, true, auth_code, redirect_url,
                          base::Bind(&OnDone));

  // Run the message loop until the StartHost completion callback.
  base::RunLoop run_loop;
  run_loop.Run();

  g_message_loop = NULL;

  // Destroy the HostStarter and URLRequestContextGetter before stopping the
  // IO thread.
  host_starter.reset();
  url_request_context_getter = NULL;

  io_thread.Stop();

  return g_started ? 0 : 1;
}
