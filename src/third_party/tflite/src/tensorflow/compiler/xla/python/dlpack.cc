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

#include "tensorflow/compiler/xla/python/dlpack.h"

#include <memory>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "include/dlpack/dlpack.h"  // from @dlpack
#include "tensorflow/compiler/xla/pjrt/pjrt_client.h"
#include "tensorflow/compiler/xla/pjrt/tracked_device_buffer.h"
#include "tensorflow/compiler/xla/python/traceback.h"
#include "tensorflow/compiler/xla/types.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/stream_executor/cuda/cuda_platform_id.h"
#include "tensorflow/stream_executor/device_memory.h"
#include "tensorflow/stream_executor/host/host_platform_id.h"
#include "tensorflow/stream_executor/platform.h"

namespace py = pybind11;

namespace xla {
namespace {

const char* const kDlTensorCapsuleName = "dltensor";

struct DLPackTensor {
  std::shared_ptr<TrackedDeviceBuffer> buffer;
  std::vector<int64> shape;
  std::vector<int64> strides;
  DLManagedTensor tensor;
};

void DLPackTensorDeleter(DLManagedTensor* t) {
  if (t) {
    delete static_cast<DLPackTensor*>(t->manager_ctx);
  }
}

StatusOr<DLDataType> PrimitiveTypeToDLDataType(PrimitiveType type) {
  switch (type) {
    case S8:
      return DLDataType{kDLInt, 8, 1};
    case S16:
      return DLDataType{kDLInt, 16, 1};
    case S32:
      return DLDataType{kDLInt, 32, 1};
    case S64:
      return DLDataType{kDLInt, 64, 1};
    case U8:
      return DLDataType{kDLUInt, 8, 1};
    case U16:
      return DLDataType{kDLUInt, 16, 1};
    case U32:
      return DLDataType{kDLUInt, 32, 1};
    case U64:
      return DLDataType{kDLUInt, 64, 1};
    case F16:
      return DLDataType{kDLFloat, 16, 1};
    case F32:
      return DLDataType{kDLFloat, 32, 1};
    case F64:
      return DLDataType{kDLFloat, 64, 1};
    case BF16:
      return DLDataType{kDLBfloat, 16, 1};
    case PRED:
    case C64:
    case C128:
    default:
      return Unimplemented("XLA type %s has no DLPack equivalent",
                           PrimitiveType_Name(type));
  }
}

StatusOr<PrimitiveType> DLDataTypeToPrimitiveType(DLDataType type) {
  if (type.lanes != 1) {
    return Unimplemented("DLPack types with lanes != 1 not implemented, got %d",
                         type.lanes);
  }
  switch (type.code) {
    case kDLInt:
      switch (type.bits) {
        case 8:
          return S8;
        case 16:
          return S16;
        case 32:
          return S32;
        case 64:
          return S64;
        default:
          return Unimplemented(
              "Invalid or unsupported DLPack integer width: %d bits",
              type.bits);
      }
    case kDLUInt:
      switch (type.bits) {
        case 8:
          return U8;
        case 16:
          return U16;
        case 32:
          return U32;
        case 64:
          return U64;
        default:
          return Unimplemented(
              "Invalid or unsupported DLPack unsigned integer width: %d bits",
              type.bits);
      }
    case kDLFloat:
      switch (type.bits) {
        case 16:
          return F16;
        case 32:
          return F32;
        case 64:
          return F64;
        default:
          return Unimplemented(
              "Invalid or unsupported DLPack float width: %d bits", type.bits);
      }
    case kDLBfloat:
      switch (type.bits) {
        case 16:
          return BF16;
        default:
          return Unimplemented(
              "Invalid or unsupported DLPack Bfloat width: %d bits", type.bits);
      }
    default:
      return Unimplemented("Unknown or invalid DLPack type code %d", type.code);
  }
}

// Returns the strides for `shape`.
std::vector<int64> StridesForShape(const Shape& shape) {
  std::vector<int64> strides;
  CHECK(shape.IsArray());
  CHECK(shape.has_layout());

  strides.resize(shape.dimensions_size());
  int64 stride = 1;
  for (int i : shape.layout().minor_to_major()) {
    strides.at(i) = stride;
    stride *= shape.dimensions(i);
  }
  return strides;
}

StatusOr<std::vector<int64>> StridesToLayout(absl::Span<int64 const> dims,
                                             absl::Span<int64 const> strides) {
  CHECK_EQ(dims.size(), strides.size());
  std::vector<int64> minor_to_major(dims.size());
  std::iota(minor_to_major.begin(), minor_to_major.end(), 0);
  absl::c_sort(minor_to_major, [&](int a, int b) {
    if (strides[a] < strides[b]) {
      return true;
    }
    if (strides[a] > strides[b]) {
      return false;
    }
    return dims[a] == 1 && dims[b] != 1;
  });
  int64 stride = 1;
  for (int64 d : minor_to_major) {
    if (strides[d] != stride) {
      return Unimplemented(
          "Only DLPack tensors with trivial (compact) striding are supported; "
          "i.e., tensors whose striding represents a transposition of the "
          "underlying buffer but not broadcasting. Dimensions were: [%s], "
          "strides were [%s].",
          absl::StrJoin(dims, ","), absl::StrJoin(strides, ","));
    }
    stride *= dims[d];
  }
  return minor_to_major;
}

StatusOr<DLDeviceType> DLDeviceTypeForDevice(const Device& device) {
  const se::Platform* platform =
      device.local_device_state()->executor()->platform();
  if (platform->id() == se::host::kHostPlatformId) {
    return kDLCPU;
  } else if (platform->id() == se::cuda::kCudaPlatformId) {
    return kDLGPU;
  }
  return InvalidArgument("Device %s cannot be used as a DLPack device.",
                         device.DebugString());
}

StatusOr<DLContext> DLContextForDevice(const Device& device) {
  DLContext context;
  TF_ASSIGN_OR_RETURN(context.device_type, DLDeviceTypeForDevice(device));
  context.device_id = device.local_device_state()->device_ordinal();
  return context;
}

StatusOr<Device*> DeviceForDLContext(const PjRtClient& client,
                                     const DLContext& context) {
  se::Platform::Id platform_id;
  switch (context.device_type) {
    case kDLCPU:
      platform_id = se::host::kHostPlatformId;
      break;
    case kDLGPU:
      platform_id = se::cuda::kCudaPlatformId;
      break;
    default:
      return InvalidArgument("Unknown/unsupported DLPack device type %d",
                             context.device_type);
  }
  auto it = absl::c_find_if(client.local_devices(), [&](Device* device) {
    return device->local_device_state()->executor()->platform()->id() ==
               platform_id &&
           device->local_device_state()->device_ordinal() == context.device_id;
  });
  if (it == client.local_devices().end()) {
    return InvalidArgument(
        "No matching device found for DLPack device_type %d device_id %d",
        context.device_type, context.device_id);
  }
  return *it;
}

}  // namespace

StatusOr<py::capsule> BufferToDLPackManagedTensor(PyBuffer* buffer) {
  auto pack = std::make_unique<DLPackTensor>();
  // Block on outstanding operations, so that it is safe to read or mutate the
  // returned buffer.
  StatusOr<std::shared_ptr<TrackedDeviceBuffer>> buffer_or =
      buffer->buffer()->Release(/*wait_for_operations_to_complete=*/true);
  if (!buffer_or.ok()) {
    return InvalidArgument(
        "Buffer synchronization failed converting to DLPack tensor: %s",
        buffer_or.status().ToString());
  }
  pack->buffer = buffer_or.ConsumeValueOrDie();
  if (!pack->buffer) {
    return InvalidArgument(
        "Cannot convert deleted/invalid buffer to DLPack tensor.");
  }
  pack->tensor.manager_ctx = pack.get();
  pack->tensor.deleter = DLPackTensorDeleter;
  DLTensor& dt = pack->tensor.dl_tensor;
  if (buffer->buffer()->on_device_shape().IsTuple()) {
    return Unimplemented(
        "unsafe_buffer_pointer is not implemented for tuple "
        "buffers.");
  }
  TF_RET_CHECK(pack->buffer->device_memory().size() == 1);
  dt.data = pack->buffer->device_memory().front().opaque();
  TF_ASSIGN_OR_RETURN(dt.ctx, DLContextForDevice(*buffer->buffer()->device()));
  dt.ctx.device_id =
      buffer->buffer()->device()->local_device_state()->device_ordinal();
  dt.ndim = buffer->buffer()->on_host_shape().dimensions_size();
  TF_ASSIGN_OR_RETURN(dt.dtype,
                      PrimitiveTypeToDLDataType(
                          buffer->buffer()->on_host_shape().element_type()));

  pack->shape =
      std::vector<int64>(buffer->buffer()->on_host_shape().dimensions().begin(),
                         buffer->buffer()->on_host_shape().dimensions().end());
  pack->strides = StridesForShape(buffer->buffer()->on_host_shape());
  dt.shape = reinterpret_cast<std::int64_t*>(pack->shape.data());
  dt.strides = reinterpret_cast<std::int64_t*>(pack->strides.data());
  dt.byte_offset = 0;

  py::capsule capsule(&pack.release()->tensor, kDlTensorCapsuleName,
                      [](PyObject* obj) {
                        DLManagedTensor* dlmt = static_cast<DLManagedTensor*>(
                            PyCapsule_GetPointer(obj, kDlTensorCapsuleName));
                        if (dlmt) {
                          DLPackTensorDeleter(dlmt);
                        } else {
                          // The tensor has been deleted. Clear any error from
                          // PyCapsule_GetPointer.
                          PyErr_Clear();
                        }
                      });
  return capsule;
}

StatusOr<std::unique_ptr<PyBuffer>> DLPackManagedTensorToBuffer(
    const pybind11::capsule& tensor, std::shared_ptr<PyClient> client) {
  if (absl::string_view(tensor.name()) != kDlTensorCapsuleName) {
    return InvalidArgument(
        "DLPack tensor must be a capsule with name \"dltensor\", got \"%s\". "
        "Note that a DLPack tensor may be consumed at most once.",
        absl::string_view(tensor.name()));
  }
  DLManagedTensor* dlmt = static_cast<DLManagedTensor*>(tensor);
  if (dlmt->dl_tensor.ndim < 0) {
    return InvalidArgument(
        "Number of dimensions in DLManagedTensor must be nonnegative, got %d",
        dlmt->dl_tensor.ndim);
  }
  TF_ASSIGN_OR_RETURN(
      Device * device,
      DeviceForDLContext(*client->pjrt_client(), dlmt->dl_tensor.ctx));
  absl::Span<int64 const> dimensions(
      reinterpret_cast<int64*>(dlmt->dl_tensor.shape), dlmt->dl_tensor.ndim);
  TF_ASSIGN_OR_RETURN(PrimitiveType element_type,
                      DLDataTypeToPrimitiveType(dlmt->dl_tensor.dtype));

  std::vector<int64> minor_to_major;
  if (dlmt->dl_tensor.strides && !absl::c_find(dimensions, 0)) {
    absl::Span<int64 const> strides(
        reinterpret_cast<int64*>(dlmt->dl_tensor.strides),
        dlmt->dl_tensor.ndim);
    TF_ASSIGN_OR_RETURN(minor_to_major, StridesToLayout(dimensions, strides));
  } else {
    minor_to_major.resize(dlmt->dl_tensor.ndim);
    std::iota(minor_to_major.rbegin(), minor_to_major.rend(), 0);
  }
  Shape shape =
      ShapeUtil::MakeShapeWithLayout(element_type, dimensions, minor_to_major);
  se::DeviceMemoryBase buffer(
      static_cast<char*>(dlmt->dl_tensor.data) + dlmt->dl_tensor.byte_offset,
      ShapeUtil::ByteSizeOf(shape));

  std::function<void()> on_delete_callback;
  if (dlmt->deleter) {
    on_delete_callback = [dlmt]() { dlmt->deleter(dlmt); };
  }
  absl::Span<const std::shared_ptr<BufferSequencingEvent>> definition_events;
  auto device_buffer = std::make_shared<TrackedDeviceBuffer>(
      /*allocator=*/nullptr, dlmt->dl_tensor.ctx.device_id,
      std::initializer_list<se::DeviceMemoryBase>{buffer}, definition_events,
      std::move(on_delete_callback));

  // We have taken ownership of the array inside the capsule; make sure the
  // capsule it cannot be used again.
  PyCapsule_SetName(tensor.ptr(), "used_dltensor");
  PyCapsule_SetDestructor(tensor.ptr(), nullptr);
  auto pjrt_buffer = std::make_unique<PjRtBuffer>(
      shape, shape, std::move(device_buffer), client->pjrt_client(), device);
  return std::make_unique<PyBuffer>(std::move(client), std::move(pjrt_buffer),
                                    Traceback::Get());
}

}  // namespace xla
