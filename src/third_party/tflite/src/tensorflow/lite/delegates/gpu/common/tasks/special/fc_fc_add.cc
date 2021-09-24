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

#include "tensorflow/lite/delegates/gpu/common/tasks/special/fc_fc_add.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "tensorflow/lite/delegates/gpu/common/operations.h"
#include "tensorflow/lite/delegates/gpu/common/task/gpu_operation.h"
#include "tensorflow/lite/delegates/gpu/common/task/tensor_linear_desc.h"
#include "tensorflow/lite/delegates/gpu/common/types.h"

namespace tflite {
namespace gpu {
namespace {
bool UseBufferForWeights(const GpuInfo& gpu_info) {
  return gpu_info.IsAdreno() || gpu_info.IsAMD() || gpu_info.IsMali();
}

void RearrangeFCWeightsToOIO4I4(
    const tflite::gpu::Tensor<OHWI, DataType::INT8>& weights, uint8_t* dst) {
  const int src_depth = DivideRoundUp(weights.shape.i, 4);
  const int dst_depth = DivideRoundUp(weights.shape.o, 4);

  int counter = 0;
  for (int d = 0; d < dst_depth; ++d) {
    for (int s = 0; s < src_depth; ++s) {
      for (int i = 0; i < 4; ++i) {
        const int src_ch = s * 4 + i;
        for (int j = 0; j < 4; ++j) {
          const int dst_ch = d * 4 + j;
          if (src_ch < weights.shape.i && dst_ch < weights.shape.o) {
            int t =
                127 +
                weights.data[weights.shape.LinearIndex({dst_ch, 0, 0, src_ch})];
            if (t < 0) {
              t = 0;
            }
            dst[counter++] = t;
          } else {
            dst[counter++] = 127;
          }
        }
      }
    }
  }
}
}  // namespace

FCFCAdd::FCFCAdd(const OperationDef& definition, const GpuInfo& gpu_info)
    : GPUOperation(definition) {
  if (gpu_info.IsAdreno()) {
    if (gpu_info.adreno_info.IsAdreno3xx()) {
      work_group_size_ = int3(16, 4, 1);
    } else if (gpu_info.adreno_info.IsAdreno4xx()) {
      work_group_size_ = int3(32, 4, 1);
    } else {
      work_group_size_ = int3(32, 4, 1);
    }
  } else if (gpu_info.IsIntel()) {
    work_group_size_ = int3(8, 4, 1);
  } else if (gpu_info.IsNvidia()) {
    work_group_size_ = int3(8, 4, 1);
  } else if (gpu_info.IsPowerVR()) {
    work_group_size_ = int3(8, 4, 1);
  } else {
    work_group_size_ = int3(16, 4, 1);
  }
}

FCFCAdd::FCFCAdd(FCFCAdd&& kernel) : GPUOperation(std::move(kernel)) {}

FCFCAdd& FCFCAdd::operator=(FCFCAdd&& kernel) {
  if (this != &kernel) {
    GPUOperation::operator=(std::move(kernel));
  }
  return *this;
}

// We split vec vec dot (every thread do vec vec dot product in basic
// vec mat mult) on 4 parts to create more threads
// tid.y thread process every 4-th element in vec vec dot
// Good results for ~1024 x 1024 sizes, for other can be written more
// optimized shaders

std::string FCFCAdd::GetFCFCAddKernelCode(const OperationDef& op_def,
                                          const GpuInfo& gpu_info,
                                          bool weights_are_buffer,
                                          bool quantized_0, bool quantized_1) {
  AddSrcTensor("src_tensor_0", op_def.src_tensors[0]);
  AddSrcTensor("src_tensor_1", op_def.src_tensors[1]);
  AddDstTensor("dst_tensor", op_def.dst_tensors[0]);

  std::string c;
  switch (op_def.precision) {
    case CalculationsPrecision::F32:
      c += "#define FLT16 float16\n";
      break;
    case CalculationsPrecision::F32_F16:
    case CalculationsPrecision::F16:
      c += "#define FLT16 half16\n";
      break;
  }

  c += "#define WG_X " + std::to_string(work_group_size_.x) + "\n";
  c += "#define WG_Y " + std::to_string(work_group_size_.y) + "\n";

  c += R"(MAIN_FUNCTION($0) {
  int gid = get_global_id(0);
  int2 tid;
  tid.x = LOCAL_ID_0;
  tid.y = LOCAL_ID_1;
  ACCUM_FLT4 s = INIT_ACCUM_FLT4(0.0f);
  if (gid < args.dst_tensor.Slices()) {
    for (int c = tid.y; c < args.src_tensor_0.Slices(); c += WG_Y) {
      FLT4 v = args.src_tensor_0.Read(0, 0, c);
)";
  if (weights_are_buffer) {
    c += R"(FLT16 w = args.weights0.Read(c * args.dst_tensor.Slices() + gid);
      FLT4 partial = v.x * FLT16_0123(w);
      partial += v.y * FLT16_4567(w);
      partial += v.z * FLT16_89ab(w);
      partial += v.w * FLT16_cdef(w);
      s += TO_ACCUM_TYPE(partial);
)";
  } else {
    c += R"(FLT4 w0 = args.weights0.Read(c * 4 + 0, gid);
      FLT4 w1 = args.weights0.Read(c * 4 + 1, gid);
      FLT4 w2 = args.weights0.Read(c * 4 + 2, gid);
      FLT4 w3 = args.weights0.Read(c * 4 + 3, gid);
      )";
    if (quantized_0) {
      c += R"(w0 = w0 * args.q0_m + args.q0_a;
      w1 = w1 * args.q0_m + args.q0_a;
      w2 = w2 * args.q0_m + args.q0_a;
      w3 = w3 * args.q0_m + args.q0_a;
)";
    }
    c += R"(FLT4 partial = v.x * w0;
      partial += v.y * w1;
      partial += v.z * w2;
      partial += v.w * w3;
      s += TO_ACCUM_TYPE(partial);
)";
  }
  c += R"(    }
    for (int c = tid.y; c < args.src_tensor_1.Slices(); c += WG_Y) {
      FLT4 v = args.src_tensor_1.Read(0, 0, c);
      )";
  if (weights_are_buffer) {
    c += R"(FLT16 w = args.weights1.Read(c * args.dst_tensor.Slices() + gid);
      FLT4 partial = v.x * FLT16_0123(w);
      partial += v.y * FLT16_4567(w);
      partial += v.z * FLT16_89ab(w);
      partial += v.w * FLT16_cdef(w);
      s += TO_ACCUM_TYPE(partial);
)";
  } else {
    c += R"(FLT4 w0 = args.weights1.Read(c * 4 + 0, gid);
      FLT4 w1 = args.weights1.Read(c * 4 + 1, gid);
      FLT4 w2 = args.weights1.Read(c * 4 + 2, gid);
      FLT4 w3 = args.weights1.Read(c * 4 + 3, gid);
      )";
    if (quantized_1) {
      c += R"(w0 = w0 * args.q1_m + args.q1_a;
      w1 = w1 * args.q1_m + args.q1_a;
      w2 = w2 * args.q1_m + args.q1_a;
      w3 = w3 * args.q1_m + args.q1_a;
)";
    }
    c += R"(FLT4 partial = v.x * w0;
      partial += v.y * w1;
      partial += v.z * w2;
      partial += v.w * w3;
      s += TO_ACCUM_TYPE(partial);
)";
  }
  c += R"(    }
  }
  __local ACCUM_FLT4 temp[WG_X][WG_Y];
  temp[tid.x][tid.y] = s;
  LOCAL_MEM_BARRIER;
  if (gid >= args.dst_tensor.Slices()) {
    return;
  }
  if (tid.y == 0) {
)";
  for (int i = 1; i < work_group_size_.y; ++i) {
    c += "    s += temp[tid.x][" + std::to_string(i) + "];\n";
  }
  c +=
      R"(    FLT4 r0 = TO_FLT4(s) + args.biases0.Read(gid) + args.biases1.Read(gid);
    args.dst_tensor.Write(r0, 0, 0, gid);
  }
})";

  return c;
}

