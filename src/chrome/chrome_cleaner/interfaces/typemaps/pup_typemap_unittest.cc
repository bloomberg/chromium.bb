// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/chrome_cleaner/interfaces/pup.mojom.h"
#include "chrome/chrome_cleaner/interfaces/test_pup_typemap.mojom.h"
#include "chrome/chrome_cleaner/ipc/ipc_test_util.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

namespace {

using base::WaitableEvent;
using testing::UnorderedElementsAreArray;

constexpr wchar_t kWindowsCurrentVersionRegKeyName[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion";

// Special result code returned by the Mojo connection error handler on child
// processes and expected by the parent process on typemap unmarshaling error
// tests.
constexpr int32_t kConnectionBrokenResultCode = 127;

class TestPUPTypemapImpl : public mojom::TestPUPTypemap {
 public:
  explicit TestPUPTypemapImpl(mojom::TestPUPTypemapRequest request)
      : binding_(this, std::move(request)) {}

  void EchoRegKeyPath(const RegKeyPath& path,
                      EchoRegKeyPathCallback callback) override {
    std::move(callback).Run(path);
  }

  void EchoRegistryFootprint(const PUPData::RegistryFootprint& reg_footprint,
                             EchoRegistryFootprintCallback callback) override {
    std::move(callback).Run(reg_footprint);
  }

  void EchoPUP(const PUPData::PUP& pup, EchoPUPCallback callback) override {
    std::move(callback).Run(pup);
  }

 private:
  mojo::Binding<mojom::TestPUPTypemap> binding_;
};

class EchoingParentProcess : public chrome_cleaner::ParentProcess {
 public:
  explicit EchoingParentProcess(scoped_refptr<MojoTaskRunner> mojo_task_runner)
      : ParentProcess(std::move(mojo_task_runner)) {}

 protected:
  void CreateImpl(mojo::ScopedMessagePipeHandle mojo_pipe) override {
    mojom::TestPUPTypemapRequest request(std::move(mojo_pipe));
    impl_ = std::make_unique<TestPUPTypemapImpl>(std::move(request));
  }

  void DestroyImpl() override { impl_.reset(); }

 private:
  ~EchoingParentProcess() override = default;

  std::unique_ptr<TestPUPTypemapImpl> impl_;
};

class EchoingChildProcess : public chrome_cleaner::ChildProcess {
 public:
  explicit EchoingChildProcess(scoped_refptr<MojoTaskRunner> mojo_task_runner)
      : ChildProcess(mojo_task_runner),
        ptr_(std::make_unique<mojom::TestPUPTypemapPtr>()) {}

  void BindToPipe(mojo::ScopedMessagePipeHandle mojo_pipe,
                  WaitableEvent* event) {
    ptr_->Bind(
        chrome_cleaner::mojom::TestPUPTypemapPtrInfo(std::move(mojo_pipe), 0));
    // If the connection is lost while this object is processing a request or
    // waiting for a response, then it will terminate with a special result code
    // that will be expected by the parent process.
    ptr_->set_connection_error_handler(base::BindOnce(
        [](bool* processing_request) {
          if (*processing_request)
            exit(kConnectionBrokenResultCode);
        },
        &processing_request_));
    event->Signal();
  }

  RegKeyPath EchoRegKeyPath(const RegKeyPath& input) {
    return Echo(input, base::BindOnce(&EchoingChildProcess::RunEchoRegKeyPath,
                                      base::Unretained(this)));
  }

  PUPData::RegistryFootprint EchoRegistryFootprint(
      const PUPData::RegistryFootprint& input) {
    return Echo(input,
                base::BindOnce(&EchoingChildProcess::RunEchoRegistryFootprint,
                               base::Unretained(this)));
  }

  PUPData::PUP EchoPUP(const PUPData::PUP& input) {
    return Echo(input, base::BindOnce(&EchoingChildProcess::RunEchoPUP,
                                      base::Unretained(this)));
  }

 private:
  ~EchoingChildProcess() override {
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<mojom::TestPUPTypemapPtr> ptr) { ptr.reset(); },
            base::Passed(&ptr_)));
  }

  void RunEchoRegKeyPath(
      const RegKeyPath& input,
      mojom::TestPUPTypemap::EchoRegKeyPathCallback callback) {
    (*ptr_)->EchoRegKeyPath(input, std::move(callback));
  }

  void RunEchoRegistryFootprint(
      const PUPData::RegistryFootprint& input,
      mojom::TestPUPTypemap::EchoRegistryFootprintCallback callback) {
    (*ptr_)->EchoRegistryFootprint(input, std::move(callback));
  }

  void RunEchoPUP(const PUPData::PUP& input,
                  mojom::TestPUPTypemap::EchoPUPCallback callback) {
    (*ptr_)->EchoPUP(input, std::move(callback));
  }

  template <typename EchoedValue>
  EchoedValue Echo(
      const EchoedValue& input,
      base::OnceCallback<void(const EchoedValue&,
                              base::OnceCallback<void(const EchoedValue&)>)>
          echoing_function) {
    DCHECK(!processing_request_);
    processing_request_ = true;

    EchoedValue output;
    WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                        WaitableEvent::InitialState::NOT_SIGNALED);
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(echoing_function), input,
                       base::BindOnce(
                           [](EchoedValue* output_holder, WaitableEvent* event,
                              const EchoedValue& output) {
                             *output_holder = output;
                             event->Signal();
                           },
                           &output, &event)));
    event.Wait();
    processing_request_ = false;

    return output;
  }

  std::unique_ptr<mojom::TestPUPTypemapPtr> ptr_;
  // All requests are synchronous and happen on the same sequence, so no
  // synchronization is needed.
  bool processing_request_ = false;
};

