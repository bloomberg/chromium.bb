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

#include "tensorflow/lite/delegates/gpu/common/task/serialization_base.h"

#include <cstdint>
#include <set>
#include <string>
#include <utility>

#include "tensorflow/lite/delegates/gpu/common/model.h"
#include "tensorflow/lite/delegates/gpu/common/precision.h"
#include "tensorflow/lite/delegates/gpu/common/task/arguments.h"
#include "tensorflow/lite/delegates/gpu/common/task/buffer_desc.h"
#include "tensorflow/lite/delegates/gpu/common/task/gpu_object_desc.h"
#include "tensorflow/lite/delegates/gpu/common/task/gpu_operation.h"
#include "tensorflow/lite/delegates/gpu/common/task/serialization_base_generated.h"
#include "tensorflow/lite/delegates/gpu/common/task/tensor_desc.h"
#include "tensorflow/lite/delegates/gpu/common/task/tensor_linear_desc.h"
#include "tensorflow/lite/delegates/gpu/common/task/texture2d_desc.h"

namespace tflite {
namespace gpu {

namespace {
data::AccessType ToFB(AccessType type) {
  switch (type) {
    case AccessType::READ:
      return data::AccessType::READ;
    case AccessType::WRITE:
      return data::AccessType::WRITE;
    case AccessType::READ_WRITE:
      return data::AccessType::READ_WRITE;
    default:
      return data::AccessType::READ_WRITE;
  }
}

data::DataType ToFB(DataType type) {
  switch (type) {
    case DataType::FLOAT16:
      return data::DataType::FLOAT16;
    case DataType::FLOAT32:
      return data::DataType::FLOAT32;
    case DataType::FLOAT64:
      return data::DataType::FLOAT64;
    case DataType::UINT8:
      return data::DataType::UINT8;
    case DataType::INT8:
      return data::DataType::INT8;
    case DataType::UINT16:
      return data::DataType::UINT16;
    case DataType::INT16:
      return data::DataType::INT16;
    case DataType::UINT32:
      return data::DataType::UINT32;
    case DataType::INT32:
      return data::DataType::INT32;
    case DataType::UINT64:
      return data::DataType::UINT64;
    case DataType::INT64:
      return data::DataType::INT64;
    case DataType::UNKNOWN:
      return data::DataType::UNKNOWN;
  }
}

data::MemoryType ToFB(MemoryType type) {
  switch (type) {
    case MemoryType::CONSTANT:
      return data::MemoryType::CONSTANT;
    case MemoryType::GLOBAL:
      return data::MemoryType::GLOBAL;
    case MemoryType::LOCAL:
      return data::MemoryType::LOCAL;
  }
}

data::LinearStorageType ToFB(LinearStorageType type) {
  switch (type) {
    case LinearStorageType::BUFFER:
      return data::LinearStorageType::BUFFER;
    case LinearStorageType::TEXTURE_2D:
      return data::LinearStorageType::TEXTURE_2D;
  }
}

data::TensorStorageType ToFB(TensorStorageType type) {
  switch (type) {
    case TensorStorageType::BUFFER:
      return data::TensorStorageType::BUFFER;
    case TensorStorageType::IMAGE_BUFFER:
      return data::TensorStorageType::IMAGE_BUFFER;
    case TensorStorageType::TEXTURE_2D:
      return data::TensorStorageType::TEXTURE_2D;
    case TensorStorageType::TEXTURE_ARRAY:
      return data::TensorStorageType::TEXTURE_ARRAY;
    case TensorStorageType::TEXTURE_3D:
      return data::TensorStorageType::TEXTURE_3D;
    case TensorStorageType::SINGLE_TEXTURE_2D:
      return data::TensorStorageType::SINGLE_TEXTURE_2D;
    case TensorStorageType::UNKNOWN:
      return data::TensorStorageType::UNKNOWN;
  }
}

data::Layout ToFB(Layout type) {
  switch (type) {
    case Layout::HWC:
      return data::Layout::HWC;
    case Layout::BHWC:
      return data::Layout::BHWC;
    case Layout::HWDC:
      return data::Layout::HWDC;
    case Layout::BHWDC:
      return data::Layout::BHWDC;
    default:
      return data::Layout::UNKNOWN;
  }
}

DataType ToEnum(data::DataType type) {
  switch (type) {
    case data::DataType::FLOAT16:
      return DataType::FLOAT16;
    case data::DataType::FLOAT32:
      return DataType::FLOAT32;
    case data::DataType::FLOAT64:
      return DataType::FLOAT64;
    case data::DataType::UINT8:
      return DataType::UINT8;
    case data::DataType::INT8:
      return DataType::INT8;
    case data::DataType::UINT16:
      return DataType::UINT16;
    case data::DataType::INT16:
      return DataType::INT16;
    case data::DataType::UINT32:
      return DataType::UINT32;
    case data::DataType::INT32:
      return DataType::INT32;
    case data::DataType::UINT64:
      return DataType::UINT64;
    case data::DataType::INT64:
      return DataType::INT64;
    case data::DataType::UNKNOWN:
      return DataType::UNKNOWN;
  }
}

AccessType ToEnum(data::AccessType type) {
  switch (type) {
    case data::AccessType::READ:
      return AccessType::READ;
    case data::AccessType::WRITE:
      return AccessType::WRITE;
    case data::AccessType::READ_WRITE:
      return AccessType::READ_WRITE;
  }
}

MemoryType ToEnum(data::MemoryType type) {
  switch (type) {
    case data::MemoryType::CONSTANT:
      return MemoryType::CONSTANT;
    case data::MemoryType::GLOBAL:
      return MemoryType::GLOBAL;
    case data::MemoryType::LOCAL:
      return MemoryType::LOCAL;
  }
}

LinearStorageType ToEnum(data::LinearStorageType type) {
  switch (type) {
    case data::LinearStorageType::BUFFER:
      return LinearStorageType::BUFFER;
    case data::LinearStorageType::TEXTURE_2D:
      return LinearStorageType::TEXTURE_2D;
  }
}

TensorStorageType ToEnum(data::TensorStorageType type) {
  switch (type) {
    case data::TensorStorageType::BUFFER:
      return TensorStorageType::BUFFER;
    case data::TensorStorageType::IMAGE_BUFFER:
      return TensorStorageType::IMAGE_BUFFER;
    case data::TensorStorageType::TEXTURE_2D:
      return TensorStorageType::TEXTURE_2D;
    case data::TensorStorageType::TEXTURE_ARRAY:
      return TensorStorageType::TEXTURE_ARRAY;
    case data::TensorStorageType::TEXTURE_3D:
      return TensorStorageType::TEXTURE_3D;
    case data::TensorStorageType::SINGLE_TEXTURE_2D:
      return TensorStorageType::SINGLE_TEXTURE_2D;
    case data::TensorStorageType::UNKNOWN:
      return TensorStorageType::UNKNOWN;
  }
}

Layout ToEnum(data::Layout type) {
  switch (type) {
    case data::Layout::HWC:
      return Layout::HWC;
    case data::Layout::BHWC:
      return Layout::BHWC;
    case data::Layout::HWDC:
      return Layout::HWDC;
    case data::Layout::BHWDC:
      return Layout::BHWDC;
    default:
      return Layout::UNKNOWN;
  }
}

data::CalculationsPrecision ToFB(CalculationsPrecision type) {
  switch (type) {
    case CalculationsPrecision::F32:
      return data::CalculationsPrecision::F32;
    case CalculationsPrecision::F32_F16:
      return data::CalculationsPrecision::F32_F16;
    case CalculationsPrecision::F16:
      return data::CalculationsPrecision::F16;
  }
}

data::TensorToGrid ToFB(TensorToGrid type) {
  switch (type) {
    case TensorToGrid::kCustom:
      return data::TensorToGrid::CUSTOM;
    case TensorToGrid::kWBToX_HDToY_SToZ:
      return data::TensorToGrid::WB_TO_X_HD_TO_Y_S_TO_Z;
    case TensorToGrid::kWBToX_HDToY_ZIs1:
      return data::TensorToGrid::WB_TO_X_HD_TO_Y_Z_IS_1;
    case TensorToGrid::kWBToX_HToY_DToZ:
      return data::TensorToGrid::WB_TO_X_H_TO_Y_D_TO_Z;
    case TensorToGrid::kBToX_YIs1_ZIs1:
      return data::TensorToGrid::B_TO_X_Y_IS_1_Z_IS_1;
  }
}

data::CompilerOptions ToFB(CompilerOptions type) {
  switch (type) {
    case CompilerOptions::kAdrenoFullSimd:
      return data::CompilerOptions::ADRENO_FULL_SIMD_LINE;
    case CompilerOptions::kAdrenoMoreWaves:
      return data::CompilerOptions::ADRENO_MORE_WAVES;
    case CompilerOptions::kClFastRelaxedMath:
      return data::CompilerOptions::CL_FAST_RELAXED_MATH;
    case CompilerOptions::kClDisableOptimizations:
      return data::CompilerOptions::CL_OPT_DISABLE;
    case CompilerOptions::kCl20:
      return data::CompilerOptions::CL_2_0;
    case CompilerOptions::kCl30:
      return data::CompilerOptions::CL_3_0;
  }
}

CalculationsPrecision ToEnum(data::CalculationsPrecision type) {
  switch (type) {
    case data::CalculationsPrecision::F32:
      return CalculationsPrecision::F32;
    case data::CalculationsPrecision::F32_F16:
      return CalculationsPrecision::F32_F16;
    case data::CalculationsPrecision::F16:
      return CalculationsPrecision::F16;
  }
}

TensorToGrid ToEnum(data::TensorToGrid type) {
  switch (type) {
    case data::TensorToGrid::CUSTOM:
      return TensorToGrid::kCustom;
    case data::TensorToGrid::WB_TO_X_HD_TO_Y_S_TO_Z:
      return TensorToGrid::kWBToX_HDToY_SToZ;
    case data::TensorToGrid::WB_TO_X_HD_TO_Y_Z_IS_1:
      return TensorToGrid::kWBToX_HDToY_ZIs1;
    case data::TensorToGrid::WB_TO_X_H_TO_Y_D_TO_Z:
      return TensorToGrid::kWBToX_HToY_DToZ;
    case data::TensorToGrid::B_TO_X_Y_IS_1_Z_IS_1:
      return TensorToGrid::kBToX_YIs1_ZIs1;
  }
}

CompilerOptions ToEnum(data::CompilerOptions type) {
  switch (type) {
    case data::CompilerOptions::ADRENO_FULL_SIMD_LINE:
      return CompilerOptions::kAdrenoFullSimd;
    case data::CompilerOptions::ADRENO_MORE_WAVES:
      return CompilerOptions::kAdrenoMoreWaves;
    case data::CompilerOptions::CL_FAST_RELAXED_MATH:
      return CompilerOptions::kClFastRelaxedMath;
    case data::CompilerOptions::CL_OPT_DISABLE:
      return CompilerOptions::kClDisableOptimizations;
    case data::CompilerOptions::CL_2_0:
      return CompilerOptions::kCl20;
    case data::CompilerOptions::CL_3_0:
      return CompilerOptions::kCl30;
  }
}

}  // namespace

flatbuffers::Offset<data::Int2> Encode(
    const int2& v, flatbuffers::FlatBufferBuilder* builder) {
  data::Int2Builder int2_builder(*builder);
  int2_builder.add_x(v.x);
  int2_builder.add_y(v.y);
  return int2_builder.Finish();
}

flatbuffers::Offset<data::Int3> Encode(
    const int3& v, flatbuffers::FlatBufferBuilder* builder) {
  data::Int3Builder int3_builder(*builder);
  int3_builder.add_x(v.x);
  int3_builder.add_y(v.y);
  int3_builder.add_z(v.z);
  return int3_builder.Finish();
}

flatbuffers::Offset<data::GPUObjectDescriptor> Encode(
    const GPUObjectDescriptor& desc, flatbuffers::FlatBufferBuilder* builder) {
  std::vector<flatbuffers::Offset<data::StateVariable>> state_vars_fb;
  for (auto& v0 : desc.state_vars_) {
    auto key_fb = builder->CreateString(v0.first);
    auto value_fb = builder->CreateString(v0.second);
    data::StateVariableBuilder state_builder(*builder);
    state_builder.add_key(key_fb);
    state_builder.add_value(value_fb);
    state_vars_fb.push_back(state_builder.Finish());
  }
  auto state_vars_fb_vec = builder->CreateVector(state_vars_fb);
  data::GPUObjectDescriptorBuilder obj_builder(*builder);
  obj_builder.add_state_vars(state_vars_fb_vec);
  obj_builder.add_access_type(ToFB(desc.access_type_));
  return obj_builder.Finish();
}

void Decode(const data::GPUObjectDescriptor* fb_obj, GPUObjectDescriptor* obj) {
  obj->access_type_ = ToEnum(fb_obj->access_type());
  for (auto state_fb : *fb_obj->state_vars()) {
    std::string key(state_fb->key()->c_str(), state_fb->key()->size());
    std::string value(state_fb->value()->c_str(), state_fb->value()->size());
    obj->state_vars_[key] = value;
  }
}

flatbuffers::Offset<data::BufferDescriptor> Encode(
    const BufferDescriptor& desc, flatbuffers::FlatBufferBuilder* builder) {
  auto obj_fb =
      Encode(*static_cast<const GPUObjectDescriptor*>(&desc), builder);

  std::vector<flatbuffers::Offset<flatbuffers::String>> attributes_fb;
  for (auto& attr : desc.attributes) {
    attributes_fb.push_back(builder->CreateString(attr));
  }
  auto attributes_fb_vec = builder->CreateVector(attributes_fb);
  auto data_fb = builder->CreateVector(desc.data);
  data::BufferDescriptorBuilder buf_builder(*builder);
  buf_builder.add_base_obj(obj_fb);
  buf_builder.add_element_type(ToFB(desc.element_type));
  buf_builder.add_element_size(desc.element_size);
  buf_builder.add_memory_type(ToFB(desc.memory_type));
  buf_builder.add_attributes(attributes_fb_vec);
  buf_builder.add_size(desc.size);
  buf_builder.add_data(data_fb);
  return buf_builder.Finish();
}

void Decode(const data::BufferDescriptor* fb_desc, BufferDescriptor* desc) {
  Decode(fb_desc->base_obj(), desc);
  desc->element_type = ToEnum(fb_desc->element_type());
  desc->element_size = fb_desc->element_size();
  desc->memory_type = ToEnum(fb_desc->memory_type());
  for (auto attr_fb : *fb_desc->attributes()) {
    std::string attr(attr_fb->c_str(), attr_fb->size());
    desc->attributes.push_back(attr);
  }
  desc->size = fb_desc->size();
  desc->data =
      std::vector<uint8_t>(fb_desc->data()->data(),
                           fb_desc->data()->data() + fb_desc->data()->size());
}

flatbuffers::Offset<data::Texture2DDescriptor> Encode(
    const Texture2DDescriptor& desc, flatbuffers::FlatBufferBuilder* builder) {
  auto obj_fb =
      Encode(*static_cast<const GPUObjectDescriptor*>(&desc), builder);

  auto data_fb = builder->CreateVector(desc.data);
  auto size_fb = Encode(desc.size, builder);
  data::Texture2DDescriptorBuilder tex_builder(*builder);
  tex_builder.add_base_obj(obj_fb);
  tex_builder.add_element_type(ToFB(desc.element_type));
  tex_builder.add_normalized(desc.normalized);
  tex_builder.add_normalized_type(ToFB(desc.normalized_type));
  tex_builder.add_size(size_fb);
  tex_builder.add_data(data_fb);
  return tex_builder.Finish();
}

void Decode(const data::Texture2DDescriptor* fb_desc,
            Texture2DDescriptor* desc) {
  Decode(fb_desc->base_obj(), desc);
  desc->element_type = ToEnum(fb_desc->element_type());
  desc->normalized = fb_desc->normalized();
  desc->normalized_type = ToEnum(fb_desc->normalized_type());
  desc->size.x = fb_desc->size()->x();
  desc->size.y = fb_desc->size()->y();
  desc->data =
      std::vector<uint8_t>(fb_desc->data()->data(),
                           fb_desc->data()->data() + fb_desc->data()->size());
}

flatbuffers::Offset<data::TensorLinearDescriptor> Encode(
    const TensorLinearDescriptor& desc,
    flatbuffers::FlatBufferBuilder* builder) {
  auto obj_fb =
      Encode(*static_cast<const GPUObjectDescriptor*>(&desc), builder);

  auto data_fb = builder->CreateVector(desc.data);
  data::TensorLinearDescriptorBuilder tensor_builder(*builder);
  tensor_builder.add_base_obj(obj_fb);
  tensor_builder.add_element_type(ToFB(desc.element_type));
  tensor_builder.add_storage_type(ToFB(desc.storage_type));
  tensor_builder.add_memory_type(ToFB(desc.memory_type));
  tensor_builder.add_size(desc.size);
  tensor_builder.add_data(data_fb);
  return tensor_builder.Finish();
}

void Decode(const data::TensorLinearDescriptor* fb_desc,
            TensorLinearDescriptor* desc) {
  Decode(fb_desc->base_obj(), desc);
  desc->element_type = ToEnum(fb_desc->element_type());
  desc->storage_type = ToEnum(fb_desc->storage_type());
  desc->memory_type = ToEnum(fb_desc->memory_type());
  desc->size = fb_desc->size();
  desc->data =
      std::vector<uint8_t>(fb_desc->data()->data(),
                           fb_desc->data()->data() + fb_desc->data()->size());
}

flatbuffers::Offset<data::TensorDescriptor> Encode(
    const TensorDescriptor& desc, flatbuffers::FlatBufferBuilder* builder) {
  auto obj_fb =
      Encode(*static_cast<const GPUObjectDescriptor*>(&desc), builder);

  data::BHWDCBuilder shape_builder(*builder);
  shape_builder.add_b(desc.GetBHWDCShape().b);
  shape_builder.add_h(desc.GetBHWDCShape().h);
  shape_builder.add_w(desc.GetBHWDCShape().w);
  shape_builder.add_d(desc.GetBHWDCShape().d);
  shape_builder.add_c(desc.GetBHWDCShape().c);
  auto shape_fb = shape_builder.Finish();

  auto data_fb = builder->CreateVector(desc.GetData());
  data::TensorDescriptorBuilder tensor_builder(*builder);
  tensor_builder.add_base_obj(obj_fb);
  tensor_builder.add_data_type(ToFB(desc.data_type));
  tensor_builder.add_storage_type(ToFB(desc.storage_type));
  tensor_builder.add_layout(ToFB(desc.layout));
  tensor_builder.add_shape(shape_fb);
  tensor_builder.add_data(data_fb);
  tensor_builder.add_use_buffer_for_write_only_2d_texture(
      desc.use_buffer_for_write_only_2d_texture);
  tensor_builder.add_use_buffer_for_write_only_image_buffer(
      desc.use_buffer_for_write_only_image_buffer);
  return tensor_builder.Finish();
}

void Decode(const data::TensorDescriptor* fb_desc, TensorDescriptor* desc) {
  Decode(fb_desc->base_obj(), desc);
  desc->data_type = ToEnum(fb_desc->data_type());
  desc->storage_type = ToEnum(fb_desc->storage_type());
  desc->layout = ToEnum(fb_desc->layout());
  desc->SetBHWDCShape(BHWDC(fb_desc->shape()->b(), fb_desc->shape()->h(),
                            fb_desc->shape()->w(), fb_desc->shape()->d(),
                            fb_desc->shape()->c()));
  desc->SetData(
      std::vector<uint8_t>(fb_desc->data()->data(),
                           fb_desc->data()->data() + fb_desc->data()->size()));
  desc->use_buffer_for_write_only_2d_texture =
      fb_desc->use_buffer_for_write_only_2d_texture();
  desc->use_buffer_for_write_only_image_buffer =
      fb_desc->use_buffer_for_write_only_image_buffer();
}

absl::Status Decode(const data::Arguments* fb_args, Arguments* args) {
  args->int_values_.clear();
  for (auto int_values_fb : *fb_args->int_values()) {
    Arguments::IntValue value;
    value.value = int_values_fb->value();
    value.active = int_values_fb->active();
    std::string name(int_values_fb->name()->c_str(),
                     int_values_fb->name()->size());
    args->int_values_[name] = value;
  }

  args->float_values_.clear();
  for (auto float_values_fb : *fb_args->float_values()) {
    Arguments::FloatValue value;
    value.value = float_values_fb->value();
    value.active = float_values_fb->active();
    std::string name(float_values_fb->name()->c_str(),
                     float_values_fb->name()->size());
    args->float_values_[name] = value;
  }

  args->half_values_.clear();
  for (auto half_values_fb : *fb_args->half_values()) {
    Arguments::HalfValue value;
    value.value = half_values_fb->value();
    value.active = half_values_fb->active();
    std::string name(half_values_fb->name()->c_str(),
                     half_values_fb->name()->size());
    args->half_values_[name] = value;
  }

  for (auto buffer_pair_fb : *fb_args->buffer_objects()) {
    std::string key(buffer_pair_fb->key()->c_str(),
                    buffer_pair_fb->key()->size());
    BufferDescriptor desc;
    Decode(buffer_pair_fb->value(), &desc);
    args->AddObject(key, absl::make_unique<BufferDescriptor>(std::move(desc)));
  }

  for (auto texture_pair_fb : *fb_args->texture2d_objects()) {
    std::string key(texture_pair_fb->key()->c_str(),
                    texture_pair_fb->key()->size());
    Texture2DDescriptor desc;
    Decode(texture_pair_fb->value(), &desc);
    args->AddObject(key,
                    absl::make_unique<Texture2DDescriptor>(std::move(desc)));
  }

  for (auto tensor_pair_fb : *fb_args->tensor_linear_objects()) {
    std::string key(tensor_pair_fb->key()->c_str(),
                    tensor_pair_fb->key()->size());
    TensorLinearDescriptor desc;
    Decode(tensor_pair_fb->value(), &desc);
    args->AddObject(key,
                    absl::make_unique<TensorLinearDescriptor>(std::move(desc)));
  }

  for (auto tensor_pair_fb : *fb_args->tensor_objects()) {
    std::string key(tensor_pair_fb->key()->c_str(),
                    tensor_pair_fb->key()->size());
    TensorDescriptor desc;
    Decode(tensor_pair_fb->value(), &desc);
    args->AddObject(key, absl::make_unique<TensorDescriptor>(std::move(desc)));
  }

  for (auto buffer_pair_fb : *fb_args->buffer_refs()) {
    std::string key(buffer_pair_fb->key()->c_str(),
                    buffer_pair_fb->key()->size());
    BufferDescriptor desc;
    Decode(buffer_pair_fb->value(), &desc);
    auto access_type = desc.GetAccess();
    args->AddObjectRef(key, access_type,
                       absl::make_unique<BufferDescriptor>(std::move(desc)));
  }

  for (auto texture_pair_fb : *fb_args->texture2d_refs()) {
    std::string key(texture_pair_fb->key()->c_str(),
                    texture_pair_fb->key()->size());
    Texture2DDescriptor desc;
    Decode(texture_pair_fb->value(), &desc);
    auto access_type = desc.GetAccess();
    args->AddObjectRef(key, access_type,
                       absl::make_unique<Texture2DDescriptor>(std::move(desc)));
  }

  for (auto tensor_pair_fb : *fb_args->tensor_linear_refs()) {
    std::string key(tensor_pair_fb->key()->c_str(),
                    tensor_pair_fb->key()->size());
    TensorLinearDescriptor desc;
    Decode(tensor_pair_fb->value(), &desc);
    auto access_type = desc.GetAccess();
    args->AddObjectRef(
        key, access_type,
        absl::make_unique<TensorLinearDescriptor>(std::move(desc)));
  }

  for (auto tensor_pair_fb : *fb_args->tensor_refs()) {
    std::string key(tensor_pair_fb->key()->c_str(),
                    tensor_pair_fb->key()->size());
    TensorDescriptor desc;
    Decode(tensor_pair_fb->value(), &desc);
    auto access_type = desc.GetAccess();
    args->AddObjectRef(key, access_type,
                       absl::make_unique<TensorDescriptor>(std::move(desc)));
  }
  return absl::OkStatus();
}

flatbuffers::Offset<data::Arguments> Encode(
    const Arguments& args, flatbuffers::FlatBufferBuilder* builder) {
  std::vector<flatbuffers::Offset<data::IntValue>> int_values_fb;
  for (auto& value : args.int_values_) {
    auto name_fb = builder->CreateString(value.first);
    data::IntValueBuilder value_builder(*builder);
    value_builder.add_name(name_fb);
    value_builder.add_value(value.second.value);
    value_builder.add_active(value.second.active);
    int_values_fb.push_back(value_builder.Finish());
  }

  std::vector<flatbuffers::Offset<data::FloatValue>> float_values_fb;
  for (auto& value : args.float_values_) {
    auto name_fb = builder->CreateString(value.first);
    data::FloatValueBuilder value_builder(*builder);
    value_builder.add_name(name_fb);
    value_builder.add_value(value.second.value);
    value_builder.add_active(value.second.active);
    float_values_fb.push_back(value_builder.Finish());
  }

  std::vector<flatbuffers::Offset<data::HalfValue>> half_values_fb;
  for (auto& value : args.half_values_) {
    auto name_fb = builder->CreateString(value.first);
    data::HalfValueBuilder value_builder(*builder);
    value_builder.add_name(name_fb);
    value_builder.add_value(value.second.value);
    value_builder.add_active(value.second.active);
    half_values_fb.push_back(value_builder.Finish());
  }

  std::vector<flatbuffers::Offset<data::BufferDescriptorMapValue>>
      buffer_objs_fb;
  for (auto& value : args.objects_) {
    const auto* buffer_desc =
        dynamic_cast<const BufferDescriptor*>(value.second.get());
    if (!buffer_desc) continue;
    auto desc_fb = Encode(*buffer_desc, builder);
    auto key_fb = builder->CreateString(value.first);
    data::BufferDescriptorMapValueBuilder buf_map_builder(*builder);
    buf_map_builder.add_key(key_fb);
    buf_map_builder.add_value(desc_fb);
    buffer_objs_fb.push_back(buf_map_builder.Finish());
  }
  std::vector<flatbuffers::Offset<data::Texture2DDescriptorMapValue>>
      texture2d_objs_fb;
  for (auto& value : args.objects_) {
    const auto* texture_desc =
        dynamic_cast<const Texture2DDescriptor*>(value.second.get());
    if (!texture_desc) continue;
    auto desc_fb = Encode(*texture_desc, builder);
    auto key_fb = builder->CreateString(value.first);
    data::Texture2DDescriptorMapValueBuilder tex_map_builder(*builder);
    tex_map_builder.add_key(key_fb);
    tex_map_builder.add_value(desc_fb);
    texture2d_objs_fb.push_back(tex_map_builder.Finish());
  }
  std::vector<flatbuffers::Offset<data::TensorLinearDescriptorMapValue>>
      tensor_linear_objs_fb;
  for (auto& value : args.objects_) {
    const auto* tensor_desc =
        dynamic_cast<const TensorLinearDescriptor*>(value.second.get());
    if (!tensor_desc) continue;
    auto desc_fb = Encode(*tensor_desc, builder);
    auto key_fb = builder->CreateString(value.first);
    data::TensorLinearDescriptorMapValueBuilder ten_map_builder(*builder);
    ten_map_builder.add_key(key_fb);
    ten_map_builder.add_value(desc_fb);
    tensor_linear_objs_fb.push_back(ten_map_builder.Finish());
  }
  std::vector<flatbuffers::Offset<data::TensorDescriptorMapValue>>
      tensor_objs_fb;
  for (auto& value : args.objects_) {
    const auto* tensor_desc =
        dynamic_cast<const TensorDescriptor*>(value.second.get());
    if (!tensor_desc) continue;
    auto desc_fb = Encode(*tensor_desc, builder);
    auto key_fb = builder->CreateString(value.first);
    data::TensorDescriptorMapValueBuilder ten_map_builder(*builder);
    ten_map_builder.add_key(key_fb);
    ten_map_builder.add_value(desc_fb);
    tensor_objs_fb.push_back(ten_map_builder.Finish());
  }

  std::vector<flatbuffers::Offset<data::BufferDescriptorMapValue>>
      buffer_refs_fb;
  for (auto& value : args.object_refs_) {
    const auto* buffer_desc =
        dynamic_cast<const BufferDescriptor*>(value.second.get());
    if (!buffer_desc) continue;
    auto desc_fb = Encode(*buffer_desc, builder);
    auto key_fb = builder->CreateString(value.first);
    data::BufferDescriptorMapValueBuilder buf_map_builder(*builder);
    buf_map_builder.add_key(key_fb);
    buf_map_builder.add_value(desc_fb);
    buffer_refs_fb.push_back(buf_map_builder.Finish());
  }
  std::vector<flatbuffers::Offset<data::Texture2DDescriptorMapValue>>
      texture2d_refs_fb;
  for (auto& value : args.object_refs_) {
    const auto* texture_desc =
        dynamic_cast<const Texture2DDescriptor*>(value.second.get());
    if (!texture_desc) continue;
    auto desc_fb = Encode(*texture_desc, builder);
    auto key_fb = builder->CreateString(value.first);
    data::Texture2DDescriptorMapValueBuilder tex_map_builder(*builder);
    tex_map_builder.add_key(key_fb);
    tex_map_builder.add_value(desc_fb);
    texture2d_refs_fb.push_back(tex_map_builder.Finish());
  }
  std::vector<flatbuffers::Offset<data::TensorLinearDescriptorMapValue>>
      tensor_linear_refs_fb;
  for (auto& value : args.object_refs_) {
    const auto* tensor_desc =
        dynamic_cast<const TensorLinearDescriptor*>(value.second.get());
    if (!tensor_desc) continue;
    auto desc_fb = Encode(*tensor_desc, builder);
    auto key_fb = builder->CreateString(value.first);
    data::TensorLinearDescriptorMapValueBuilder ten_map_builder(*builder);
    ten_map_builder.add_key(key_fb);
    ten_map_builder.add_value(desc_fb);
    tensor_linear_refs_fb.push_back(ten_map_builder.Finish());
  }
  std::vector<flatbuffers::Offset<data::TensorDescriptorMapValue>>
      tensor_refs_fb;
  for (auto& value : args.object_refs_) {
    const auto* tensor_desc =
        dynamic_cast<const TensorDescriptor*>(value.second.get());
    if (!tensor_desc) continue;
    auto desc_fb = Encode(*tensor_desc, builder);
    auto key_fb = builder->CreateString(value.first);
    data::TensorDescriptorMapValueBuilder ten_map_builder(*builder);
    ten_map_builder.add_key(key_fb);
    ten_map_builder.add_value(desc_fb);
    tensor_refs_fb.push_back(ten_map_builder.Finish());
  }

  auto int_values_fb_vec = builder->CreateVector(int_values_fb);
  auto float_values_fb_vec = builder->CreateVector(float_values_fb);
  auto half_values_fb_vec = builder->CreateVector(half_values_fb);
  auto buffer_objs_fb_vec = builder->CreateVector(buffer_objs_fb);
  auto texture2d_objs_fb_vec = builder->CreateVector(texture2d_objs_fb);
  auto tensor_linear_objs_fb_vec = builder->CreateVector(tensor_linear_objs_fb);
  auto tensor_objs_fb_vec = builder->CreateVector(tensor_objs_fb);
  auto buffer_refs_fb_vec = builder->CreateVector(buffer_refs_fb);
  auto texture2d_refs_fb_vec = builder->CreateVector(texture2d_refs_fb);
  auto tensor_linear_refs_fb_vec = builder->CreateVector(tensor_linear_refs_fb);
  auto tensor_refs_fb_vec = builder->CreateVector(tensor_refs_fb);
  data::ArgumentsBuilder arguments_builder(*builder);
  arguments_builder.add_int_values(int_values_fb_vec);
  arguments_builder.add_float_values(float_values_fb_vec);
  arguments_builder.add_half_values(half_values_fb_vec);
  arguments_builder.add_buffer_objects(buffer_objs_fb_vec);
  arguments_builder.add_texture2d_objects(texture2d_objs_fb_vec);
  arguments_builder.add_tensor_linear_objects(tensor_linear_objs_fb_vec);
  arguments_builder.add_tensor_objects(tensor_objs_fb_vec);
  arguments_builder.add_buffer_refs(buffer_refs_fb_vec);
  arguments_builder.add_texture2d_refs(texture2d_refs_fb_vec);
  arguments_builder.add_tensor_linear_refs(tensor_linear_refs_fb_vec);
  arguments_builder.add_tensor_refs(tensor_refs_fb_vec);
  return arguments_builder.Finish();
}

flatbuffers::Offset<data::OperationDef> Encode(
    const OperationDef& def, flatbuffers::FlatBufferBuilder* builder) {
  std::vector<flatbuffers::Offset<tflite::gpu::data::TensorDescriptor>>
      src_tensors_fb;
  for (auto& desc : def.src_tensors) {
    auto desc_fb = Encode(desc, builder);
    src_tensors_fb.push_back(desc_fb);
  }

  std::vector<flatbuffers::Offset<tflite::gpu::data::TensorDescriptor>>
      dst_tensors_fb;
  for (auto& desc : def.dst_tensors) {
    auto desc_fb = Encode(desc, builder);
    dst_tensors_fb.push_back(desc_fb);
  }

  auto src_tensors_fb_vec = builder->CreateVector(src_tensors_fb);
  auto dst_tensors_fb_vec = builder->CreateVector(dst_tensors_fb);

  data::OperationDefBuilder def_builder(*builder);
  def_builder.add_precision(ToFB(def.precision));
  def_builder.add_src_tensors(src_tensors_fb_vec);
  def_builder.add_dst_tensors(dst_tensors_fb_vec);
  return def_builder.Finish();
}

void Decode(const data::OperationDef* fb_def, OperationDef* def) {
  for (auto src_fb : *fb_def->src_tensors()) {
    TensorDescriptor desc;
    Decode(src_fb, &desc);
    def->src_tensors.push_back(std::move(desc));
  }
  for (auto dst_fb : *fb_def->dst_tensors()) {
    TensorDescriptor desc;
    Decode(dst_fb, &desc);
    def->dst_tensors.push_back(std::move(desc));
  }
  def->precision = ToEnum(fb_def->precision());
}

absl::Status Decode(const data::GPUOperation* fb_op, GPUOperation* op) {
  RETURN_IF_ERROR(Decode(fb_op->arguments(), &op->args_));
  op->work_group_size_.x = fb_op->work_group_size()->x();
  op->work_group_size_.y = fb_op->work_group_size()->y();
  op->work_group_size_.z = fb_op->work_group_size()->z();
  op->tensor_to_grid_ = ToEnum(fb_op->tensor_to_grid());
  op->elementwise_ = fb_op->elementwise();
  op->linkable_ = fb_op->linkable();
  op->check_src_channels_size_ = fb_op->check_src_channels_size();
  op->flops_ = fb_op->flops();
  Decode(fb_op->definition(), &op->definition_);
  op->grid_dimension_ = fb_op->grid_dimension();
  op->work_group_launch_order_.x = fb_op->work_group_launch_order()->x();
  op->work_group_launch_order_.y = fb_op->work_group_launch_order()->y();
  op->work_group_launch_order_.z = fb_op->work_group_launch_order()->z();
  op->grid_size_.x = fb_op->grid_size()->x();
  op->grid_size_.y = fb_op->grid_size()->y();
  op->grid_size_.z = fb_op->grid_size()->z();
  for (auto name_fb : *fb_op->src_tensors_names()) {
    std::string name(name_fb->c_str(), name_fb->size());
    op->src_tensors_names_.push_back(std::move(name));
  }
  for (auto name_fb : *fb_op->dst_tensors_names()) {
    std::string name(name_fb->c_str(), name_fb->size());
    op->dst_tensors_names_.push_back(std::move(name));
  }
  op->work_groups_count_.x = fb_op->work_groups_count()->x();
  op->work_groups_count_.y = fb_op->work_groups_count()->y();
  op->work_groups_count_.z = fb_op->work_groups_count()->z();
  op->linkable_count_ = fb_op->linkable_count();
  op->CalculateConstArgsSize();
  return absl::OkStatus();
}

flatbuffers::Offset<data::GPUOperation> Encode(
    const GPUOperation& op, flatbuffers::FlatBufferBuilder* builder) {
  auto args_fb = Encode(op.args_, builder);
  auto work_group_size_fb = Encode(op.work_group_size_, builder);

  auto def_fb = Encode(op.definition_, builder);
  auto work_group_launch_order_fb =
      Encode(op.work_group_launch_order_, builder);
  auto grid_size_fb = Encode(op.grid_size_, builder);
  auto work_groups_count_fb = Encode(op.work_groups_count_, builder);

  std::vector<flatbuffers::Offset<flatbuffers::String>> src_names_fb;
  for (auto& name : op.src_tensors_names_) {
    src_names_fb.push_back(builder->CreateString(name));
  }
  auto src_names_fb_vec = builder->CreateVector(src_names_fb);

  std::vector<flatbuffers::Offset<flatbuffers::String>> dst_names_fb;
  for (auto& name : op.dst_tensors_names_) {
    dst_names_fb.push_back(builder->CreateString(name));
  }
  auto dst_names_fb_vec = builder->CreateVector(dst_names_fb);

  data::GPUOperationBuilder op_builder(*builder);
  op_builder.add_arguments(args_fb);
  op_builder.add_work_group_size(work_group_size_fb);
  op_builder.add_tensor_to_grid(ToFB(op.tensor_to_grid_));
  op_builder.add_elementwise(op.elementwise_);
  op_builder.add_linkable(op.linkable_);
  op_builder.add_check_src_channels_size(op.check_src_channels_size_);
  op_builder.add_flops(op.flops_);
  op_builder.add_definition(def_fb);
  op_builder.add_grid_dimension(op.grid_dimension_);
  op_builder.add_work_group_launch_order(work_group_launch_order_fb);
  op_builder.add_grid_size(grid_size_fb);
  op_builder.add_src_tensors_names(src_names_fb_vec);
  op_builder.add_dst_tensors_names(dst_names_fb_vec);
  op_builder.add_work_groups_count(work_groups_count_fb);
  op_builder.add_linkable_count(op.linkable_count_);
  return op_builder.Finish();
}

}  // namespace gpu
}  // namespace tflite
