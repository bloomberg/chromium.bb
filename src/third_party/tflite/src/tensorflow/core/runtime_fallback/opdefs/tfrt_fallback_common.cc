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
#include "tensorflow/core/runtime_fallback/opdefs/tfrt_fallback_common.h"

#include "mlir/IR/Builders.h"  // from @llvm-project

namespace tfrt {
namespace fallback_common {

void GetExecuteOpAttrsCommon(
    mlir::MLIRContext *context, llvm::ArrayRef<mlir::Attribute> op_attr_array,
    llvm::SmallVectorImpl<std::pair<llvm::StringRef, mlir::Attribute>>
        *op_attrs) {
  assert(op_attrs);
  op_attrs->clear();

  mlir::Builder builder(context);
  for (auto iter : op_attr_array) {
    auto key_value = iter.cast<mlir::ArrayAttr>().getValue();
    llvm::StringRef key = key_value[0].cast<mlir::StringAttr>().getValue();
    mlir::Attribute value = key_value[1];
    op_attrs->push_back({key, value});
  }
}

mlir::ParseResult ParseExecuteOpCommon(mlir::OpAsmParser &parser,
                                       mlir::Builder &builder,
                                       mlir::OperationState &result,
                                       mlir::Type tensor_type,
                                       const ParseExecuteOpOptions &options) {
  auto chain_type = builder.getType<compiler::ChainType>();

  mlir::IntegerAttr op_key;
  mlir::IntegerAttr cost;
  mlir::StringAttr device;
  mlir::StringAttr op_name;
  llvm::SmallVector<mlir::OpAsmParser::OperandType, 4> in_chains;
  llvm::SmallVector<mlir::OpAsmParser::OperandType, 4> operands;
  mlir::NamedAttrList op_attrs;
  mlir::NamedAttrList op_func_attrs;
  auto loc = parser.getNameLoc();

  if (options.has_chain &&
      parser.parseOperandList(in_chains,
                              /*requiredOperandCount=*/1,
                              mlir::OpAsmParser::Delimiter::Paren))
    return mlir::failure();

  if (options.has_key &&
      (parser.parseKeyword("key") || parser.parseLParen() ||
       parser.parseAttribute(op_key, "op_key", result.attributes) ||
       parser.parseRParen()))
    return mlir::failure();

  if (options.has_cost &&
      (parser.parseKeyword("cost") || parser.parseLParen() ||
       parser.parseAttribute(cost, "_tfrt_cost", result.attributes) ||
       parser.parseRParen()))
    return mlir::failure();

  if (options.has_device &&
      (parser.parseKeyword("device") || parser.parseLParen() ||
       parser.parseAttribute(device, "device", result.attributes) ||
       parser.parseRParen()))
    return mlir::failure();

  if (parser.parseAttribute(op_name, "op_name", result.attributes) ||
      parser.parseOperandList(operands, mlir::OpAsmParser::Delimiter::Paren) ||
      parser.parseOptionalAttrDict(op_attrs) ||
      parser.parseOptionalAttrDict(op_func_attrs))
    return mlir::failure();

  int64_t num_results = 0;
  if (succeeded(parser.parseOptionalColon())) {
    mlir::IntegerAttr attr;
    mlir::NamedAttrList attrs;
    if (failed(parser.parseAttribute(attr, "num_results", attrs)))
      return mlir::failure();
    num_results = attr.getValue().getSExtValue();
  }

  llvm::SmallVector<mlir::Type, 4> operand_types;
  if (options.has_chain) operand_types.push_back(chain_type);
  if (parser.resolveOperands(in_chains, operand_types, loc, result.operands) ||
      parser.resolveOperands(operands, tensor_type, result.operands))
    return mlir::failure();

  if (options.has_chain) result.types.push_back(chain_type);
  result.types.append(num_results, tensor_type);

  llvm::SmallVector<mlir::Attribute, 4> op_attr_array;
  for (const auto &key_value : op_attrs) {
    auto key = key_value.getName();
    auto value = key_value.getValue();
    op_attr_array.push_back(builder.getArrayAttr({key, value}));
  }

  result.attributes.push_back(
      builder.getNamedAttr("op_attrs", builder.getArrayAttr(op_attr_array)));

  // TODO(tfrt-devs): support func attributes in tfrt_fallback_sync.
  if (options.has_func_attr) {
    llvm::SmallVector<mlir::Attribute, 4> op_func_attr_array;
    for (const auto &key_value : op_func_attrs) {
      auto key = key_value.getName();
      auto value = key_value.getValue();
      op_func_attr_array.push_back(builder.getArrayAttr({key, value}));
    }

    result.attributes.push_back(builder.getNamedAttr(
        "op_func_attrs", builder.getArrayAttr(op_func_attr_array)));
  }

  return mlir::success();
}

}  // namespace fallback_common
}  // namespace tfrt
