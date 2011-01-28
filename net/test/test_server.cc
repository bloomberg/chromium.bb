// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

#include "base/base64.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/json/json_reader.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_root_certs.h"
#include "net/socket/tcp_client_socket.h"
#include "net/test/python_utils.h"
#include "testing/platform_test.h"

namespace net {

namespace {

// Number of connection attempts for tests.
const int kServerConnectionAttempts = 10;

// Connection timeout in milliseconds for tests.
const int kServerConnectionTimeoutMs = 1000;

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
    : type_(type),
      started_(false) {
  Init(document_root);
}

TestServer::TestServer(const HTTPSOptions& https_options,
                       const FilePath& document_root)
    : https_options_(https_options),
      type_(TYPE_HTTPS),
      started_(false) {
  Init(document_root);
}

TestServer::~TestServer() {
  TestRootCerts* root_certs = TestRootCerts::GetInstance();
  root_certs->Clear();
  Stop();
}

bool TestServer::Start() {
  if (type_ == TYPE_HTTPS) {
    if (!LoadTestRootCert())
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

  allowed_port_.reset(new ScopedPortException(host_port_pair_.port()));

  started_ = true;
  return true;
}

bool TestServer::Stop() {
  if (!process_handle_)
    return true;

  started_ = false;

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

  allowed_port_.reset();

  return ret;
}

const HostPortPair& TestServer::host_port_pair() const {
  DCHECK(started_);
  return host_port_pair_;
}

const DictionaryValue& TestServer::server_data() const {
  DCHECK(started_);
  return *server_data_;
}

std::string TestServer::GetScheme() const {
  switch (type_) {
    case TYPE_FTP:
      return "ftp";
    case TYPE_HTTP:
    case TYPE_SYNC:
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

GURL TestServer::GetURL(const std::string& path) const {
  return GURL(GetScheme() + "://" + host_port_pair_.ToString() +
              "/" + path);
}

GURL TestServer::GetURLWithUser(const std::string& path,
                                const std::string& user) const {
  return GURL(GetScheme() + "://" + user + "@" +
              host_port_pair_.ToString() +
              "/" + path);
}

GURL TestServer::GetURLWithUserAndPassword(const std::string& path,
                                           const std::string& user,
                                           const std::string& password) const {
  return GURL(GetScheme() + "://" + user + ":" + password +
              "@" + host_port_pair_.ToString() +
              "/" + path);
}

// static
bool TestServer::GetFilePathWithReplacements(
    const std::string& original_file_path,
    const std::vector<StringPair>& text_to_replace,
    std::string* replacement_path) {
  std::string new_file_path = original_file_path;
  bool first_query_parameter = true;
  const std::vector<StringPair>::const_iterator end = text_to_replace.end();
  for (std::vector<StringPair>::const_iterator it = text_to_replace.begin();
       it != end;
       ++it) {
    const std::string& old_text = it->first;
    const std::string& new_text = it->second;
    std::string base64_old;
    std::string base64_new;
    if (!base::Base64Encode(old_text, &base64_old))
      return false;
    if (!base::Base64Encode(new_text, &base64_new))
      return false;
    if (first_query_parameter) {
      new_file_path += "?";
      first_query_parameter = false;
    } else {
      new_file_path += "&";
    }
    new_file_path += "replace_text=";
    new_file_path += base64_old;
    new_file_path += ":";
    new_file_path += base64_new;
  }

  *replacement_path = new_file_path;
  return true;
}

void TestServer::Init(const FilePath& document_root) {
  // At this point, the port that the testserver will listen on is unknown.
  // The testserver will listen on an ephemeral port, and write the port
  // number out over a pipe that this TestServer object will read from. Once
  // that is complete, the host_port_pair_ will contain the actual port.
  host_port_pair_ = HostPortPair(GetHostname(type_, https_options_), 0);
  process_handle_ = base::kNullProcessHandle;

  FilePath src_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &src_dir);

  document_root_ = src_dir.Append(document_root);

  certificates_dir_ = src_dir.Append(FILE_PATH_LITERAL("net"))
                       .Append(FILE_PATH_LITERAL("data"))
                       .Append(FILE_PATH_LITERAL("ssl"))
                       .Append(FILE_PATH_LITERAL("certificates"));
}

bool TestServer::SetPythonPath() {
  FilePath third_party_dir;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &third_party_dir)) {
    LOG(ERROR) << "Failed to get DIR_SOURCE_ROOT";
    return false;
  }
  third_party_dir = third_party_dir.Append(FILE_PATH_LITERAL("third_party"));

  // For simplejson. (simplejson, unlike all the other python modules
  // we include, doesn't have an extra 'simplejson' directory, so we
  // need to include its parent directory, i.e. third_party_dir).
  AppendToPythonPath(third_party_dir);

  AppendToPythonPath(third_party_dir.Append(FILE_PATH_LITERAL("tlslite")));
  AppendToPythonPath(third_party_dir.Append(FILE_PATH_LITERAL("pyftpdlib")));

  // Locate the Python code generated by the protocol buffers compiler.
  FilePath pyproto_code_dir;
  if (!GetPyProtoPath(&pyproto_code_dir)) {
    LOG(ERROR) << "Failed to get python dir for generated code.";
    return false;
  }

  AppendToPythonPath(pyproto_code_dir);
  AppendToPythonPath(pyproto_code_dir.Append(FILE_PATH_LITERAL("sync_pb")));
  AppendToPythonPath(pyproto_code_dir.Append(
      FILE_PATH_LITERAL("device_management_pb")));

  return true;
}

bool TestServer::ParseServerData(const std::string& server_data) {
  VLOG(1) << "Server data: " << server_data;
  base::JSONReader json_reader;
  scoped_ptr<Value> value(json_reader.JsonToValue(server_data, true, false));
  if (!value.get() ||
      !value->IsType(Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Could not parse server data: "
               << json_reader.GetErrorMessage();
    return false;
  }
  server_data_.reset(static_cast<DictionaryValue*>(value.release()));
  int port = 0;
  if (!server_data_->GetInteger("port", &port)) {
    LOG(ERROR) << "Could not find port value";
    return false;
  }
  if ((port <= 0) || (port > kuint16max)) {
    LOG(ERROR) << "Invalid port value: " << port;
    return false;
  }
  host_port_pair_.set_port(port);
  return true;
}

FilePath TestServer::GetRootCertificatePath() const {
  return certificates_dir_.AppendASCII("root_ca_cert.crt");
}

bool TestServer::LoadTestRootCert() {
  TestRootCerts* root_certs = TestRootCerts::GetInstance();
  return root_certs->AddFromFile(GetRootCertificatePath());
}

bool TestServer::AddCommandLineArguments(CommandLine* command_line) const {
  command_line->AppendSwitchASCII("port",
                                  base::IntToString(host_port_pair_.port()));
  command_line->AppendSwitchPath("data-dir", document_root_);
  command_line->AppendSwitchPath("policy-cert-chain",
                                 certificates_dir_.AppendASCII("ok_cert.pem"));
  command_line->AppendSwitchPath("policy-cert-chain", GetRootCertificatePath());

  if (type_ == TYPE_FTP) {
    command_line->AppendArg("-f");
  } else if (type_ == TYPE_SYNC) {
    command_line->AppendArg("--sync");
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
