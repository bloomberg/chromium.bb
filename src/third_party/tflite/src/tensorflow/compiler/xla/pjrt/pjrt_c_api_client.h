/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_COMPILER_XLA_PJRT_PJRT_C_API_CLIENT_H_
#define TENSORFLOW_COMPILER_XLA_PJRT_PJRT_C_API_CLIENT_H_

#include <functional>
#include <string>
#include <utility>

#include "tensorflow/compiler/xla/pjrt/pjrt_client.h"
#include "tensorflow/core/platform/casts.h"

namespace xla {

class PjRtCApiDevice : public PjRtDevice {
 public:
  explicit PjRtCApiDevice(PjRtDevice* wrapped) : wrapped_(wrapped) {}

  // Must set client exactly once.
  void SetClient(PjRtClient* client) {
    CHECK(client_ == nullptr) << ToString();
    client_ = client;
  }

  PjRtClient* client() const override { return client_; }

  bool IsAddressable() const override { return wrapped_->IsAddressable(); }

  int id() const override { return wrapped_->id(); }

  int process_index() const override { return wrapped_->process_index(); }

  int local_hardware_id() const override {
    return wrapped_->local_hardware_id();
  }

  absl::string_view device_kind() const override {
    return wrapped_->device_kind();
  }

  std::string DebugString() const override { return wrapped_->DebugString(); }

  std::string ToString() const override {
    return absl::StrCat("PjRtCApiDevice(wrapped=", wrapped_->ToString(), ")");
  }

  Status TransferToInfeed(const LiteralSlice& literal) override {
    return wrapped_->TransferToInfeed(literal);
  }

  Status TransferFromOutfeed(MutableBorrowingLiteral literal) override {
    return wrapped_->TransferFromOutfeed(std::move(literal));
  }

  std::unique_ptr<ScopedAsyncTrackingEvent> CreateAsyncTrackingEvent(
      absl::string_view description) const override {
    return wrapped_->CreateAsyncTrackingEvent(description);
  }

  const absl::flat_hash_map<std::string, PjRtDeviceAttribute>& Attributes()
      const override {
    return wrapped_->Attributes();
  }

  PjRtDevice* wrapped() const { return wrapped_; }

  static PjRtDevice* GetWrapped(PjRtDevice* c_api_device) {
    return tensorflow::down_cast<PjRtCApiDevice*>(c_api_device)->wrapped();
  }

 private:
  PjRtClient* client_ = nullptr;
  PjRtDevice* wrapped_;
};

class PjRtCApiClient : public PjRtClient {
 public:
  PjRtCApiClient(std::vector<std::unique_ptr<PjRtCApiDevice>> devices,
                 PjRtClient* wrapped);

  ~PjRtCApiClient() override;

  int process_index() const override { return wrapped_->process_index(); }

  int device_count() const override { return wrapped_->device_count(); }

  int addressable_device_count() const override {
    return wrapped_->addressable_device_count();
  }

  absl::Span<PjRtDevice* const> devices() const override { return devices_; }

  absl::Span<PjRtDevice* const> addressable_devices() const override {
    return addressable_devices_;
  }

  StatusOr<PjRtDevice*> LookupDevice(int device_id) const override {
    TF_ASSIGN_OR_RETURN(PjRtDevice * wrapped_device,
                        wrapped_->LookupDevice(device_id));
    return GetCApiDevice(wrapped_device);
  }

  StatusOr<PjRtDevice*> LookupAddressableDevice(
      int local_hardware_id) const override {
    TF_ASSIGN_OR_RETURN(PjRtDevice * wrapped_device,
                        wrapped_->LookupAddressableDevice(local_hardware_id));
    return GetCApiDevice(wrapped_device);
  }

  PjRtPlatformId platform_id() const override {
    return wrapped_->platform_id();
  }

  absl::string_view platform_name() const override {
    return wrapped_->platform_name();
  }

  absl::string_view platform_version() const override {
    return wrapped_->platform_version();
  }

  PjRtRuntimeType runtime_type() const override {
    return wrapped_->runtime_type();
  }

