// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <set>
#include <string>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "crypto/openssl_util.h"
#include "crypto/sha2.h"
#include "net/tools/root_store_tool/root_store.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/bio.h"
#include "third_party/boringssl/src/include/openssl/pem.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"

using chrome_root_store::RootStore;

namespace {

absl::optional<std::string> DecodePEM(base::StringPiece pem) {
  // TODO(https://crbug.com/1216547): net/cert/pem.h has a much nicer API, but
  // it would require some build refactoring to avoid a circular dependency.
  // This is assuming that the chrome trust store code goes in
  // net/cert/internal, which it may not.
  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(pem.data(), pem.size()));
  if (!bio) {
    return absl::nullopt;
  }
  char* name;
  char* header;
  unsigned char* data;
  long len;
  if (!PEM_read_bio(bio.get(), &name, &header, &data, &len)) {
    LOG(ERROR) << "Could not find PEM block.";
    return absl::nullopt;
  }
  bssl::UniquePtr<char> scoped_name(name);
  bssl::UniquePtr<char> scoped_header(header);
  bssl::UniquePtr<unsigned char> scoped_data(data);
  if (strcmp(name, "CERTIFICATE") != 0) {
    LOG(ERROR) << "Found PEM block of type " << name
               << " instead of CERTIFICATE";
    return absl::nullopt;
  }
  return std::string(data, data + len);
}

absl::optional<RootStore> ReadTextRootStore(
    const base::FilePath& root_store_dir,
    std::set<base::FilePath>* deps) {
  base::FilePath root_store_path =
      root_store_dir.AppendASCII("root_store.textproto");
  std::string root_store_text;
  if (!base::ReadFileToString(base::MakeAbsoluteFilePath(root_store_path),
                              &root_store_text)) {
    LOG(ERROR) << "Could not read " << root_store_path;
    return absl::nullopt;
  }
  deps->insert(root_store_path);

  RootStore root_store;
  if (!google::protobuf::TextFormat::ParseFromString(root_store_text,
                                                     &root_store)) {
    LOG(ERROR) << "Could not parse " << root_store_path;
    return absl::nullopt;
  }

  // Replace the filenames with the actual certificate contents.
  base::FilePath certs_dir = root_store_dir.AppendASCII("certs");
  for (auto& anchor : *root_store.mutable_trust_anchors()) {
    base::FilePath pem_path = certs_dir.AppendASCII(anchor.filename());

    if (!base::PathExists(pem_path)) {
      LOG(ERROR) << "Error file does not exist: " << pem_path;
      return absl::nullopt;
    }

    if (base::DirectoryExists(pem_path)) {
      LOG(ERROR) << "Error path is a directory: " << pem_path;
      return absl::nullopt;
    }

    if (!base::PathIsReadable(pem_path)) {
      LOG(ERROR) << "Error file is not readable: " << pem_path;
      return absl::nullopt;
    }

    std::string pem;
    if (!base::ReadFileToString(base::MakeAbsoluteFilePath(pem_path), &pem)) {
      LOG(ERROR) << "Error reading " << pem_path;
      return absl::nullopt;
    }
    absl::optional<std::string> der = DecodePEM(pem);
    if (!der) {
      LOG(ERROR) << "Error decoding " << pem_path;
      return absl::nullopt;
    }
    anchor.clear_filename();
    anchor.set_der(*der);
    deps->insert(pem_path);
  }
  return std::move(root_store);
}

void AppendMakefilePath(std::string* out, const base::FilePath& path) {
#if BUILDFLAG(IS_WIN)
  std::string path_str = base::WideToUTF8(path.value());
#else
  std::string path_str = path.value();
#endif
  // Escaping for Makefiles is complex. See `DepfileParser::Parse` in Ninja,
  // which ultimately consumes this output. We output relative paths and thus
  // can assume no escaping is needed, even if the build directory would need to
  // be escaped. This matches, e.g., `GenerateDepfile` in //tools/grit, which
  // similarly does not escape output.
  out->append(path_str);
}

