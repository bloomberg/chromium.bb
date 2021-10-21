/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0(the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/c/experimental/stream_executor/stream_executor.h"
#include "tensorflow/c/experimental/stream_executor/stream_executor_internal.h"
#include "tensorflow/c/experimental/stream_executor/stream_executor_test_util.h"
#include "tensorflow/cc/client/client_session.h"
#include "tensorflow/cc/framework/scope.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/core/common_runtime/pluggable_device/pluggable_device_factory.h"
#include "tensorflow/core/framework/function_testlib.h"
#include "tensorflow/core/graph/node_builder.h"
#include "tensorflow/core/kernels/ops_testutil.h"
#include "tensorflow/stream_executor/event.h"
#include "tensorflow/stream_executor/multi_platform_manager.h"

namespace tensorflow {
namespace {

class WhileOpTest : public OpsTestBase {
 protected:
  WhileOpTest() {}
  void SetUp() override {
    stream_executor::test_util::PopulateDefaultPlatform(&platform_,
                                                        &platform_fns_);
    stream_executor::test_util::PopulateDefaultDeviceFns(&device_fns_);
    stream_executor::test_util::PopulateDefaultStreamExecutor(&se_);
    stream_executor::test_util::PopulateDefaultTimerFns(&timer_fns_);
  }
  void TearDown() override {}

  SP_Platform platform_;
  SP_PlatformFns platform_fns_;
  SP_DeviceFns device_fns_;
  SP_StreamExecutor se_;
  SP_TimerFns timer_fns_;
};

TEST_F(WhileOpTest, WhileOpCPUBuildWithPluggableDevice) {
  const std::string platform_name = "MY_TEST";
  const std::string platform_type = "FAKE";
  platform_.name = platform_name.c_str();
  platform_.type = platform_type.c_str();

  // TODO(penporn): Refactor this to test_pluggable_device.cc.
  static bool memcpy_d2h_called = false;
  se_.memcpy_dtoh = [](const SP_Device* device, SP_Stream stream,
                       void* host_dst, const SP_DeviceMemoryBase* device_src,
                       uint64_t size, TF_Status* status) {
    TF_SetStatus(status, TF_OK, "");
    memcpy_d2h_called = true;
    std::memcpy(host_dst, device_src->opaque, size);
  };
  se_.memcpy_htod = [](const SP_Device* const device, SP_Stream stream,
                       SP_DeviceMemoryBase* const device_dst,
                       const void* host_src, uint64_t size,
                       TF_Status* const status) {
    TF_SetStatus(status, TF_OK, "");
    std::memcpy(device_dst->opaque, host_src, size);
  };

  se_.host_memory_allocate = [](const SP_Device* const device, uint64_t size) {
    return malloc(size);
  };
  se_.host_memory_deallocate = [](const SP_Device* const device, void* mem) {
    free(mem);
  };

  se_.allocate = [](const SP_Device* const device, uint64_t size,
                    int64_t memory_space, SP_DeviceMemoryBase* const mem) {
    mem->struct_size = SP_DEVICE_MEMORY_BASE_STRUCT_SIZE;
    mem->opaque = malloc(size);
    mem->size = size;
  };
  se_.deallocate = [](const SP_Device* const device,
                      SP_DeviceMemoryBase* const mem) {
    free(mem->opaque);
    mem->opaque = nullptr;
    mem->size = 0;
  };

  static SE_EventStatus event_status = SE_EVENT_COMPLETE;
  se_.create_event = [](const SP_Device* const device, SP_Event* event,
                        TF_Status* const status) -> void {
    *event = new SP_Event_st(666);
  };
  se_.destroy_event = [](const SP_Device* const device,
                         SP_Event event) -> void { delete event; };
  se_.get_event_status = [](const SP_Device* const device,
                            SP_Event event) -> SE_EventStatus {
    EXPECT_EQ(event->event_id, 666);
    return event_status;
  };

  std::unique_ptr<stream_executor::CPlatform> cplatform(
      new stream_executor::CPlatform(
          std::move(platform_), stream_executor::test_util::DestroyPlatform,
          std::move(platform_fns_),
          stream_executor::test_util::DestroyPlatformFns,
          std::move(device_fns_), std::move(se_), std::move(timer_fns_)));
  SE_CHECK_OK(stream_executor::MultiPlatformManager::RegisterPlatform(
      std::move(cplatform)));

  DeviceFactory::Register(
      platform_type, new PluggableDeviceFactory(platform_type, platform_name),
      /*priority=*/220, /*is_pluggable_device=*/true);
  std::unique_ptr<Device> plug_device(
      DeviceFactory::NewDevice(platform_type, {}, "/job:a/replica:0"));
  OpsTestBase::SetDevice(platform_type.c_str(), std::move(plug_device));
  std::unique_ptr<Graph> graph(new Graph(OpRegistry::Global()));
  Scope root = Scope::NewRootScope().ExitOnError();
  FunctionDef x_times_two = test::function::XTimesTwo();
  FunctionDef less_than_or_eq = test::function::LessThanOrEqualToN(8);

  FunctionDefLibrary f_lib_proto;
  *f_lib_proto.add_function() = x_times_two;
  *f_lib_proto.add_function() = less_than_or_eq;

  TF_ASSERT_OK(root.graph()->AddFunctionLibrary(f_lib_proto));
  auto a = ops::Placeholder(root.WithOpName("A"), DT_FLOAT);
  AttrValue cond_func;
  cond_func.mutable_func()->set_name("LessThanOrEqualToN");
  (*cond_func.mutable_func()->mutable_attr())["T"].set_type(DT_FLOAT);
  AttrValue body_func;
  body_func.mutable_func()->set_name("XTimesTwo");
  (*body_func.mutable_func()->mutable_attr())["T"].set_type(DT_FLOAT);

  std::vector<NodeBuilder::NodeOut> inputs({NodeBuilder::NodeOut(a.node())});

  Node* node;
  TF_EXPECT_OK(NodeBuilder("while_test", "While", &root.graph()->flib_def())
                   .Input(inputs)
                   .Attr("T", {DT_FLOAT})
                   .Attr("cond", cond_func)
                   .Attr("body", body_func)
                   .Attr("parallel_iterations", 100)
                   .Finalize(root.graph(), &node));
  auto c = ops::Identity(
      root.WithOpName("C").WithControlDependencies(Output(node)), Output(node));

  TF_ASSERT_OK(root.DoShapeInference(node));
  TF_ASSERT_OK(root.ToGraph(graph.get()));
  ClientSession session(root);
  {
    ClientSession::FeedType feeds;
    feeds.emplace(Output(a.node()), Input::Initializer(1.f));
    std::vector<Tensor> out_tensors;
    TF_ASSERT_OK(session.Run(feeds, {Output(c.node())}, &out_tensors));
    ASSERT_EQ(memcpy_d2h_called, true);
    ASSERT_EQ(out_tensors.size(), 1);
    EXPECT_EQ(out_tensors[0].scalar<float>()(), 16.f);
  }
}

}  // namespace
}  // namespace tensorflow