int3 FCFCAdd::GetGridSize() const { return int3(dst_[0]->Slices(), 1, 1); }

void FCFCAdd::UploadQuantizedWeights(
    const tflite::gpu::Tensor<OHWI, DataType::INT8>& weights, float scale,
    float zero_point, int index) {
  const bool f32_weights = definition_.precision == CalculationsPrecision::F32;
  const int src_depth = DivideRoundUp(weights.shape.i, 4);
  const int dst_depth = DivideRoundUp(weights.shape.o, 4);
  Texture2DDescriptor desc;
  desc.element_type = DataType::UINT8;
  desc.normalized = true;
  desc.normalized_type = f32_weights ? DataType::FLOAT32 : DataType::FLOAT16;
  desc.size = int2(src_depth * 4, dst_depth);
  desc.data.resize(src_depth * 4 * dst_depth * 4);
  RearrangeFCWeightsToOIO4I4(weights, desc.data.data());

  std::string q_name = "q" + std::to_string(index) + "_";
  if (definition_.precision == CalculationsPrecision::F32) {
    args_.AddFloat(q_name + "m", scale * 255.0f);
    args_.AddFloat(q_name + "a", -scale * (127.0 + zero_point));
  } else {
    args_.AddHalf(q_name + "m", half(scale * 255.0f));
    args_.AddHalf(q_name + "a", half(-scale * (127.0 + zero_point)));
  }
  args_.AddObject("weights" + std::to_string(index),
                  absl::make_unique<Texture2DDescriptor>(std::move(desc)));
}