// Returns true if file was correctly written, false otherwise.
bool WriteRootCppFile(const RootStore& root_store,
                      const base::FilePath cpp_path) {
  // Root store should have at least one trust anchors.
  CHECK_GT(root_store.trust_anchors_size(), 0);

  std::string string_to_write =
      "// This file is auto-generated, DO NOT EDIT.\n\n"
      "const ChromeRootCertInfo kChromeRootCertList[] = {\n";

  for (auto& anchor : root_store.trust_anchors()) {
    // Every trust anchor at this point should have a DER.
    CHECK(!anchor.der().empty());
    std::string der = anchor.der();

    // Begin struct. Assumed type of ChromeRootCertInfo:
    //
    // struct {
    //   base::span<const uint8_t> der;
    // };
    string_to_write += "    {{{";

    // Convert each character to hex representation, escaped.
    for (auto c : der) {
      base::StringAppendF(&string_to_write, "0x%02xu,",
                          static_cast<uint8_t>(c));
    }

    // End struct
    string_to_write += "}}},\n";
  }
  string_to_write += "};";
  if (!base::WriteFile(cpp_path, string_to_write)) {
    return false;
  }
  return true;
}

// Returns true if file was correctly written, false otherwise.
bool WriteEvCppFile(const RootStore& root_store,
                    const base::FilePath cpp_path) {
  // There should be at least one EV root.
  CHECK_GT(root_store.trust_anchors_size(), 0);

  std::string string_to_write =
      "// This file is auto-generated, DO NOT EDIT.\n\n"
      "static const EVMetadata kEvRootCaMetadata[] = {\n";

  for (auto& anchor : root_store.trust_anchors()) {
    // Every trust anchor at this point should have a DER.
    CHECK(!anchor.der().empty());

    std::string sha256_hash = crypto::SHA256HashString(anchor.der());

    // Begin struct. Assumed type of EVMetadata:
    //
    // struct EVMetadata {
    //  static const size_t kMaxOIDsPerCA = 2;
    //  SHA256HashValue fingerprint;
    //  const base::StringPiece policy_oids[kMaxOIDsPerCA];
    // };
    string_to_write += "    {\n";
    string_to_write += "        {{";

    int wrap_count = 0;
    for (auto c : sha256_hash) {
      if (wrap_count != 0) {
        if (wrap_count % 11 == 0) {
          string_to_write += ",\n          ";
        } else {
          string_to_write += ", ";
        }
      }
      base::StringAppendF(&string_to_write, "0x%02x", static_cast<uint8_t>(c));
      wrap_count++;
    }

    string_to_write += "}},\n";
    string_to_write += "        {\n";

    // struct expects exactly two policy oids, and we can only support 1 or 2
    // policy OIDs. These checks will need to change if we ever merge the EV and
    // Chrome Root Store textprotos.
    const int kMaxPolicyOids = 2;
    int oids_size = anchor.ev_policy_oids_size();
    std::string hexencode_hash =
        base::HexEncode(sha256_hash.data(), sha256_hash.size());
    if (oids_size > kMaxPolicyOids) {
      PLOG(ERROR) << hexencode_hash << " has too many OIDs!";
      return false;
    } else if (oids_size < 1) {
      PLOG(ERROR) << hexencode_hash << " has no OIDs!";
      return false;
    }
    for (int i = 0; i < kMaxPolicyOids; i++) {
      std::string oid;
      if (i < oids_size) {
        oid = anchor.ev_policy_oids(i);
      }
      string_to_write += "            \"" + oid + "\",\n";
    }

    // End struct
    string_to_write += "        },\n";
    string_to_write += "    },\n";
  }
  string_to_write += "};\n";
  if (!base::WriteFile(cpp_path, string_to_write)) {
    PLOG(ERROR) << "Error writing cpp include file";
    return false;
  }
  return true;
}

