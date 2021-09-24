/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fstream>
#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "flatbuffers/flatbuffers.h"  // from @flatbuffers
#include "tensorflow/lite/experimental/acceleration/compatibility/android_info.h"
#include "tensorflow/lite/experimental/acceleration/configuration/configuration_generated.h"
#include "tensorflow/lite/experimental/acceleration/mini_benchmark/big_little_affinity.h"
#include "tensorflow/lite/experimental/acceleration/mini_benchmark/validator.h"

extern const unsigned char TENSORFLOW_ACCELERATION_MODEL_DATA_VARIABLE[];
extern const int TENSORFLOW_ACCELERATION_MODEL_LENGTH_VARIABLE;

namespace tflite {
namespace acceleration {
namespace {

class LocalizerValidationRegressionTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    int error = -1;
#if defined(__ANDROID__)
    AndroidInfo android_info;
    auto status = RequestAndroidInfo(&android_info);
    ASSERT_TRUE(status.ok());
    if (android_info.is_emulator) {
      std::cerr << "Running on an emulator, skipping processor affinity\n";
    } else {
      std::cerr << "Running on hardware, setting processor affinity\n";
      BigLittleAffinity affinity = GetAffinity();
      cpu_set_t set;
      CPU_ZERO(&set);
      for (int i = 0; i < 16; i++) {
        if (affinity.big_core_affinity & (0x1 << i)) {
          CPU_SET(i, &set);
        }
      }
      error = sched_setaffinity(getpid(), sizeof(set), &set);
      if (error == -1) {
        perror("sched_setaffinity failed");
      }
    }
#endif
    std::string dir = GetTestTmpDir();
    error = mkdir(dir.c_str(), 0777);
    if (error == -1) {
      if (errno != EEXIST) {
        perror("mkdir failed");
        ASSERT_TRUE(false);
      }
    }

    std::string path = ModelPath();
    (void)unlink(path.c_str());
    std::string contents(reinterpret_cast<const char*>(
                             TENSORFLOW_ACCELERATION_MODEL_DATA_VARIABLE),
                         TENSORFLOW_ACCELERATION_MODEL_LENGTH_VARIABLE);
    std::ofstream f(path, std::ios::binary);
    ASSERT_TRUE(f.is_open());
    f << contents;
    f.close();
    ASSERT_EQ(chmod(path.c_str(), 0500), 0);
  }
  static std::string ModelPath() { return GetTestTmpDir() + "/model.tflite"; }
  static std::string GetTestTmpDir() {
    const char* from_env = getenv("TEST_TMPDIR");
    if (from_env) {
      return from_env;
    }
#ifdef __ANDROID__
    return "/data/local/tmp";
#else
    return "/tmp";
#endif
  }
  void CheckValidation() {
    std::string path = ModelPath();
    const ComputeSettings* settings =
        flatbuffers::GetRoot<ComputeSettings>(fbb_.GetBufferPointer());
    int fd = open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    struct stat stat_buf = {0};
    ASSERT_EQ(fstat(fd, &stat_buf), 0);
    auto validator =
        std::make_unique<Validator>(fd, 0, stat_buf.st_size, settings);
    close(fd);

    Validator::Results results;
    EXPECT_EQ(validator->RunValidation(&results), kMinibenchmarkSuccess);
    EXPECT_TRUE(results.ok);
    EXPECT_EQ(results.delegate_error, 0);

    for (const auto& metric : results.metrics) {
      std::cerr << "Metric " << metric.first;
      for (float v : metric.second) {
        std::cerr << " " << v;
      }
      std::cerr << "\n";
    }
    std::cerr << "Compilation time us " << results.compilation_time_us
              << std::endl;
    std::cerr << "Execution time us";
    for (int64_t t : results.execution_time_us) {
      std::cerr << " " << t;
    }
    std::cerr << std::endl;
  }
  flatbuffers::FlatBufferBuilder fbb_;
};

TEST_F(LocalizerValidationRegressionTest, Cpu) {
  fbb_.Finish(CreateComputeSettings(fbb_, ExecutionPreference_ANY,
                                    CreateTFLiteSettings(fbb_)));
  CheckValidation();
}

TEST_F(LocalizerValidationRegressionTest, Nnapi) {
  fbb_.Finish(
      CreateComputeSettings(fbb_, ExecutionPreference_ANY,
                            CreateTFLiteSettings(fbb_, Delegate_NNAPI)));
  AndroidInfo android_info;
  auto status = RequestAndroidInfo(&android_info);
  ASSERT_TRUE(status.ok());
  if (android_info.android_sdk_version >= "28") {
    CheckValidation();
  }
}

TEST_F(LocalizerValidationRegressionTest, Gpu) {
  AndroidInfo android_info;
  auto status = RequestAndroidInfo(&android_info);
  ASSERT_TRUE(status.ok());
  if (android_info.is_emulator) {
    std::cerr << "Skipping GPU on emulator\n";
    return;
  }
  fbb_.Finish(CreateComputeSettings(fbb_, ExecutionPreference_ANY,
                                    CreateTFLiteSettings(fbb_, Delegate_GPU)));
#ifdef __ANDROID__
  CheckValidation();
#endif  // __ANDROID__
}

}  // namespace
}  // namespace acceleration
}  // namespace tflite
