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
#include "tensorflow/lite/experimental/acceleration/configuration/flatbuffer_to_proto.h"

#include <memory>
#include <string>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "tensorflow/lite/experimental/acceleration/configuration/configuration.pb.h"
#include "tensorflow/lite/experimental/acceleration/configuration/configuration_generated.h"

namespace tflite {
namespace acceleration {
namespace {

class ConversionTest : public ::testing::Test {
 protected:
  void CheckDelegateEnum(Delegate input, proto::Delegate output) {
    settings_.tflite_settings.reset(new TFLiteSettingsT());
    settings_.tflite_settings->delegate = input;
    const proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
    EXPECT_EQ(output, compute.tflite_settings().delegate());
  }
  void CheckExecutionPreference(ExecutionPreference input,
                                proto::ExecutionPreference output) {
    settings_.preference = input;
    const proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
    EXPECT_EQ(output, compute.preference());
  }
  void CheckNNAPIExecutionPreference(NNAPIExecutionPreference input,
                                     proto::NNAPIExecutionPreference output) {
    settings_.tflite_settings.reset(new TFLiteSettingsT());
    settings_.tflite_settings->nnapi_settings.reset(new NNAPISettingsT());
    settings_.tflite_settings->nnapi_settings->execution_preference = input;
    const proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
    EXPECT_EQ(
        output,
        compute.tflite_settings().nnapi_settings().execution_preference());
  }
  void CheckNNAPIExecutionPriority(NNAPIExecutionPriority input,
                                   proto::NNAPIExecutionPriority output) {
    settings_.tflite_settings.reset(new TFLiteSettingsT());
    settings_.tflite_settings->nnapi_settings.reset(new NNAPISettingsT());
    settings_.tflite_settings->nnapi_settings->execution_priority = input;
    const proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
    EXPECT_EQ(output,
              compute.tflite_settings().nnapi_settings().execution_priority());
  }
  void CheckGPUBackend(GPUBackend input, proto::GPUBackend output) {
    settings_.tflite_settings.reset(new TFLiteSettingsT());
    settings_.tflite_settings->gpu_settings.reset(new GPUSettingsT());
    settings_.tflite_settings->gpu_settings->force_backend = input;
    const proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
    EXPECT_EQ(output, compute.tflite_settings().gpu_settings().force_backend());
  }

