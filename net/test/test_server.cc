// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_server.h"

#include <algorithm>
#include <string>
#include <vector>

#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "net/base/x509_certificate.h"
#endif

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/leak_annotations.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "net/base/cert_test_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "net/test/python_utils.h"
#include "testing/platform_test.h"

namespace {

// Number of connection attempts for tests.
const int kServerConnectionAttempts = 10;

// Connection timeout in milliseconds for tests.
const int kServerConnectionTimeoutMs = 1000;

const char kTestServerShardFlag[] = "test-server-shard";

int GetPortBase(net::TestServer::Type type) {
  switch (type) {
    case net::TestServer::TYPE_FTP:
      return 3117;
    case net::TestServer::TYPE_HTTP:
      return 1337;
    case net::TestServer::TYPE_HTTPS:
      return 9443;
    case net::TestServer::TYPE_HTTPS_CLIENT_AUTH:
      return 9543;
    case net::TestServer::TYPE_HTTPS_EXPIRED_CERTIFICATE:
      // TODO(phajdan.jr): Some tests rely on this hardcoded value.
      // Some uses of this are actually in .html/.js files.
      return 9666;
    case net::TestServer::TYPE_HTTPS_MISMATCHED_HOSTNAME:
      return 9643;
    default:
      NOTREACHED();
  }
  return -1;
}

int GetPort(net::TestServer::Type type) {
  int port = GetPortBase(type);
  if (CommandLine::ForCurrentProcess()->HasSwitch(kTestServerShardFlag)) {
    std::string shard_str(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                              kTestServerShardFlag));
    int shard = -1;
    if (base::StringToInt(shard_str, &shard)) {
      port += shard;
    } else {
      LOG(FATAL) << "Got invalid " << kTestServerShardFlag << " flag value. "
                 << "An integer is expected.";
    }
  }
  return port;
}

std::string GetHostname(net::TestServer::Type type) {
  if (type == net::TestServer::TYPE_HTTPS_MISMATCHED_HOSTNAME) {
    // Return a different hostname string that resolves to the same hostname.
    return "localhost";
  }

  return "127.0.0.1";
}

}  // namespace

namespace net {

#if defined(OS_MACOSX)
void SetMacTestCertificate(X509Certificate* cert);
#endif

TestServer::TestServer(Type type, const FilePath& document_root)
    : host_port_pair_(GetHostname(type), GetPort(type)),
      process_handle_(base::kNullProcessHandle),
      type_(type) {
  FilePath src_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &src_dir);

  document_root_ = src_dir.Append(document_root);

  certificates_dir_ = src_dir.Append(FILE_PATH_LITERAL("net"))
                       .Append(FILE_PATH_LITERAL("data"))
                       .Append(FILE_PATH_LITERAL("ssl"))
                       .Append(FILE_PATH_LITERAL("certificates"));
}

TestServer::~TestServer() {
#if defined(OS_MACOSX)
  SetMacTestCertificate(NULL);
#endif
  Stop();
}

bool TestServer::Start() {
  if (GetScheme() == "https") {
    if (!LoadTestRootCert())
      return false;
    if (!CheckCATrusted())
      return false;
  }

  // Get path to python server script
  FilePath testserver_path;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &testserver_path)) {
    LOG(ERROR) << "Failed to get DIR_SOURCE_ROOT";
    return false;
  }
  testserver_path = testserver_path
      .Append(FILE_PATH_LITERAL("net"))
      .Append(FILE_PATH_LITERAL("tools"))
      .Append(FILE_PATH_LITERAL("testserver"))
      .Append(FILE_PATH_LITERAL("testserver.py"));

  if (!SetPythonPath())
    return false;

  if (!LaunchPython(testserver_path))
    return false;

  if (!WaitToStart()) {
    Stop();
    return false;
  }

  return true;
}

bool TestServer::Stop() {
  if (!process_handle_)
    return true;

  // First check if the process has already terminated.
  bool ret = base::WaitForSingleProcess(process_handle_, 0);
  if (!ret)
    ret = base::KillProcess(process_handle_, 1, true);

  if (ret) {
    base::CloseProcessHandle(process_handle_);
    process_handle_ = base::kNullProcessHandle;
  } else {
    LOG(INFO) << "Kill failed?";
  }

  return ret;
}

bool TestServer::WaitToFinish(int timeout_ms) {
  if (!process_handle_)
    return true;

  bool ret = base::WaitForSingleProcess(process_handle_, timeout_ms);
  if (ret) {
    base::CloseProcessHandle(process_handle_);
    process_handle_ = base::kNullProcessHandle;
  } else {
    LOG(ERROR) << "Timed out.";
  }
  return ret;
}