FCFCAdd CreateFCFCAdd(const GpuInfo& gpu_info, const OperationDef& definition,
                      const FullyConnectedAttributes& attr0,
                      const FullyConnectedAttributes& attr1) {
  FCFCAdd result(definition, gpu_info);
  bool weights_are_buffer = UseBufferForWeights(gpu_info);
  result.UploadWeights(attr0.weights, "weights0", weights_are_buffer);
  result.UploadWeights(attr1.weights, "weights1", weights_are_buffer);
  result.code_ = result.GetFCFCAddKernelCode(definition, gpu_info,
                                             weights_are_buffer, false, false);

  TensorLinearDescriptor desc0;
  desc0.storage_type = LinearStorageType::TEXTURE_2D;
  desc0.element_type = definition.GetDataType();
  desc0.UploadLinearData(attr0.bias);
  result.args_.AddObject(
      "biases0", absl::make_unique<TensorLinearDescriptor>(std::move(desc0)));

  TensorLinearDescriptor desc1;
  desc1.storage_type = LinearStorageType::TEXTURE_2D;
  desc1.element_type = definition.GetDataType();
  desc1.UploadLinearData(attr1.bias);
  result.args_.AddObject(
      "biases1", absl::make_unique<TensorLinearDescriptor>(std::move(desc1)));

  return result;
}

FCFCAdd CreateFCFCAdd(const GpuInfo& gpu_info, const OperationDef& definition,
                      const FullyConnectedInt8Attributes& attr0,
                      const FullyConnectedInt8Attributes& attr1) {
  FCFCAdd result(definition, gpu_info);
  result.UploadQuantizedWeights(attr0.weights, attr0.scale, attr0.zero_point,
                                0);
  result.UploadQuantizedWeights(attr1.weights, attr1.scale, attr1.zero_point,
                                1);
  result.code_ =
      result.GetFCFCAddKernelCode(definition, gpu_info, false, true, true);

  TensorLinearDescriptor desc0;
  desc0.storage_type = LinearStorageType::TEXTURE_2D;
  desc0.element_type = definition.GetDataType();
  desc0.UploadLinearData(attr0.bias);
  result.args_.AddObject(
      "biases0", absl::make_unique<TensorLinearDescriptor>(std::move(desc0)));

  TensorLinearDescriptor desc1;
  desc1.storage_type = LinearStorageType::TEXTURE_2D;
  desc1.element_type = definition.GetDataType();
  desc1.UploadLinearData(attr1.bias);
  result.args_.AddObject(
      "biases1", absl::make_unique<TensorLinearDescriptor>(std::move(desc1)));

  return result;
}

}  // namespace gpu
}  // namespace tflite