  StatusOr<DeviceAssignment> GetDefaultDeviceAssignment(
      int num_replicas, int num_partitions) const override {
    return wrapped_->GetDefaultDeviceAssignment(num_replicas, num_partitions);
  }

  StatusOr<std::unique_ptr<HloCostAnalysis>> GetHloCostAnalysis() override {
    return wrapped_->GetHloCostAnalysis();
  }

  StatusOr<std::unique_ptr<PjRtExecutable>> Compile(
      const XlaComputation& computation, CompileOptions options) override {
    return WrapExecutable(wrapped_->Compile(computation, options));
  }

  StatusOr<std::unique_ptr<PjRtExecutable>> Compile(
      mlir::ModuleOp module, CompileOptions options) override {
    return WrapExecutable(wrapped_->Compile(module, options));
  }

  StatusOr<absl::optional<std::string>> ExecutableFingerprint(
      const PjRtExecutable& executable) const override;

  StatusOr<std::string> SerializeExecutable(
      const PjRtExecutable& executable) const override;

  StatusOr<std::unique_ptr<PjRtExecutable>> DeserializeExecutable(
      absl::string_view serialized, CompileOptions options) override {
    return WrapExecutable(wrapped_->DeserializeExecutable(serialized, options));
  }

  StatusOr<std::unique_ptr<PjRtBuffer>> CreateUninitializedBuffer(
      const Shape& shape, PjRtDevice* device) override {
    return Unimplemented(
        "PJRT C API does not support CreateUninitializedBuffer");
  }

  StatusOr<std::unique_ptr<AsyncBufferTransferManager>>
  CreateBuffersForAsyncTransfer(absl::Span<const Shape> shapes,
                                PjRtDevice* device) override {
    return Unimplemented(
        "PJRT C API does not support CreateBuffersForAsyncTransfer");
  }

  StatusOr<std::unique_ptr<PjRtBuffer>> BufferFromHostBuffer(
      const void* data, PrimitiveType type, absl::Span<int64_t const> dims,
      absl::optional<absl::Span<int64_t const>> byte_strides,
      HostBufferSemantics host_buffer_semantics,
      std::function<void()> on_done_with_host_buffer,
      PjRtDevice* device) override {
    return WrapBuffer(wrapped_->BufferFromHostBuffer(
        data, type, dims, byte_strides, host_buffer_semantics,
        on_done_with_host_buffer, PjRtCApiDevice::GetWrapped(device)));
  }

  StatusOr<std::unique_ptr<PjRtBuffer>> BufferFromHostLiteral(
      const LiteralSlice& literal, PjRtDevice* device) override {
    return WrapBuffer(wrapped_->BufferFromHostLiteral(
        literal, PjRtCApiDevice::GetWrapped(device)));
  }

  StatusOr<std::unique_ptr<PjRtBuffer>> CreateViewOfDeviceBuffer(
      void* device_ptr, const Shape& shape, PjRtDevice* device,
      std::function<void()> on_delete_callback) override {
    return WrapBuffer(wrapped_->CreateViewOfDeviceBuffer(
        device_ptr, shape, PjRtCApiDevice::GetWrapped(device),
        on_delete_callback));
  }

  StatusOr<std::uintptr_t> UnsafeBufferPointer(PjRtBuffer* buffer) override;

  StatusOr<std::vector<std::unique_ptr<PjRtBuffer>>>
  MakeCrossHostReceiveBuffers(absl::Span<const Shape> shapes,
                              PjRtDevice* device,
                              PjRtCrossHostRecvNotifier&& notifier) override {
    return Unimplemented(
        "PJRT C API does not support MakeCrossHostReceiveBuffers");
  }

  StatusOr<std::vector<std::unique_ptr<PjRtBuffer>>>
  MakeCrossHostReceiveBuffersForGather(
      absl::Span<const Shape> shapes, std::vector<GatherDetails> gather_details,
      PjRtDevice* device, PjRtCrossHostRecvNotifier&& notifier) override {
    return Unimplemented(
        "PJRT C API does not support MakeCrossHostReceiveBuffers");
  }