scoped_refptr<EchoingChildProcess> InitChildProcess() {
  auto mojo_task_runner = MojoTaskRunner::Create();
  auto child_process =
      base::MakeRefCounted<EchoingChildProcess>(mojo_task_runner);
  auto message_pipe_handle = child_process->CreateMessagePipeFromCommandLine();

  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  mojo_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&EchoingChildProcess::BindToPipe, child_process,
                                base::Passed(&message_pipe_handle), &event));
  event.Wait();

  return child_process;
}

MULTIPROCESS_TEST_MAIN(EchoRegKeyPathMain) {
  scoped_refptr<EchoingChildProcess> child_process = InitChildProcess();

  RegKeyPath version_key(HKEY_LOCAL_MACHINE, kWindowsCurrentVersionRegKeyName,
                         KEY_WOW64_32KEY);
  EXPECT_EQ(version_key, child_process->EchoRegKeyPath(version_key));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(EchoRegistryFootprint) {
  scoped_refptr<EchoingChildProcess> child_process = InitChildProcess();

  RegKeyPath version_key(HKEY_LOCAL_MACHINE, kWindowsCurrentVersionRegKeyName,
                         KEY_WOW64_32KEY);

  PUPData::RegistryFootprint fp1(version_key, L"value_name", L"value_substring",
                                 REGISTRY_VALUE_MATCH_EXACT);
  EXPECT_EQ(fp1, child_process->EchoRegistryFootprint(fp1));

  PUPData::RegistryFootprint fp2(version_key, L"value_name", L"",
                                 REGISTRY_VALUE_MATCH_EXACT);
  EXPECT_EQ(fp2, child_process->EchoRegistryFootprint(fp2));

  PUPData::RegistryFootprint fp3(version_key, L"", L"",
                                 REGISTRY_VALUE_MATCH_EXACT);
  EXPECT_EQ(fp3, child_process->EchoRegistryFootprint(fp3));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(EchoRegistryFootprintFailure) {
  scoped_refptr<EchoingChildProcess> child_process = InitChildProcess();

  RegKeyPath version_key(HKEY_LOCAL_MACHINE, kWindowsCurrentVersionRegKeyName,
                         KEY_WOW64_32KEY);

  // Creates a RegistryFootprint with a rule that doesn't correspond to a valid
  // RegistryMatchRule enum value. This will trigger a deserialization error on
  // the broker process that will cause the pipe to be closed. As a consequence,
  // the connection error handler, defined in BindToPipe(), will terminate the
  // child process with a special exit code that will be expected to be received
  // by the parent process.
  PUPData::RegistryFootprint fp_invalid(version_key, L"value_name",
                                        L"value_substring",
                                        static_cast<RegistryMatchRule>(-1));
  PUPData::RegistryFootprint unused =
      child_process->EchoRegistryFootprint(fp_invalid);

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(EchoPUP) {
  scoped_refptr<EchoingChildProcess> child_process = InitChildProcess();

  PUPData::PUP pup;

  pup.AddDiskFootprint(base::FilePath(L"C:\\Program Files\\File1.exe"));
  pup.AddDiskFootprint(base::FilePath(L"C:\\Program Files\\File2.exe"));
  pup.AddDiskFootprint(base::FilePath(L"C:\\Program Files\\File3.exe"));

  RegKeyPath version_key(HKEY_LOCAL_MACHINE, kWindowsCurrentVersionRegKeyName,
                         KEY_WOW64_32KEY);
  pup.expanded_registry_footprints.emplace_back(version_key, L"value_name",
                                                L"value_substring",
                                                REGISTRY_VALUE_MATCH_EXACT);
  pup.expanded_registry_footprints.emplace_back(version_key, L"", L"",
                                                REGISTRY_VALUE_MATCH_EXACT);

  pup.expanded_scheduled_tasks.push_back(L"Scheduled task 1");
  pup.expanded_scheduled_tasks.push_back(L"Scheduled task 2");
  pup.expanded_scheduled_tasks.push_back(L"Scheduled task 3");

  pup.disk_footprints_info.Insert(base::FilePath(L"C:\\File1.exe"),
                                  PUPData::FileInfo({UwS::FOUND_IN_MEMORY}));
  pup.disk_footprints_info.Insert(
      base::FilePath(L"C:\\File2.exe"),
      PUPData::FileInfo({UwS::FOUND_IN_SHELL, UwS::FOUND_IN_CLSID}));

  const PUPData::PUP echoed = child_process->EchoPUP(pup);
  // Not using operator== because error messages only show the sequences of
  // bytes for each object, which makes it very hard to identify the differences
  // between both objects.
  EXPECT_THAT(
      echoed.expanded_disk_footprints.file_paths(),
      UnorderedElementsAreArray(pup.expanded_disk_footprints.file_paths()));
  EXPECT_THAT(echoed.expanded_registry_footprints,
              UnorderedElementsAreArray(pup.expanded_registry_footprints));
  EXPECT_THAT(echoed.expanded_scheduled_tasks,
              UnorderedElementsAreArray(pup.expanded_scheduled_tasks));
  EXPECT_EQ(pup.disk_footprints_info.map(), echoed.disk_footprints_info.map());

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(EchoPUPFailure_InvalidRegistryMatchRule) {
  scoped_refptr<EchoingChildProcess> child_process = InitChildProcess();

  // Same approach as used in EchoRegistryFootprintFailure, creates a registry
  // footprint in the pup data with a rule that doesn't correspond to a valid
  // RegistryMatchRule enum value. This will trigger a deserialization error on
  // the broker process that will cause the pipe to be closed. As a consequence,
  // the connection error handler, defined in BindToPipe(), will terminate the
  // child process with a special exit code that will be expected to be received
  // by the parent process.
  PUPData::PUP pup;
  RegKeyPath version_key(HKEY_LOCAL_MACHINE, kWindowsCurrentVersionRegKeyName,
                         KEY_WOW64_32KEY);
  pup.expanded_registry_footprints.emplace_back(
      version_key, L"value_name", L"value_substring",
      static_cast<RegistryMatchRule>(-1));

  const PUPData::PUP unused = child_process->EchoPUP(pup);

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(EchoPUPFailure_InvalidTraceLocation) {
  scoped_refptr<EchoingChildProcess> child_process = InitChildProcess();

  // Same approach as used in EchoPUPFailure_InvalidRegistryMatchRule, this time
  // passing an invalid trace location.
  PUPData::PUP pup;
  pup.disk_footprints_info.Insert(
      base::FilePath(L"C:\\File1.exe"),
      PUPData::FileInfo({UwS::FOUND_IN_UNINSTALLSTRING}));
  pup.disk_footprints_info.Insert(
      base::FilePath(L"C:\\File2.exe"),
      PUPData::FileInfo({static_cast<UwS::TraceLocation>(-1)}));

  const PUPData::PUP unused = child_process->EchoPUP(pup);

  return ::testing::Test::HasNonfatalFailure();
}

// PUPTypemapTest is parametrized with:
//  - expected_to_succeed_: whether the child process is expected to succeed or
//    to fail;
//  - child_main_function_: the name of the MULTIPROCESS_TEST_MAIN function for
//    the child process.
typedef std::tuple<bool, const char*> PUPTypemapTestParams;

class PUPTypemapTest : public ::testing::TestWithParam<PUPTypemapTestParams> {
 public:
  void SetUp() override {
    std::tie(expected_to_succeed_, child_main_function_) = GetParam();

    mojo_task_runner_ = MojoTaskRunner::Create();
    parent_process_ =
        base::MakeRefCounted<EchoingParentProcess>(mojo_task_runner_);
  }

 protected:
  const char* child_main_function_;
  bool expected_to_succeed_;

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  scoped_refptr<EchoingParentProcess> parent_process_;
};

TEST_P(PUPTypemapTest, Echo) {
  int32_t exit_code = -1;
  EXPECT_TRUE(parent_process_->LaunchConnectedChildProcess(child_main_function_,
                                                           &exit_code));
  EXPECT_EQ(expected_to_succeed_ ? 0 : kConnectionBrokenResultCode, exit_code);
}

INSTANTIATE_TEST_CASE_P(
    Success,
    PUPTypemapTest,
    testing::Combine(testing::Values(true),
                     testing::Values("EchoRegKeyPathMain",
                                     "EchoRegistryFootprint",
                                     "EchoPUP")),
    GetParamNameForTest());

INSTANTIATE_TEST_CASE_P(
    Failure,
    PUPTypemapTest,
    testing::Combine(testing::Values(false),
                     testing::Values("EchoRegistryFootprintFailure",
                                     "EchoPUPFailure_InvalidRegistryMatchRule",
                                     "EchoPUPFailure_InvalidTraceLocation")),
    GetParamNameForTest());

}  // namespace

}  // namespace chrome_cleaner