bool ValidCppOutputFormatValue(base::StringPiece value) {
  return (value == "root" || value == "ev");
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  base::CommandLine::Init(argc, argv);

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  crypto::EnsureOpenSSLInit();

  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  base::FilePath deps_path = command_line.GetSwitchValuePath("write-deps");
  base::FilePath proto_path = command_line.GetSwitchValuePath("write-proto");
  base::FilePath cpp_path = command_line.GetSwitchValuePath("write-cpp");
  std::string cpp_output_format =
      command_line.GetSwitchValueASCII("cpp-output-format");
  // Get root store directory. Assumptions:
  //  - $(ROOT_STORE_DIR)/root_store.textproto contains the textproto definition
  //    of the root store
  //
  //  - Any certificate files referenced in
  //    $(ROOT_STORE_DIR)/root_store.textproto exist in the
  //    $(ROOT_STORE_DIR)/certs/ subdirectory.
  base::FilePath root_store_dir =
      command_line.GetSwitchValuePath("root-store-dir");

  if ((proto_path.empty() && cpp_path.empty()) || root_store_dir.empty() ||
      command_line.HasSwitch("help") ||
      (!cpp_path.empty() && !ValidCppOutputFormatValue(cpp_output_format))) {
    std::cerr << cpp_output_format << " ";
    std::cerr << "Usage: root_store_tool "
              << "--root-store-dir=<path> "
              << "[--write-deps=DEP_FILE] "
              << "[--write-proto=PROTO_FILE] "
              << "[--write-cpp=CPP_FILE --cpp-output-format=[ev|root]] "
              << std::endl;
    return 1;
  }

  std::set<base::FilePath> deps;
  absl::optional<RootStore> root_store =
      ReadTextRootStore(root_store_dir, &deps);
  if (!root_store) {
    return 1;
  }

  // TODO(https://crbug.com/1216547): Figure out how to use the serialized
  // proto to support component update.
  // components/resources/ssl/ssl_error_assistant/push_proto.py
  // does it through a GCS bucket (I think) so that might be an option.
  if (!proto_path.empty()) {
    std::string serialized;
    if (!root_store->SerializeToString(&serialized)) {
      LOG(ERROR) << "Error serializing root store proto"
                 << root_store->DebugString();
      return 1;
    }
    if (!base::WriteFile(proto_path, serialized)) {
      PLOG(ERROR) << "Error writing serialized proto root store";
      return 1;
    }
  }

  if (!cpp_path.empty()) {
    bool success;
    if (cpp_output_format == "root") {
      success = WriteRootCppFile(*root_store, cpp_path);
    } else if (cpp_output_format == "ev") {
      success = WriteEvCppFile(*root_store, cpp_path);
    } else {
      // Unknown format.
      success = false;
    }
    if (!success) {
      PLOG(ERROR) << "Error writing cpp include file";
      return 1;
    }
  }

  if (!deps_path.empty()) {
    // The output depends directly on the input proto and indirectly on all the
    // files it references. Emit a depfile for Ninja to consume. This ensures
    // dependencies are correct without needing to list every file separately
    // in the textproto and build files.
    //
    // See https://gn.googlesource.com/gn/+/main/docs/reference.md#var_depfile
    // and `DepfileParser::Parse` in Ninja, which ultimately consumes this file.
    std::string string_to_write;
    for (const auto& output : {cpp_path, proto_path}) {
      if (!output.empty()) {
        AppendMakefilePath(&string_to_write, output);
        string_to_write.push_back(':');
        for (const base::FilePath& dep : deps) {
          string_to_write.push_back(' ');
          AppendMakefilePath(&string_to_write, dep);
        }
        string_to_write.push_back('\n');
      }
    }
    if (!base::WriteFile(deps_path, string_to_write)) {
      PLOG(ERROR) << "Error writing deps include file";
      return 1;
    }
  }

  return 0;
}