  StatusOr<ChannelHandle> CreateChannelHandle() override {
    return Unimplemented("PJRT C API does not support CreateChannelHandle");
  }

  StatusOr<ChannelHandle> CreateDeviceToHostChannelHandle() override {
    return Unimplemented(
        "PJRT C API does not support CreateDeviceToHostChannelHandle");
  }

  StatusOr<ChannelHandle> CreateHostToDeviceChannelHandle() override {
    return Unimplemented(
        "PJRT C API does not support CreateHostToDeviceChannelHandle");
  }

  Status Defragment() override { return wrapped_->Defragment(); }

  PjRtDevice* GetCApiDevice(PjRtDevice* wrapped_device) const {
    auto it = wrapped_device_map_.find(wrapped_device);
    CHECK(it != wrapped_device_map_.end());
    return it->second;
  }

  StatusOr<std::unique_ptr<PjRtExecutable>> WrapExecutable(
      StatusOr<std::unique_ptr<PjRtExecutable>> to_wrap);

  StatusOr<std::unique_ptr<PjRtBuffer>> WrapBuffer(
      StatusOr<std::unique_ptr<PjRtBuffer>> to_wrap);

 private:
  std::vector<std::unique_ptr<PjRtCApiDevice>> owned_devices_;
  std::vector<PjRtDevice*> devices_;
  std::vector<PjRtDevice*> addressable_devices_;

  // TODO(skyewm): this is a shim so we can run PjRtCApiClient code without the
  // C API being fully implemented. All methods using wrapped_ should either be
  // marked unimplemented or implemented in terms of the C API, at which point
  // wrapped_ and related functionality should be removed.
  PjRtClient* wrapped_;
  absl::flat_hash_map<PjRtDevice*, PjRtCApiDevice*> wrapped_device_map_;
};

class PjRtCApiBuffer : public PjRtBuffer {
 public:
  PjRtCApiBuffer(PjRtCApiClient* client, std::unique_ptr<PjRtBuffer> wrapped)
      : client_(client), wrapped_(std::move(wrapped)) {}

  const Shape& on_device_shape() const override {
    return wrapped_->on_device_shape();
  }

  StatusOr<Shape> logical_on_device_shape() override {
    return wrapped_->logical_on_device_shape();
  }

  PjRtDevice* device() const override {
    return client_->GetCApiDevice(wrapped_->device());
  }

  PjRtClient* client() const override { return client_; }

  StatusOr<std::unique_ptr<ExternalReference>> AcquireExternalReference()
      override {
    return wrapped_->AcquireExternalReference();
  }

  PjRtFuture<Status> ToLiteral(MutableLiteralBase* literal) override {
    return wrapped_->ToLiteral(literal);
  }

  StatusOr<size_t> GetOnDeviceSizeInBytes() const override {
    return wrapped_->GetOnDeviceSizeInBytes();
  }

  PjRtFuture<Status> CopyRawToHost(void* dst, int64_t offset,
                                   int64_t transfer_size) override {
    return wrapped_->CopyRawToHost(dst, offset, transfer_size);
  }

  void Delete() override { wrapped_->Delete(); }

  StatusOr<std::unique_ptr<ExternalReference>> ReleaseDeviceMemoryOwnership(
      bool wait_for_operations_to_complete) override {
    return wrapped_->ReleaseDeviceMemoryOwnership(
        wait_for_operations_to_complete);
  }

  bool IsDeleted() override { return wrapped_->IsDeleted(); }

  StatusOr<std::unique_ptr<PjRtBuffer>> CopyToDevice(
      PjRtDevice* dst_device) override {
    if (dst_device->client() == client_) {
      return client_->WrapBuffer(
          wrapped_->CopyToDevice(PjRtCApiDevice::GetWrapped(dst_device)));
    } else {
      return wrapped_->CopyToDevice(dst_device);
    }
  }

  void CopyToRemoteDevice(absl::string_view serialized_descriptor,
                          RemoteSendCallback on_done) override {
    LOG(ERROR) << "PJRT C API does not support CopyToRemoteDevice";
  }

