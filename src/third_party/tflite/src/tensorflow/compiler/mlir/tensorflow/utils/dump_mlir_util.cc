/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/mlir/tensorflow/utils/dump_mlir_util.h"

#include <cstdint>
#include <cstring>
#include <string>

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"
#include "mlir/IR/Operation.h"  // from @llvm-project
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/path.h"

namespace tensorflow {

namespace {
struct NameCounts {
  mutex counts_mutex;
  llvm::StringMap<int64_t> counts;
};

std::string MakeUniqueFilename(string name) {
  static NameCounts& instance = *new NameCounts;

  // Remove illegal characters from `name`.
  for (int i = 0, e = name.size(); i < e; ++i) {
    char ch = name[i];
    if (ch == '/' || ch == '[' || ch == ']' || ch == '*' || ch == '?' ||
        ch == '\\') {
      name[i] = '_';
    }
  }

  int count;
  {
    mutex_lock lock(instance.counts_mutex);
    count = instance.counts[name]++;
  }

  std::string filename = name;
  if (count > 0) {
    filename = llvm::formatv("{0}_{1}", filename, count).str();
  }
  filename = llvm::Twine(filename).concat(".mlir").str();
  return filename;
}

// Simple raw_ostream that prints to stderr.
struct LogInfoRawStream : public llvm::raw_ostream {
  LogInfoRawStream() { SetUnbuffered(); }
  ~LogInfoRawStream() override = default;
  uint64_t current_pos() const override { return 0; }

  void write_impl(const char* ptr, size_t size) override {
    fprintf(stderr, "%.*s", static_cast<int>(size), ptr);
  }
};

// Simple raw_ostream that prints to a file.
struct WritableFileRawStream : public llvm::raw_ostream {
  explicit WritableFileRawStream(std::unique_ptr<WritableFile> file)
      : file(std::move(file)) {
    SetUnbuffered();
  }
  ~WritableFileRawStream() override = default;
  uint64_t current_pos() const override { return 0; }

  void write_impl(const char* ptr, size_t size) override {
    // Write the file if it is still valid. If the write fails, null out the
    // file to avoid encountering another error.
    if (file && !file->Append(StringPiece(ptr, size)).ok()) {
      file = nullptr;
    }
  }

  // The file being written to.
  std::unique_ptr<WritableFile> file;
};
}  // namespace

Status CreateFileForDumping(llvm::StringRef name,
                            std::unique_ptr<llvm::raw_ostream>* os,
                            std::string* filepath, llvm::StringRef dirname) {
  std::string dir;
  if (!dirname.empty())
    dir = std::string(dirname);
  else
    dir = GetDumpDirFromEnvVar();

  if (dir.empty()) {
    return Status(error::Code::INVALID_ARGUMENT,
                  "(TF_DUMP_GRAPH_PREFIX not specified)");
  }

  if (dir == "-") {
    *os = std::make_unique<LogInfoRawStream>();
    *filepath = "(stderr)";
    return Status();
  }

  // Get a valid file path to dump with.
  Env* env = Env::Default();
  Status status = env->RecursivelyCreateDir(dir);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to create '" << dir
                 << "' directory for dumping: " << status;
    return Status(error::Code::UNAVAILABLE, "(unavailable)");
  }
  *filepath = io::JoinPath(dir, MakeUniqueFilename(std::string(name)));

  // Try to open the file and generate a raw_ostream.
  std::unique_ptr<WritableFile> file;
  status = env->NewWritableFile(*filepath, &file);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to create file '" << filepath << "': " << status;
    return Status(error::Code::UNAVAILABLE, "(unavailable)");
  }
  *os = std::make_unique<WritableFileRawStream>(std::move(file));
  return Status();
}

std::string DumpMlirOpToFile(llvm::StringRef name, mlir::Operation* op,
                             llvm::StringRef dirname) {
  std::unique_ptr<llvm::raw_ostream> os;
  std::string filepath;
  Status result = CreateFileForDumping(name, &os, &filepath, dirname);
  if (!result.ok()) return result.error_message();

  op->print(*os, mlir::OpPrintingFlags().useLocalScope().printGenericOpForm());
  LOG(INFO) << "Dumped MLIR operation '" << op->getName().getStringRef().str()
            << "' to '" << filepath << "'";
  return filepath;
}

std::string GetDumpDirFromEnvVar() {
  const char* prefix_env = getenv("TF_DUMP_GRAPH_PREFIX");
  if (!prefix_env) {
    LOG(WARNING)
        << "Failed to dump MLIR module because dump location is not "
        << " specified through TF_DUMP_GRAPH_PREFIX environment variable.";
    return "";
  }

  std::string result = prefix_env;

  if (absl::EqualsIgnoreCase(result, "sponge") &&
      !io::GetTestUndeclaredOutputsDir(&result)) {
    LOG(WARNING) << "TF_DUMP_GRAPH_PREFIX=sponge but "
                    "TEST_UNDECLARED_OUTPUT_DIRS is not set";
    return "";
  }
  return result;
}

std::string DumpRawStringToFile(llvm::StringRef name, llvm::StringRef content,
                                llvm::StringRef dirname) {
  std::unique_ptr<llvm::raw_ostream> os;
  std::string filepath;
  Status result = CreateFileForDumping(name, &os, &filepath, dirname);
  if (!result.ok()) return result.error_message();

  (*os) << content;
  LOG(INFO) << "Outputted requested string to '" << filepath << "'";
  return filepath;
}

}  // namespace tensorflow