  ComputeSettingsT settings_;
  MiniBenchmarkEventT event_;
};

TEST_F(ConversionTest, Delegate) {
  CheckDelegateEnum(Delegate_NONE, proto::Delegate::NONE);
  CheckDelegateEnum(Delegate_NNAPI, proto::Delegate::NNAPI);
  CheckDelegateEnum(Delegate_GPU, proto::Delegate::GPU);
  CheckDelegateEnum(Delegate_HEXAGON, proto::Delegate::HEXAGON);
  CheckDelegateEnum(Delegate_EDGETPU, proto::Delegate::EDGETPU);
  CheckDelegateEnum(Delegate_EDGETPU_CORAL, proto::Delegate::EDGETPU_CORAL);
  CheckDelegateEnum(Delegate_XNNPACK, proto::Delegate::XNNPACK);
}

TEST_F(ConversionTest, ExecutionPreference) {
  CheckExecutionPreference(ExecutionPreference_ANY,
                           proto::ExecutionPreference::ANY);
  CheckExecutionPreference(ExecutionPreference_LOW_LATENCY,
                           proto::ExecutionPreference::LOW_LATENCY);
  CheckExecutionPreference(ExecutionPreference_LOW_POWER,
                           proto::ExecutionPreference::LOW_POWER);
  CheckExecutionPreference(ExecutionPreference_FORCE_CPU,
                           proto::ExecutionPreference::FORCE_CPU);
}

TEST_F(ConversionTest, ModelIdentifier) {
  settings_.model_identifier_for_statistics = "id";
  settings_.model_namespace_for_statistics = "ns";
  const proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
  EXPECT_EQ(compute.model_namespace_for_statistics(), "ns");
  EXPECT_EQ(compute.model_identifier_for_statistics(), "id");
}

TEST_F(ConversionTest, NNAPISettings) {
  settings_.tflite_settings.reset(new TFLiteSettingsT());
  settings_.tflite_settings->nnapi_settings.reset(new NNAPISettingsT());
  NNAPISettingsT* input_settings =
      settings_.tflite_settings->nnapi_settings.get();
  input_settings->accelerator_name = "a";
  input_settings->cache_directory = "d";
  input_settings->model_token = "t";
  input_settings->allow_fp16_precision_for_fp32 = true;

  proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
  proto::NNAPISettings output_settings =
      compute.tflite_settings().nnapi_settings();
  EXPECT_EQ(output_settings.accelerator_name(), "a");
  EXPECT_EQ(output_settings.cache_directory(), "d");
  EXPECT_EQ(output_settings.model_token(), "t");
  EXPECT_TRUE(output_settings.allow_fp16_precision_for_fp32());
  EXPECT_FALSE(output_settings.allow_nnapi_cpu_on_android_10_plus());
  EXPECT_FALSE(output_settings.fallback_settings()
                   .allow_automatic_fallback_on_compilation_error());
  EXPECT_FALSE(output_settings.fallback_settings()
                   .allow_automatic_fallback_on_execution_error());

  input_settings->fallback_settings.reset(new FallbackSettingsT());
  input_settings->fallback_settings
      ->allow_automatic_fallback_on_compilation_error = true;
  compute = ConvertFromFlatbuffer(settings_);
  output_settings = compute.tflite_settings().nnapi_settings();
  EXPECT_TRUE(output_settings.fallback_settings()
                  .allow_automatic_fallback_on_compilation_error());
  EXPECT_FALSE(output_settings.fallback_settings()
                   .allow_automatic_fallback_on_execution_error());

  input_settings->fallback_settings
      ->allow_automatic_fallback_on_compilation_error = false;
  input_settings->fallback_settings
      ->allow_automatic_fallback_on_execution_error = true;
  compute = ConvertFromFlatbuffer(settings_);
  output_settings = compute.tflite_settings().nnapi_settings();
  EXPECT_FALSE(output_settings.fallback_settings()
                   .allow_automatic_fallback_on_compilation_error());
  EXPECT_TRUE(output_settings.fallback_settings()
                  .allow_automatic_fallback_on_execution_error());

  input_settings->allow_fp16_precision_for_fp32 = false;
  compute = ConvertFromFlatbuffer(settings_);
  output_settings = compute.tflite_settings().nnapi_settings();
  EXPECT_FALSE(output_settings.allow_fp16_precision_for_fp32());
}

TEST_F(ConversionTest, NNAPIAllowDynamicDimensions) {
  settings_.tflite_settings.reset(new TFLiteSettingsT());
  settings_.tflite_settings->nnapi_settings.reset(new NNAPISettingsT());
  NNAPISettingsT* input_settings =
      settings_.tflite_settings->nnapi_settings.get();

  proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
  proto::NNAPISettings output_settings =
      compute.tflite_settings().nnapi_settings();
  EXPECT_FALSE(output_settings.allow_dynamic_dimensions());

  input_settings->allow_dynamic_dimensions = true;
  compute = ConvertFromFlatbuffer(settings_);
  output_settings = compute.tflite_settings().nnapi_settings();
  EXPECT_TRUE(output_settings.allow_dynamic_dimensions());
}

TEST_F(ConversionTest, NNAPIBurstComputation) {
  settings_.tflite_settings.reset(new TFLiteSettingsT());
  settings_.tflite_settings->nnapi_settings.reset(new NNAPISettingsT());
  NNAPISettingsT* input_settings =
      settings_.tflite_settings->nnapi_settings.get();

  proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
  proto::NNAPISettings output_settings =
      compute.tflite_settings().nnapi_settings();
  EXPECT_FALSE(output_settings.use_burst_computation());

  input_settings->use_burst_computation = true;
  compute = ConvertFromFlatbuffer(settings_);
  output_settings = compute.tflite_settings().nnapi_settings();
  EXPECT_TRUE(output_settings.use_burst_computation());
}

TEST_F(ConversionTest, NNAPIExecutionPreference) {
  CheckNNAPIExecutionPreference(
      NNAPIExecutionPreference_NNAPI_FAST_SINGLE_ANSWER,
      proto::NNAPIExecutionPreference::NNAPI_FAST_SINGLE_ANSWER);
  CheckNNAPIExecutionPreference(
      NNAPIExecutionPreference_NNAPI_LOW_POWER,
      proto::NNAPIExecutionPreference::NNAPI_LOW_POWER);
  CheckNNAPIExecutionPreference(
      NNAPIExecutionPreference_NNAPI_SUSTAINED_SPEED,
      proto::NNAPIExecutionPreference::NNAPI_SUSTAINED_SPEED);
  CheckNNAPIExecutionPreference(NNAPIExecutionPreference_UNDEFINED,
                                proto::NNAPIExecutionPreference::UNDEFINED);
}

TEST_F(ConversionTest, NNAPIExecutionPriority) {
  CheckNNAPIExecutionPriority(
      NNAPIExecutionPriority_NNAPI_PRIORITY_LOW,
      proto::NNAPIExecutionPriority::NNAPI_PRIORITY_LOW);
  CheckNNAPIExecutionPriority(
      NNAPIExecutionPriority_NNAPI_PRIORITY_MEDIUM,
      proto::NNAPIExecutionPriority::NNAPI_PRIORITY_MEDIUM);
  CheckNNAPIExecutionPriority(
      NNAPIExecutionPriority_NNAPI_PRIORITY_HIGH,
      proto::NNAPIExecutionPriority::NNAPI_PRIORITY_HIGH);
  CheckNNAPIExecutionPriority(
      NNAPIExecutionPriority_NNAPI_PRIORITY_UNDEFINED,
      proto::NNAPIExecutionPriority::NNAPI_PRIORITY_UNDEFINED);
}

TEST_F(ConversionTest, GPUSettings) {
  settings_.tflite_settings.reset(new TFLiteSettingsT());
  settings_.tflite_settings->gpu_settings.reset(new GPUSettingsT());
  GPUSettingsT* input_settings = settings_.tflite_settings->gpu_settings.get();

  input_settings->is_precision_loss_allowed = true;
  proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
  proto::GPUSettings output_settings = compute.tflite_settings().gpu_settings();
  EXPECT_TRUE(output_settings.is_precision_loss_allowed());

  input_settings->is_precision_loss_allowed = false;
  compute = ConvertFromFlatbuffer(settings_);
  output_settings = compute.tflite_settings().gpu_settings();
  EXPECT_FALSE(output_settings.is_precision_loss_allowed());

  EXPECT_TRUE(output_settings.enable_quantized_inference());
  input_settings->enable_quantized_inference = false;
  compute = ConvertFromFlatbuffer(settings_);
  output_settings = compute.tflite_settings().gpu_settings();
  EXPECT_FALSE(output_settings.enable_quantized_inference());
}

TEST_F(ConversionTest, GPUBacked) {
  CheckGPUBackend(GPUBackend_UNSET, proto::GPUBackend::UNSET);
  CheckGPUBackend(GPUBackend_OPENCL, proto::GPUBackend::OPENCL);
  CheckGPUBackend(GPUBackend_OPENGL, proto::GPUBackend::OPENGL);
}

TEST_F(ConversionTest, GPUInferencePriority) {
  settings_.tflite_settings.reset(new TFLiteSettingsT());
  settings_.tflite_settings->gpu_settings.reset(new GPUSettingsT());
  GPUSettingsT* input_settings = settings_.tflite_settings->gpu_settings.get();

  input_settings->inference_priority1 =
      GPUInferencePriority_GPU_PRIORITY_MIN_MEMORY_USAGE;
  input_settings->inference_priority2 =
      GPUInferencePriority_GPU_PRIORITY_MIN_LATENCY;
  // Third priority is AUTO by default.

  proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
  proto::GPUSettings output_settings = compute.tflite_settings().gpu_settings();

  EXPECT_EQ(proto::GPUInferencePriority::GPU_PRIORITY_MIN_MEMORY_USAGE,
            output_settings.inference_priority1());
  EXPECT_EQ(proto::GPUInferencePriority::GPU_PRIORITY_MIN_LATENCY,
            output_settings.inference_priority2());
  EXPECT_EQ(proto::GPUInferencePriority::GPU_PRIORITY_AUTO,
            output_settings.inference_priority3());
}

TEST_F(ConversionTest, GPUInferencePreference) {
  settings_.tflite_settings.reset(new TFLiteSettingsT());
  settings_.tflite_settings->gpu_settings.reset(new GPUSettingsT());
  GPUSettingsT* input_settings = settings_.tflite_settings->gpu_settings.get();

  input_settings->inference_preference =
      GPUInferenceUsage_GPU_INFERENCE_PREFERENCE_FAST_SINGLE_ANSWER;
  proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
  proto::GPUSettings output_settings = compute.tflite_settings().gpu_settings();
  EXPECT_EQ(
      proto::GPUInferenceUsage::GPU_INFERENCE_PREFERENCE_FAST_SINGLE_ANSWER,
      output_settings.inference_preference());

  input_settings->inference_preference =
      GPUInferenceUsage_GPU_INFERENCE_PREFERENCE_SUSTAINED_SPEED;
  compute = ConvertFromFlatbuffer(settings_);
  output_settings = compute.tflite_settings().gpu_settings();
  EXPECT_EQ(proto::GPUInferenceUsage::GPU_INFERENCE_PREFERENCE_SUSTAINED_SPEED,
            output_settings.inference_preference());
}

TEST_F(ConversionTest, HexagonSettings) {
  settings_.tflite_settings.reset(new TFLiteSettingsT());
  settings_.tflite_settings->hexagon_settings.reset(new HexagonSettingsT());
  HexagonSettingsT* input_settings =
      settings_.tflite_settings->hexagon_settings.get();
  input_settings->debug_level = 1;
  input_settings->powersave_level = 2;
  input_settings->print_graph_profile = true;

  const proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
  const proto::HexagonSettings& output_settings =
      compute.tflite_settings().hexagon_settings();
  EXPECT_EQ(1, output_settings.debug_level());
  EXPECT_EQ(2, output_settings.powersave_level());
  EXPECT_TRUE(output_settings.print_graph_profile());
  EXPECT_FALSE(output_settings.print_graph_debug());
}

TEST_F(ConversionTest, EdgeTpuSettings) {
  settings_.tflite_settings.reset(new TFLiteSettingsT());
  settings_.tflite_settings->edgetpu_settings.reset(new EdgeTpuSettingsT());
  EdgeTpuSettingsT* input_settings =
      settings_.tflite_settings->edgetpu_settings.get();

  constexpr EdgeTpuPowerState kInferencePowerState = EdgeTpuPowerState_ACTIVE;
  constexpr EdgeTpuPowerState kInactivePowerState =
      EdgeTpuPowerState_ACTIVE_MIN_POWER;
  constexpr int64_t kInactiveTimeoutUs = 300000;
  constexpr int kInferencePriority = 2;
  const std::string kModelToken = "model_token";
  constexpr EdgeTpuSettings_::FloatTruncationType kFloatTruncationType =
      EdgeTpuSettings_::FloatTruncationType_HALF;

  input_settings->inference_power_state = kInferencePowerState;
  input_settings->inference_priority = kInferencePriority;
  input_settings->model_token = kModelToken;
  input_settings->float_truncation_type = kFloatTruncationType;

  std::unique_ptr<EdgeTpuInactivePowerConfigT> inactive_power_config(
      new EdgeTpuInactivePowerConfigT());
  inactive_power_config->inactive_power_state = kInactivePowerState;
  inactive_power_config->inactive_timeout_us = kInactiveTimeoutUs;
  input_settings->inactive_power_configs.emplace_back(
      std::move(inactive_power_config));

  constexpr EdgeTpuDeviceSpec_::PlatformType kPlatformType =
      EdgeTpuDeviceSpec_::PlatformType_MMIO;
  constexpr int kNumChips = 1;
  const std::string kDevicePath = "/dev/abrolhos";
  constexpr int kChipFamily = 1;

  input_settings->edgetpu_device_spec.reset(new EdgeTpuDeviceSpecT());
  EdgeTpuDeviceSpecT* input_spec = input_settings->edgetpu_device_spec.get();
  input_spec->platform_type = kPlatformType;
  input_spec->num_chips = kNumChips;
  input_spec->chip_family = kChipFamily;

  proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
  proto::EdgeTpuSettings output_settings =
      compute.tflite_settings().edgetpu_settings();

  EXPECT_EQ(
      static_cast<EdgeTpuPowerState>(output_settings.inference_power_state()),
      kInferencePowerState);
  EXPECT_EQ(output_settings.inactive_power_configs().size(), 1);
  EXPECT_EQ(
      static_cast<EdgeTpuPowerState>(output_settings.inactive_power_configs()
                                         .at(0)
                                         .inactive_power_state()),
      kInactivePowerState);
  EXPECT_EQ(
      output_settings.inactive_power_configs().at(0).inactive_timeout_us(),
      kInactiveTimeoutUs);

  EXPECT_EQ(output_settings.inference_priority(), kInferencePriority);
  EXPECT_EQ(output_settings.model_token(), kModelToken);
  EXPECT_EQ(static_cast<EdgeTpuSettings_::FloatTruncationType>(
                output_settings.float_truncation_type()),
            kFloatTruncationType);

  EXPECT_EQ(static_cast<EdgeTpuDeviceSpec_::PlatformType>(
                output_settings.edgetpu_device_spec().platform_type()),
            kPlatformType);
  EXPECT_EQ(output_settings.edgetpu_device_spec().num_chips(), kNumChips);
  EXPECT_EQ(output_settings.edgetpu_device_spec().device_paths_size(), 0);
  EXPECT_EQ(output_settings.edgetpu_device_spec().chip_family(), kChipFamily);

  input_spec->device_paths.push_back(kDevicePath);

  compute = ConvertFromFlatbuffer(settings_);
  output_settings = compute.tflite_settings().edgetpu_settings();
  EXPECT_EQ(output_settings.edgetpu_device_spec().device_paths().size(), 1);
  EXPECT_EQ(output_settings.edgetpu_device_spec().device_paths()[0],
            kDevicePath);
}

TEST_F(ConversionTest, XNNPackSettings) {
  settings_.tflite_settings.reset(new TFLiteSettingsT());
  settings_.tflite_settings->xnnpack_settings.reset(new XNNPackSettingsT());
  XNNPackSettingsT* input_settings =
      settings_.tflite_settings->xnnpack_settings.get();

  input_settings->num_threads = 2;
  const proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
  EXPECT_EQ(compute.tflite_settings().xnnpack_settings().num_threads(), 2);
}

TEST_F(ConversionTest, CoralSettings) {
  settings_.tflite_settings.reset(new TFLiteSettingsT());
  settings_.tflite_settings->coral_settings.reset(new CoralSettingsT());
  CoralSettingsT* input_settings =
      settings_.tflite_settings->coral_settings.get();

  input_settings->device = "test";
  input_settings->performance = CoralSettings_::Performance_HIGH;
  input_settings->usb_always_dfu = true;
  input_settings->usb_max_bulk_in_queue_length = 768;

  const proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
  const proto::CoralSettings& output_settings =
      compute.tflite_settings().coral_settings();
  EXPECT_EQ("test", output_settings.device());
  EXPECT_TRUE(output_settings.usb_always_dfu());
  EXPECT_EQ(proto::CoralSettings::HIGH, output_settings.performance());
  EXPECT_EQ(768, output_settings.usb_max_bulk_in_queue_length());
}

TEST_F(ConversionTest, CPUSettings) {
  settings_.tflite_settings.reset(new TFLiteSettingsT());
  settings_.tflite_settings->cpu_settings.reset(new CPUSettingsT());

  settings_.tflite_settings->cpu_settings->num_threads = 2;
  const proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
  EXPECT_EQ(compute.tflite_settings().cpu_settings().num_threads(), 2);
}

TEST_F(ConversionTest, MaxDelegatedPartitions) {
  settings_.tflite_settings.reset(new TFLiteSettingsT());
  settings_.tflite_settings->max_delegated_partitions = 2;
  const proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
  EXPECT_EQ(compute.tflite_settings().max_delegated_partitions(), 2);
}

TEST_F(ConversionTest, MiniBenchmarkSettings) {
  settings_.tflite_settings.reset(new TFLiteSettingsT());
  settings_.tflite_settings->cpu_settings.reset(new CPUSettingsT());
  settings_.tflite_settings->cpu_settings->num_threads = 2;
  settings_.model_identifier_for_statistics = "id";
  settings_.model_namespace_for_statistics = "ns";
  settings_.settings_to_test_locally.reset(new MinibenchmarkSettingsT());
  MinibenchmarkSettingsT* mini_settings =
      settings_.settings_to_test_locally.get();
  mini_settings->model_file.reset(new ModelFileT());
  mini_settings->model_file->filename = "test_model";
  mini_settings->storage_paths.reset(new BenchmarkStoragePathsT());
  mini_settings->storage_paths->storage_file_path = "/data/local/tmp";
  std::unique_ptr<TFLiteSettingsT> xnnpack(new TFLiteSettingsT());
  xnnpack->xnnpack_settings.reset(new XNNPackSettingsT());
  xnnpack->xnnpack_settings->num_threads = 2;
  std::unique_ptr<TFLiteSettingsT> hexagon(new TFLiteSettingsT());
  hexagon->hexagon_settings.reset(new HexagonSettingsT());
  hexagon->hexagon_settings->powersave_level = 3;
  mini_settings->settings_to_test.emplace_back(std::move(xnnpack));
  mini_settings->settings_to_test.emplace_back(std::move(hexagon));

  proto::ComputeSettings compute = ConvertFromFlatbuffer(settings_);
  EXPECT_EQ(2, compute.tflite_settings().cpu_settings().num_threads());
  EXPECT_EQ("id", compute.model_identifier_for_statistics());
  EXPECT_EQ("ns", compute.model_namespace_for_statistics());
  EXPECT_TRUE(compute.has_settings_to_test_locally());
  const proto::MinibenchmarkSettings& mini_output =
      compute.settings_to_test_locally();
  EXPECT_EQ("test_model", mini_output.model_file().filename());
  EXPECT_EQ("/data/local/tmp", mini_output.storage_paths().storage_file_path());

  EXPECT_EQ(2, mini_output.settings_to_test_size());
  EXPECT_EQ(
      2, mini_output.settings_to_test().at(0).xnnpack_settings().num_threads());
  EXPECT_EQ(3, mini_output.settings_to_test()
                   .at(1)
                   .hexagon_settings()
                   .powersave_level());

  compute =
      ConvertFromFlatbuffer(settings_, /*skip_mini_benchmark_settings=*/true);
  EXPECT_EQ(2, compute.tflite_settings().cpu_settings().num_threads());
  EXPECT_EQ("id", compute.model_identifier_for_statistics());
  EXPECT_EQ("ns", compute.model_namespace_for_statistics());
  EXPECT_FALSE(compute.has_settings_to_test_locally());
}

TEST_F(ConversionTest, BestAccelerationDecisionEvent) {
  event_.is_log_flushing_event = true;
  event_.best_acceleration_decision.reset(new BestAccelerationDecisionT());
  event_.best_acceleration_decision->number_of_source_events = 4;
  event_.best_acceleration_decision->min_inference_time_us = 3000;

  proto::MiniBenchmarkEvent proto_event = ConvertFromFlatbuffer(event_);
  EXPECT_TRUE(proto_event.is_log_flushing_event());
  const auto& best_decision = proto_event.best_acceleration_decision();
  EXPECT_EQ(4, best_decision.number_of_source_events());
  EXPECT_EQ(3000, best_decision.min_inference_time_us());
  EXPECT_FALSE(best_decision.has_min_latency_event());

  event_.best_acceleration_decision->min_latency_event.reset(
      new BenchmarkEventT());
  auto* min_event = event_.best_acceleration_decision->min_latency_event.get();
  min_event->event_type = BenchmarkEventType_END;
  min_event->tflite_settings.reset(new TFLiteSettingsT());
  min_event->tflite_settings->delegate = Delegate_XNNPACK;
  min_event->tflite_settings->xnnpack_settings.reset(new XNNPackSettingsT());
  min_event->tflite_settings->xnnpack_settings->num_threads = 2;
  min_event->result.reset(new BenchmarkResultT());
  min_event->result->initialization_time_us.push_back(100);
  min_event->result->initialization_time_us.push_back(110);
  min_event->result->inference_time_us.push_back(3000);
  min_event->result->inference_time_us.push_back(3500);
  min_event->result->max_memory_kb = 1234;
  min_event->result->ok = true;
  min_event->boottime_us = 1111;
  min_event->wallclock_us = 2222;

  proto_event = ConvertFromFlatbuffer(event_);
  EXPECT_TRUE(proto_event.best_acceleration_decision().has_min_latency_event());
  const auto& proto_min_event =
      proto_event.best_acceleration_decision().min_latency_event();
  EXPECT_EQ(proto::BenchmarkEventType::END, proto_min_event.event_type());
  EXPECT_EQ(proto::Delegate::XNNPACK,
            proto_min_event.tflite_settings().delegate());
  EXPECT_EQ(2,
            proto_min_event.tflite_settings().xnnpack_settings().num_threads());
  EXPECT_TRUE(proto_min_event.has_result());
  EXPECT_EQ(2, proto_min_event.result().initialization_time_us_size());
  EXPECT_EQ(100, proto_min_event.result().initialization_time_us()[0]);
  EXPECT_EQ(110, proto_min_event.result().initialization_time_us()[1]);
  EXPECT_EQ(2, proto_min_event.result().inference_time_us_size());
  EXPECT_EQ(3000, proto_min_event.result().inference_time_us()[0]);
  EXPECT_EQ(3500, proto_min_event.result().inference_time_us()[1]);
  EXPECT_EQ(1234, proto_min_event.result().max_memory_kb());
  EXPECT_TRUE(proto_min_event.result().ok());
  EXPECT_EQ(1111, proto_min_event.boottime_us());
  EXPECT_EQ(2222, proto_min_event.wallclock_us());
}

TEST_F(ConversionTest, BenchmarkInitializationEvent) {
  event_.initialization_failure.reset(new BenchmarkInitializationFailureT());
  event_.initialization_failure->initialization_status = 101;

  proto::MiniBenchmarkEvent proto_event = ConvertFromFlatbuffer(event_);
  EXPECT_FALSE(proto_event.is_log_flushing_event());
  EXPECT_EQ(101, proto_event.initialization_failure().initialization_status());
}

TEST_F(ConversionTest, BenchmarkError) {
  event_.benchmark_event.reset(new BenchmarkEventT());
  event_.benchmark_event->error.reset(new BenchmarkErrorT());
  auto* error = event_.benchmark_event->error.get();
  error->stage = BenchmarkStage_INITIALIZATION;
  error->exit_code = 123;
  error->signal = 321;
  error->mini_benchmark_error_code = 456;
  std::unique_ptr<ErrorCodeT> code1(new ErrorCodeT());
  code1->source = Delegate_EDGETPU;
  code1->tflite_error = 3;
  code1->underlying_api_error = 301;
  error->error_code.emplace_back(std::move(code1));
  std::unique_ptr<ErrorCodeT> code2(new ErrorCodeT());
  code2->source = Delegate_NNAPI;
  code2->tflite_error = 4;
  code2->underlying_api_error = 404;
  error->error_code.emplace_back(std::move(code2));

  const proto::MiniBenchmarkEvent proto_event = ConvertFromFlatbuffer(event_);
  const auto& proto_error = proto_event.benchmark_event().error();
  EXPECT_EQ(proto::BenchmarkStage::INITIALIZATION, proto_error.stage());
  EXPECT_EQ(123, proto_error.exit_code());
  EXPECT_EQ(321, proto_error.signal());
  EXPECT_EQ(456, proto_error.mini_benchmark_error_code());
  EXPECT_EQ(2, proto_error.error_code_size());

  EXPECT_EQ(proto::Delegate::EDGETPU, proto_error.error_code()[0].source());
  EXPECT_EQ(3, proto_error.error_code()[0].tflite_error());
  EXPECT_EQ(301, proto_error.error_code()[0].underlying_api_error());

  EXPECT_EQ(proto::Delegate::NNAPI, proto_error.error_code()[1].source());
  EXPECT_EQ(4, proto_error.error_code()[1].tflite_error());
  EXPECT_EQ(404, proto_error.error_code()[1].underlying_api_error());
}

TEST_F(ConversionTest, BenchmarkMetric) {
  event_.benchmark_event.reset(new BenchmarkEventT());
  event_.benchmark_event->result.reset(new BenchmarkResultT());
  std::unique_ptr<BenchmarkMetricT> metric(new BenchmarkMetricT());
  metric->name = "test";
  metric->values.push_back(1.234);
  metric->values.push_back(5.678);
  event_.benchmark_event->result->metrics.emplace_back(std::move(metric));

  const proto::MiniBenchmarkEvent proto_event = ConvertFromFlatbuffer(event_);
  EXPECT_EQ(1, proto_event.benchmark_event().result().metrics_size());
  const auto& proto_metric =
      proto_event.benchmark_event().result().metrics()[0];
  EXPECT_EQ("test", proto_metric.name());
  EXPECT_EQ(2, proto_metric.values_size());
  EXPECT_FLOAT_EQ(1.234, proto_metric.values()[0]);
  EXPECT_FLOAT_EQ(5.678, proto_metric.values()[1]);
}
}  // namespace
}  // namespace acceleration
}  // namespace tflite