  void CopyToRemoteDeviceScattered(
      absl::Span<const std::pair<std::string, RemoteSendCallback>>
          serialized_descriptors_and_callbacks,
      const ScatterDetails& scatter_details) override {
    LOG(ERROR) << "PJRT C API does not support CopyToRemoteDeviceScattered";
  }

  PjRtFuture<Status> GetReadyFuture() override {
    return wrapped_->GetReadyFuture();
  }

  bool IsOnCpu() const override { return wrapped_->IsOnCpu(); }

  PjRtBuffer* wrapped() const { return wrapped_.get(); }

  static PjRtBuffer* GetWrapped(PjRtBuffer* c_api_buffer) {
    return tensorflow::down_cast<PjRtCApiBuffer*>(c_api_buffer)->wrapped();
  }

  static std::vector<PjRtBuffer*> GetWrappedVector(
      absl::Span<PjRtBuffer* const> c_api_buffers) {
    std::vector<PjRtBuffer*> wrapped;
    wrapped.reserve(c_api_buffers.size());
    for (PjRtBuffer* c_api_buf : c_api_buffers) {
      wrapped.push_back(GetWrapped(c_api_buf));
    }
    return wrapped;
  }

 private:
  PjRtCApiClient* client_;
  std::unique_ptr<PjRtBuffer> wrapped_;
};

class PjRtCApiExecutable : public PjRtExecutable {
 public:
  PjRtCApiExecutable(PjRtCApiClient* client,
                     std::unique_ptr<PjRtExecutable> wrapped);

  PjRtClient* client() const override { return client_; }
  absl::string_view name() const override { return wrapped_->name(); }
  int num_replicas() const override { return wrapped_->num_replicas(); }
  int num_partitions() const override { return wrapped_->num_partitions(); }

  int64_t SizeOfGeneratedCodeInBytes() const override {
    return wrapped_->SizeOfGeneratedCodeInBytes();
  }

  const DeviceAssignment& device_assignment() const override {
    return wrapped_->device_assignment();
  }

  absl::Span<const LogicalDeviceIds> addressable_device_logical_ids()
      const override {
    return wrapped_->addressable_device_logical_ids();
  }

  absl::Span<PjRtDevice* const> addressable_devices() const override {
    return addressable_devices_;
  }

  StatusOr<std::vector<std::shared_ptr<HloModule>>> GetHloModules()
      const override {
    return wrapped_->GetHloModules();
  }

  StatusOr<std::vector<std::vector<std::unique_ptr<PjRtBuffer>>>> Execute(
      absl::Span<const std::vector<PjRtBuffer*>> argument_handles,
      const ExecuteOptions& options,
      absl::optional<std::vector<PjRtFuture<Status>>>& returned_futures)
      override;

  StatusOr<std::vector<std::unique_ptr<PjRtBuffer>>> ExecuteSharded(
      absl::Span<PjRtBuffer* const> argument_handles, PjRtDevice* device,
      const ExecuteOptions& options,
      absl::optional<PjRtFuture<Status>>& returned_future,
      bool fill_future) override;

  StatusOr<std::vector<std::unique_ptr<PjRtBuffer>>> ExecutePortable(
      absl::Span<PjRtBuffer* const> argument_handles, PjRtDevice* device,
      const ExecuteOptions& options,
      absl::optional<PjRtFuture<Status>>& returned_future,
      bool fill_future) override;

  void Delete() override { return wrapped_->Delete(); }
  bool IsDeleted() override { return wrapped_->IsDeleted(); }

  PjRtExecutable* wrapped() const { return wrapped_.get(); }

  static PjRtExecutable* GetWrapped(const PjRtExecutable* c_api_executable) {
    return tensorflow::down_cast<const PjRtCApiExecutable*>(c_api_executable)
        ->wrapped();
  }

 private:
  PjRtCApiClient* client_;
  std::unique_ptr<PjRtExecutable> wrapped_;
  std::vector<PjRtDevice*> addressable_devices_;
};

// Takes ownership of wrapped.
StatusOr<std::unique_ptr<PjRtClient>> GetCApiClient(PjRtClient* wrapped);

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_PJRT_PJRT_C_API_CLIENT_H_
