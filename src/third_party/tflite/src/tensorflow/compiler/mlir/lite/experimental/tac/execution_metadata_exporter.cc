// Copyright 2020 The TensorFlow Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tensorflow/compiler/mlir/lite/experimental/tac/execution_metadata_exporter.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "flatbuffers/flatbuffers.h"  // from @flatbuffers
#include "llvm/ADT/None.h"
#include "llvm/ADT/Optional.h"
#include "llvm/Support/Casting.h"
#include "mlir/Dialect/Arithmetic/IR/Arithmetic.h"  // from @llvm-project
#include "mlir/Dialect/StandardOps/IR/Ops.h"  // from @llvm-project
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/IR/Operation.h"  // from @llvm-project
#include "mlir/IR/Region.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/lite/experimental/tac/common/targets.h"
#include "tensorflow/compiler/mlir/lite/experimental/tac/hardwares/target_hardware.h"
#include "tensorflow/compiler/mlir/lite/experimental/tac/runtime_metadata_generated.h"
#include "tensorflow/compiler/mlir/lite/ir/tfl_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"

namespace tflite {
namespace {

bool IsConst(mlir::Operation* op) {
  return llvm::isa<mlir::arith::ConstantOp, mlir::TF::ConstOp,
                   mlir::TFL::ConstOp, mlir::TFL::QConstOp>(op);
}

bool IsOpSupported(mlir::Operation* op, const std::string& hardware) {
  auto* devce_hardware = mlir::TFL::tac::GetTargetHardware(hardware);
  if (devce_hardware == nullptr) return {};
  return devce_hardware->IsOpSupported(op);
}

bool HasValidHardwareTarget(mlir::Operation* op) {
  // All TFLite ops has CPU interface, should be enough to check for cpu.
  return IsOpSupported(op, "CPU");
}

llvm::Optional<std::string> GetDeviceName(mlir::Operation* op) {
  if (IsConst(op)) return llvm::None;

  // The model may contain quant stats op which is unrelevant to the
  // execution.
  if (llvm::isa<mlir::ReturnOp, mlir::quant::StatisticsOp>(op))
    return llvm::None;

  if (!HasValidHardwareTarget(op)) return llvm::None;

  auto device = op->getAttrOfType<mlir::StringAttr>(mlir::TFL::tac::kDevice);
  if (device == nullptr) return llvm::None;

  llvm::StringRef device_name_str = device.getValue();
  return device_name_str.str();
}

llvm::Optional<std::vector<float>> GetPerDeviceCosts(
    const std::map<std::string, uint8_t>& hardware_map, mlir::Operation* op) {
  auto device_costs_attr =
      op->getAttrOfType<mlir::DictionaryAttr>("per_device_costs");
  if (device_costs_attr == nullptr) return llvm::None;

  std::vector<float> device_costs(hardware_map.size(), -1.f);

  for (const auto& kv : hardware_map) {
    auto cost_attr = device_costs_attr.getNamed(kv.first);
    if (!cost_attr.hasValue()) return llvm::None;
    float cost = cost_attr->getValue()
                     .dyn_cast_or_null<mlir::FloatAttr>()
                     .getValueAsDouble();
    device_costs[kv.second] = cost;
  }
  return device_costs;
}

flatbuffers::Offset<SubgraphMetadata> CreateSubgraphMetadata(
    const std::map<std::string, uint8_t>& hardware_map, mlir::Region* Region,
    flatbuffers::FlatBufferBuilder* builder) {
  auto& block = Region->front();
  int index = 0;
  std::vector<flatbuffers::Offset<tflite::OpMetadata>> ops;
  for (auto& inst : block) {
    // Const nodes are mapped to const vectors in flatbuffer, so skip.
    if (IsConst(&inst)) continue;

    // The model may contain quant stats op which is unrelevant to the
    // execution.
    if (llvm::isa<mlir::ReturnOp, mlir::quant::StatisticsOp>(&inst)) continue;

    // If an op doesn't implement any of the hardware interface we skip it.
    // This can happen in cases like Flex when we have non TFLite ops.
    auto device_name = GetDeviceName(&inst);

    if (device_name.hasValue()) {
      // Add per device costs if present.
      auto per_device_cost = GetPerDeviceCosts(hardware_map, &inst);
      flatbuffers::Offset<flatbuffers::Vector<float>> per_device_cost_offset;

      if (per_device_cost.hasValue()) {
        per_device_cost_offset =
            builder->CreateVector(per_device_cost.getValue());
      }

      OpMetadataBuilder op_builder(*builder);
      op_builder.add_index(index);
      uint8_t hardware = hardware_map.at(device_name.getValue());
      op_builder.add_hardware(hardware);

      if (per_device_cost.hasValue()) {
        op_builder.add_op_costs(per_device_cost_offset);
      }

      ops.push_back(op_builder.Finish());
    }
    index++;
  }
  return CreateSubgraphMetadata(*builder, builder->CreateVector(ops));
}

flatbuffers::Offset<tflite::HardwareMetadata>
CreateHardwareMetadataAndPopulateLookupTable(
    std::vector<mlir::FuncOp>* funcs, flatbuffers::FlatBufferBuilder* builder,
    std::map<std::string, uint8_t>* hardware_names) {
  uint8_t index = 0;
  for (auto& func : *funcs) {
    func.walk([&hardware_names, &index](mlir::Operation* op) {
      auto device_name = GetDeviceName(op);
      if (!device_name.hasValue()) return;

      auto iter = hardware_names->find(device_name.getValue());
      if (iter == hardware_names->end()) {
        hardware_names->insert({device_name.getValue(), index++});
      }
    });
  }

  // Build the flatbuffer.
  std::vector<flatbuffers::Offset<flatbuffers::String>> hardwares;
  for (const auto& kv : *hardware_names) {
    hardwares.push_back(builder->CreateString(kv.first));
  }

  return CreateHardwareMetadata(*builder, builder->CreateVector(hardwares));
}

}  // namespace

llvm::Optional<std::string> ExportRuntimeMetadata(mlir::ModuleOp module) {
  mlir::FuncOp main_fn = module.lookupSymbol<mlir::FuncOp>("main");
  if (!main_fn) return std::string("");

  flatbuffers::FlatBufferBuilder fb_builder;
  std::vector<mlir::FuncOp> funcs;
  funcs.push_back(main_fn);
  module.walk([&](mlir::FuncOp fn) {
    if (fn != main_fn) {
      funcs.push_back(fn);
    }
  });

  // Populate the hardware metadata.
  // And collect the hardwares used.
  std::map<std::string, uint8_t> hardware_map;
  flatbuffers::Offset<tflite::HardwareMetadata> hardware_metadata_offset =
      CreateHardwareMetadataAndPopulateLookupTable(&funcs, &fb_builder,
                                                   &hardware_map);

  // Populate the runtime metadata.
  std::vector<flatbuffers::Offset<SubgraphMetadata>> subgraphs_metadata;
  subgraphs_metadata.reserve(funcs.size());
  for (auto& func : funcs) {
    subgraphs_metadata.push_back(
        CreateSubgraphMetadata(hardware_map, &func.getBody(), &fb_builder));
  }
  auto runtime_metadata =
      CreateRuntimeMetadata(fb_builder, hardware_metadata_offset,
                            fb_builder.CreateVector(subgraphs_metadata));
  fb_builder.Finish(runtime_metadata);
  return std::string(
      reinterpret_cast<const char*>(fb_builder.GetBufferPointer()),
      fb_builder.GetSize());
}
}  // namespace tflite
