/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/core/platform/resource_loader.h"

#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/path.h"
#include "tools/cpp/runfiles/runfiles.h"

using bazel::tools::cpp::runfiles::Runfiles;

namespace tensorflow {

std::string GetDataDependencyFilepath(const std::string& relative_path) {
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::CreateForTest(&error));

  if (runfiles == nullptr) {
    LOG(FATAL) << "Unable to access the data dependencies of this test.\n"
                  "Make sure you are running this test using bazel.";
  }
  return runfiles->Rlocation(io::JoinPath("org_tensorflow", relative_path));
}

}  // namespace tensorflow
