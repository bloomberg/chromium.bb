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
#include "base/debug/leak_annotations.h"
#include "base/file_util.h"
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

namespace net {

namespace {

// Number of connection attempts for tests.
const int kServerConnectionAttempts = 10;

// Connection timeout in milliseconds for tests.
const int kServerConnectionTimeoutMs = 1000;

const char kTestServerShardFlag[] = "test-server-shard";

int GetHTTPSPortBase(const TestServer::HTTPSOptions& options) {
  if (options.request_client_certificate)
    return 9543;

  switch (options.server_certificate) {
    case TestServer::HTTPSOptions::CERT_OK:
      return 9443;
    case TestServer::HTTPSOptions::CERT_MISMATCHED_NAME:
      return 9643;
    case TestServer::HTTPSOptions::CERT_EXPIRED:
      // TODO(phajdan.jr): Some tests rely on this hardcoded value.
      // Some uses of this are actually in .html/.js files.
      return 9666;
    default:
      NOTREACHED();
  }
  return -1;
}

int GetPortBase(TestServer::Type type,
                const TestServer::HTTPSOptions& options) {
  switch (type) {
    case TestServer::TYPE_FTP:
      return 3117;
    case TestServer::TYPE_HTTP:
      return 1337;
    case TestServer::TYPE_HTTPS:
      return GetHTTPSPortBase(options);
    default:
      NOTREACHED();
  }
  return -1;
}

int GetPort(TestServer::Type type,
            const TestServer::HTTPSOptions& options) {
  int port = GetPortBase(type, options);
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

std::string GetHostname(TestServer::Type type,
                        const TestServer::HTTPSOptions& options) {
  if (type == TestServer::TYPE_HTTPS &&
      options.server_certificate ==
          TestServer::HTTPSOptions::CERT_MISMATCHED_NAME) {
    // Return a different hostname string that resolves to the same hostname.
    return "localhost";
  }

  return "127.0.0.1";
}

}  // namespace

#if defined(OS_MACOSX)
void SetMacTestCertificate(X509Certificate* cert);
#endif

TestServer::HTTPSOptions::HTTPSOptions()
    : server_certificate(CERT_OK),
      request_client_certificate(false),
      bulk_ciphers(HTTPSOptions::BULK_CIPHER_ANY) {}

TestServer::HTTPSOptions::HTTPSOptions(
    TestServer::HTTPSOptions::ServerCertificate cert)
    : server_certificate(cert),
      request_client_certificate(false),
      bulk_ciphers(HTTPSOptions::BULK_CIPHER_ANY) {}

TestServer::HTTPSOptions::~HTTPSOptions() {}

FilePath TestServer::HTTPSOptions::GetCertificateFile() const {
  switch (server_certificate) {
    case CERT_OK:
    case CERT_MISMATCHED_NAME:
      return FilePath(FILE_PATH_LITERAL("ok_cert.pem"));
    case CERT_EXPIRED:
      return FilePath(FILE_PATH_LITERAL("expired_cert.pem"));
    default:
      NOTREACHED();
  }
  return FilePath();
}

TestServer::TestServer(Type type, const FilePath& document_root)
    : type_(type) {
  Init(document_root);
}

TestServer::TestServer(const HTTPSOptions& https_options,
                       const FilePath& document_root)
    : https_options_(https_options), type_(TYPE_HTTPS) {
  Init(document_root);
}

TestServer::~TestServer() {
#if defined(OS_MACOSX)
  SetMacTestCertificate(NULL);
#endif
  Stop();
}

void TestServer::Init(const FilePath& document_root) {
  host_port_pair_ = HostPortPair(GetHostname(type_, https_options_),
                                 GetPort(type_, https_options_));
  process_handle_ = base::kNullProcessHandle;

  FilePath src_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &src_dir);

  document_root_ = src_dir.Append(document_root);

  certificates_dir_ = src_dir.Append(FILE_PATH_LITERAL("net"))
                       .Append(FILE_PATH_LITERAL("data"))
                       .Append(FILE_PATH_LITERAL("ssl"))
                       .Append(FILE_PATH_LITERAL("certificates"));
}

bool TestServer::Start() {
  if (type_ == TYPE_HTTPS) {
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
    VLOG(1) << "Kill failed?";
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
      return "https";
    default:
      NOTREACHED();
  }
  return std::string();
}

bool TestServer::GetAddressList(AddressList* address_list) const {
  DCHECK(address_list);

  scoped_ptr<HostResolver> resolver(
      CreateSystemHostResolver(HostResolver::kDefaultParallelism, NULL, NULL));
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

  static const FilePath kPyProto(FILE_PATH_LITERAL("pyproto"));

#if defined(OS_MACOSX)
  // On Mac, DIR_EXE might be pointing deep into the Release/ (or Debug/)
  // directory and we can't depend on how far down it goes. So we walk upwards
  // from DIR_EXE until we find a likely looking spot.
  while (!file_util::DirectoryExists(generated_code_dir.Append(kPyProto))) {
    FilePath parent = generated_code_dir.DirName();
    if (parent == generated_code_dir) {
      // We hit the root directory. Maybe we didn't build any targets which
      // produced Python protocol buffers.
      PathService::Get(base::DIR_EXE, &generated_code_dir);
      break;
    }
    generated_code_dir = parent;
  }
#endif

  AppendToPythonPath(generated_code_dir.Append(kPyProto));
  AppendToPythonPath(generated_code_dir.Append(kPyProto).
                     Append(FILE_PATH_LITERAL("sync_pb")));

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

bool TestServer::AddCommandLineArguments(CommandLine* command_line) const {
  command_line->AppendSwitchASCII("port",
                                  base::IntToString(host_port_pair_.port()));
  command_line->AppendSwitchPath("data-dir", document_root_);

  if (type_ == TYPE_FTP) {
    command_line->AppendArg("-f");
  } else if (type_ == TYPE_HTTPS) {
    FilePath certificate_path(certificates_dir_);
    certificate_path = certificate_path.Append(
        https_options_.GetCertificateFile());
    if (!file_util::PathExists(certificate_path)) {
      LOG(ERROR) << "Certificate path " << certificate_path.value()
                 << " doesn't exist. Can't launch https server.";
      return false;
    }
    command_line->AppendSwitchPath("https", certificate_path);

    if (https_options_.request_client_certificate)
      command_line->AppendSwitch("ssl-client-auth");

    for (std::vector<FilePath>::const_iterator it =
             https_options_.client_authorities.begin();
         it != https_options_.client_authorities.end(); ++it) {
      if (!file_util::PathExists(*it)) {
        LOG(ERROR) << "Client authority path " << it->value()
                   << " doesn't exist. Can't launch https server.";
        return false;
      }

      command_line->AppendSwitchPath("ssl-client-ca", *it);
    }

    const char kBulkCipherSwitch[] = "ssl-bulk-cipher";
    if (https_options_.bulk_ciphers & HTTPSOptions::BULK_CIPHER_RC4)
      command_line->AppendSwitchASCII(kBulkCipherSwitch, "rc4");
    if (https_options_.bulk_ciphers & HTTPSOptions::BULK_CIPHER_AES128)
      command_line->AppendSwitchASCII(kBulkCipherSwitch, "aes128");
    if (https_options_.bulk_ciphers & HTTPSOptions::BULK_CIPHER_AES256)
      command_line->AppendSwitchASCII(kBulkCipherSwitch, "aes256");
    if (https_options_.bulk_ciphers & HTTPSOptions::BULK_CIPHER_3DES)
      command_line->AppendSwitchASCII(kBulkCipherSwitch, "3des");
  }

  return true;
}

}  // namespace net
