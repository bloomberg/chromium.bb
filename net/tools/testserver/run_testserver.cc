// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "net/test/test_server.h"

static void PrintUsage() {
  printf("run_testserver --doc-root=relpath\n"
         "               [--http|--https|--ws|--wss|--ftp]\n"
         "               [--ssl-cert=ok|mismatched-name|expired]\n");
  printf("(NOTE: relpath should be relative to the 'src' directory.\n");
}

int main(int argc, const char* argv[]) {
  base::AtExitManager at_exit_manager;
  MessageLoopForIO message_loop;

  // Process command line
  CommandLine::Init(argc, argv);
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (!logging::InitLogging(
          FILE_PATH_LITERAL("testserver.log"),
          logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
          logging::LOCK_LOG_FILE,
          logging::APPEND_TO_OLD_LOG_FILE,
          logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS)) {
    printf("Error: could not initialize logging. Exiting.\n");
    return -1;
  }

  TestTimeouts::Initialize();

  if (command_line->GetSwitches().empty() ||
      command_line->HasSwitch("help")) {
    PrintUsage();
    return -1;
  }

  net::TestServer::Type server_type;
  if (command_line->HasSwitch("http")) {
    server_type = net::TestServer::TYPE_HTTP;
  } else if (command_line->HasSwitch("https")) {
    server_type = net::TestServer::TYPE_HTTPS;
  } else if (command_line->HasSwitch("ws")) {
    server_type = net::TestServer::TYPE_WS;
  } else if (command_line->HasSwitch("wss")) {
    server_type = net::TestServer::TYPE_WSS;
  } else if (command_line->HasSwitch("ftp")) {
    server_type = net::TestServer::TYPE_FTP;
  } else {
    // If no scheme switch is specified, select http or https scheme.
    // TODO(toyoshim): Remove this estimation.
    if (command_line->HasSwitch("ssl-cert"))
      server_type = net::TestServer::TYPE_HTTPS;
    else
      server_type = net::TestServer::TYPE_HTTP;
  }

  net::TestServer::SSLOptions ssl_options;
  if (command_line->HasSwitch("ssl-cert")) {
    if (!net::TestServer::UsingSSL(server_type)) {
      printf("Error: --ssl-cert is specified on non-secure scheme\n");
      PrintUsage();
      return -1;
    }
    std::string cert_option = command_line->GetSwitchValueASCII("ssl-cert");
    if (cert_option == "ok") {
      ssl_options.server_certificate = net::TestServer::SSLOptions::CERT_OK;
    } else if (cert_option == "mismatched-name") {
      ssl_options.server_certificate =
          net::TestServer::SSLOptions::CERT_MISMATCHED_NAME;
    } else if (cert_option == "expired") {
      ssl_options.server_certificate =
          net::TestServer::SSLOptions::CERT_EXPIRED;
    } else {
      printf("Error: --ssl-cert has invalid value %s\n", cert_option.c_str());
      PrintUsage();
      return -1;
    }
  }

  base::FilePath doc_root = command_line->GetSwitchValuePath("doc-root");
  if (doc_root.empty()) {
    printf("Error: --doc-root must be specified\n");
    PrintUsage();
    return -1;
  }

  scoped_ptr<net::TestServer> test_server;
  if (net::TestServer::UsingSSL(server_type)) {
    test_server.reset(new net::TestServer(server_type, ssl_options, doc_root));
  } else {
    test_server.reset(new net::TestServer(server_type,
                                          net::TestServer::kLocalhost,
                                          doc_root));
  }

  if (!test_server->Start()) {
    printf("Error: failed to start test server. Exiting.\n");
    return -1;
  }

  if (!file_util::DirectoryExists(test_server->document_root())) {
    printf("Error: invalid doc root: \"%s\" does not exist!\n",
        UTF16ToUTF8(test_server->document_root().LossyDisplayName()).c_str());
    return -1;
  }

  printf("testserver running at %s (type ctrl+c to exit)\n",
         test_server->host_port_pair().ToString().c_str());

  message_loop.Run();
  return 0;
}
