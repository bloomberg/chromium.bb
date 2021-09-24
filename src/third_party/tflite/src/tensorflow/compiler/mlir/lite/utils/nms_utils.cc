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

#include "tensorflow/compiler/mlir/lite/utils/nms_utils.h"

#include "mlir/Dialect/StandardOps/IR/Ops.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/lite/ir/tfl_ops.h"

namespace mlir {
namespace TFL {

namespace {

// TODO(b/162842801): Consolidate all util definitions of kTFImplements.
constexpr char kTFImplements[] = "tf._implements";
constexpr char kCustomSSDPostprocessing[] = "TFLite_Detection_PostProcess";
constexpr char kTfNMSPadded[] = "non_max_suppression_padded_v2";

inline OpaqueElementsAttr CustomOption(OpBuilder* builder,
                                       const std::string& content) {
  ShapedType type = RankedTensorType::get(
      {static_cast<int64_t>(content.size())}, builder->getIntegerType(8));
  return OpaqueElementsAttr::get(builder->getContext()->getLoadedDialect("tfl"),
                                 type,
                                 StringRef(content.data(), content.size()));
}

}  // namespace

void ConvertNMSPaddedFunc::RewriteFunc() {
  func_->setAttr(kTFImplements,
                 StringAttr::get(func_.getContext(), kTfNMSPadded));
  Value boxes = func_.getArgument(0);
  Value scores = func_.getArgument(1);
  Value max_output_size = func_.getArgument(2);
  Value iou_threshold = func_.getArgument(3);
  Value score_threshold = func_.getArgument(4);
  auto output_type0 = func_.getType().getResult(0);
  auto output_type1 = func_.getType().getResult(1);

  OpBuilder builder(func_.getBody());
  auto op = builder.create<mlir::TFL::NonMaxSuppressionV4Op>(
      func_.getLoc(), output_type0, output_type1, boxes, scores,
      max_output_size, iou_threshold, score_threshold);

  builder.create<mlir::ReturnOp>(func_.getLoc(), op.getResults());
}

LogicalResult ConvertNMSPaddedFunc::VerifySignature() {
  // Verify high-level function signature.
  // Relevant argument characteristics are checked by the TFL op definition.
  if (func_.getNumArguments() < 5) {
    return func_.emitWarning()
           << "Invalid number of arguments to "
              "non_max_suppression_padded_v2 (need at least 5): "
           << func_.getNumArguments();
  }
  if (func_.getType().getNumResults() != 2) {
    return func_.emitWarning() << "Invalid number of results from "
                                  "non_max_suppression_padded_v2 (need 2): "
                               << func_.getType().getNumResults();
  }
  // The TFLite fused op does not support batching yet.
  // TODO(b/158709815): Add support for batches with padded NMS.
  auto boxes_type = func_.getType().getInput(0).dyn_cast<RankedTensorType>();
  if (boxes_type == nullptr || !boxes_type.hasRank() ||
      boxes_type.getRank() != 2) {
    return func_.emitWarning() << "TFLite does not support batched input for "
                                  "non_max_suppression_padded";
  }
  return success();
}

LogicalResult ConvertSSDPostProcessFunc::RewriteFunc() {
  func_.eraseBody();
  func_.addEntryBlock();
  func_->setAttr(kTFImplements,
                 StringAttr::get(func_.getContext(), kCustomSSDPostprocessing));

  OpBuilder builder(func_.getBody());
  std::string custom_option_buffer;
  if (failed(CreateNMSCustomOptions(func_, attr_.getAttrs(),
                                    custom_option_buffer))) {
    return failure();
  }
  auto op = builder.create<CustomOp>(
      func_.getLoc(), func_.getType().getResults(), func_.getArguments(),
      kCustomSSDPostprocessing, CustomOption(&builder, custom_option_buffer));
  builder.create<ReturnOp>(func_.getLoc(), op.getResults());

  return success();
}

LogicalResult ConvertSSDPostProcessFunc::CreateNMSCustomOptions(
    FuncOp func, DictionaryAttr attrs, std::string& custom_option_buffer) {
  flexbuffers::Builder fbb;
  size_t start_map = fbb.StartMap();

  if (failed(AddIntAttr(func, attrs, "max_detections", &fbb)) ||
      failed(AddIntAttr(func, attrs, "max_classes_per_detection", &fbb)) ||
      failed(AddIntAttr(func, attrs, "num_classes", &fbb)) ||
      failed(AddFloatAttr(func, attrs, "nms_score_threshold", &fbb)) ||
      failed(AddFloatAttr(func, attrs, "nms_iou_threshold", &fbb)) ||
      failed(AddFloatAttr(func, attrs, "y_scale", &fbb)) ||
      failed(AddFloatAttr(func, attrs, "x_scale", &fbb)) ||
      failed(AddFloatAttr(func, attrs, "h_scale", &fbb)) ||
      failed(AddFloatAttr(func, attrs, "w_scale", &fbb)))
    return failure();
  auto use_regular_nms =
      attrs.get("use_regular_nms").dyn_cast_or_null<BoolAttr>();
  if (!use_regular_nms) {
    return func.emitError()
           << "use_regular_nms attribute is not set or not a bool";
  }
  fbb.Int("use_regular_nms", use_regular_nms.getValue());

  fbb.EndMap(start_map);
  fbb.Finish();
  custom_option_buffer.assign(fbb.GetBuffer().begin(), fbb.GetBuffer().end());
  return success();
}

LogicalResult ConvertSSDPostProcessFunc::AddIntAttr(
    FuncOp func, DictionaryAttr attrs, const std::string& attribute,
    flexbuffers::Builder* builder) {
  auto int_attr = attrs.get(attribute).dyn_cast_or_null<IntegerAttr>();
  if (!int_attr) {
    return func.emitError()
           << attribute.c_str() << " attribute is not set or not an integer";
  }
  builder->Int(attribute.c_str(), int_attr.getInt());
  return success();
}

LogicalResult ConvertSSDPostProcessFunc::AddFloatAttr(
    FuncOp func, DictionaryAttr attrs, const std::string& attribute,
    flexbuffers::Builder* builder) {
  auto float_attr = attrs.get(attribute).dyn_cast_or_null<FloatAttr>();
  if (!float_attr) {
    return func.emitError()
           << attribute.c_str() << " attribute is not set or not a float";
  }
  builder->Float(attribute.c_str(), float_attr.getValue().convertToFloat());
  return success();
}

LogicalResult ConvertSSDPostProcessFunc::HasIntAttr(
    FuncOp func, DictionaryAttr attrs, const std::string& attribute) {
  auto int_attr = attrs.get(attribute).dyn_cast_or_null<IntegerAttr>();
  if (!int_attr) {
    return func.emitWarning()
           << attribute.c_str() << " attribute is not set or not an integer";
  }
  return success();
}

LogicalResult ConvertSSDPostProcessFunc::HasFloatAttr(
    FuncOp func, DictionaryAttr attrs, const std::string& attribute) {
  auto float_attr = attrs.get(attribute).dyn_cast_or_null<FloatAttr>();
  if (!float_attr) {
    return func.emitWarning()
           << attribute.c_str() << " attribute is not set or not a float";
  }
  return success();
}

LogicalResult ConvertSSDPostProcessFunc::VerifySignature() {
  // Verify high-level function signature.
  if (func_.getNumArguments() != 3) {
    return func_.emitWarning()
           << "Invalid number of arguments to " << kCustomSSDPostprocessing
           << ": " << func_.getNumArguments();
  }
  if (func_.getType().getNumResults() != 4) {
    return func_.emitWarning()
           << "Invalid number of results from " << kCustomSSDPostprocessing
           << ": " << func_.getType().getNumResults();
  }

  auto attrs = attr_.getAttrs();
  if (failed(HasIntAttr(func_, attrs, "max_detections")) ||
      failed(HasIntAttr(func_, attrs, "max_classes_per_detection")) ||
      failed(HasIntAttr(func_, attrs, "num_classes")) ||
      failed(HasFloatAttr(func_, attrs, "nms_score_threshold")) ||
      failed(HasFloatAttr(func_, attrs, "nms_iou_threshold")) ||
      failed(HasFloatAttr(func_, attrs, "y_scale")) ||
      failed(HasFloatAttr(func_, attrs, "x_scale")) ||
      failed(HasFloatAttr(func_, attrs, "h_scale")) ||
      failed(HasFloatAttr(func_, attrs, "w_scale"))) {
    return failure();
  }
  return success();
}

}  // namespace TFL
}  // namespace mlir