std::string TestServer::GetScheme() const {
  switch (type_) {
    case TYPE_FTP:
      return "ftp";
    case TYPE_HTTP:
      return "http";
    case TYPE_HTTPS:
    case TYPE_HTTPS_CLIENT_AUTH:
    case TYPE_HTTPS_MISMATCHED_HOSTNAME:
    case TYPE_HTTPS_EXPIRED_CERTIFICATE:
      return "https";
    default:
      NOTREACHED();
  }
  return std::string();
}

bool TestServer::GetAddressList(AddressList* address_list) const {
  DCHECK(address_list);

  scoped_refptr<HostResolver> resolver(
      CreateSystemHostResolver(HostResolver::kDefaultParallelism, NULL));
  HostResolver::RequestInfo info(host_port_pair_);
  int rv = resolver->Resolve(info, address_list, NULL, NULL, BoundNetLog());
  if (rv != net::OK) {
    LOG(ERROR) << "Failed to resolve hostname: " << host_port_pair_.host();
    return false;
  }
  return true;
}

GURL TestServer::GetURL(const std::string& path) {
  return GURL(GetScheme() + "://" + host_port_pair_.ToString() +
              "/" + path);
}

GURL TestServer::GetURLWithUser(const std::string& path,
                                const std::string& user) {
  return GURL(GetScheme() + "://" + user + "@" +
              host_port_pair_.ToString() +
              "/" + path);
}

GURL TestServer::GetURLWithUserAndPassword(const std::string& path,
                                           const std::string& user,
                                           const std::string& password) {
  return GURL(GetScheme() + "://" + user + ":" + password +
              "@" + host_port_pair_.ToString() +
              "/" + path);
}

bool TestServer::SetPythonPath() {
  FilePath third_party_dir;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &third_party_dir)) {
    LOG(ERROR) << "Failed to get DIR_SOURCE_ROOT";
    return false;
  }
  third_party_dir = third_party_dir.Append(FILE_PATH_LITERAL("third_party"));

  AppendToPythonPath(third_party_dir.Append(FILE_PATH_LITERAL("tlslite")));
  AppendToPythonPath(third_party_dir.Append(FILE_PATH_LITERAL("pyftpdlib")));

  // Locate the Python code generated by the protocol buffers compiler.
  FilePath generated_code_dir;
  if (!PathService::Get(base::DIR_EXE, &generated_code_dir)) {
    LOG(ERROR) << "Failed to get DIR_EXE";
    return false;
  }
  generated_code_dir = generated_code_dir.Append(FILE_PATH_LITERAL("pyproto"));
  AppendToPythonPath(generated_code_dir);
  AppendToPythonPath(generated_code_dir.Append(FILE_PATH_LITERAL("sync_pb")));

  return true;
}

FilePath TestServer::GetRootCertificatePath() {
  return certificates_dir_.AppendASCII("root_ca_cert.crt");
}

bool TestServer::LoadTestRootCert() {
#if defined(USE_NSS)
  if (cert_)
    return true;

  // TODO(dkegel): figure out how to get this to only happen once?

  // This currently leaks a little memory.
  // TODO(dkegel): fix the leak and remove the entry in
  // tools/valgrind/memcheck/suppressions.txt
  ANNOTATE_SCOPED_MEMORY_LEAK;  // Tell heap checker about the leak.
  cert_ = LoadTemporaryRootCert(GetRootCertificatePath());
  return (cert_ != NULL);
#elif defined(OS_MACOSX)
  X509Certificate* cert = LoadTemporaryRootCert(GetRootCertificatePath());
  if (!cert)
    return false;
  SetMacTestCertificate(cert);
  return true;
#else
  return true;
#endif
}

FilePath TestServer::GetCertificatePath() {
  switch (type_) {
    case TYPE_FTP:
    case TYPE_HTTP:
      return FilePath();
    case TYPE_HTTPS:
    case TYPE_HTTPS_CLIENT_AUTH:
    case TYPE_HTTPS_MISMATCHED_HOSTNAME:
      return certificates_dir_.AppendASCII("ok_cert.pem");
    case TYPE_HTTPS_EXPIRED_CERTIFICATE:
      return certificates_dir_.AppendASCII("expired_cert.pem");
    default:
      NOTREACHED();
  }
  return FilePath();
}

}  // namespace net
