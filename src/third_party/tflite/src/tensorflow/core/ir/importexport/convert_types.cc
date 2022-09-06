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

#include "tensorflow/core/ir/importexport/convert_types.h"

#include "absl/strings/str_cat.h"
#include "llvm/Support/Casting.h"
#include "mlir/IR/BuiltinTypes.h"  // from @llvm-project
#include "mlir/IR/Types.h"  // from @llvm-project
#include "mlir/Support/DebugStringHelper.h"  // from @llvm-project
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/framework/types.pb.h"
#include "tensorflow/core/ir/dialect.h"
#include "tensorflow/core/lib/core/errors.h"

namespace mlir {
namespace tfg {

using tensorflow::DataType;
using tensorflow::Status;
using tensorflow::TensorShape;
using tensorflow::TensorShapeProto;
using tensorflow::errors::InvalidArgument;
using tensorflow::errors::Unimplemented;

Status ConvertDataType(DataType dtype, Builder& builder, Type* type) {
  switch (dtype) {
    case tensorflow::DT_HALF:
      *type = builder.getF16Type();
      return Status::OK();
    case tensorflow::DT_FLOAT:
      *type = builder.getF32Type();
      return Status::OK();
    case tensorflow::DT_DOUBLE:
      *type = builder.getF64Type();
      return Status::OK();
    case tensorflow::DT_BOOL:
      *type = builder.getIntegerType(1);
      return Status::OK();
    case tensorflow::DT_INT8:
      *type = builder.getIntegerType(8);
      return Status::OK();
    case tensorflow::DT_INT16:
      *type = builder.getIntegerType(16);
      return Status::OK();
    case tensorflow::DT_INT32:
      *type = builder.getIntegerType(32);
      return Status::OK();
    case tensorflow::DT_INT64:
      *type = builder.getIntegerType(64);
      return Status::OK();
    case tensorflow::DT_UINT8:
      *type = builder.getIntegerType(8, /*isSigned=*/false);
      return Status::OK();
    case tensorflow::DT_UINT16:
      *type = builder.getIntegerType(16, /*isSigned=*/false);
      return Status::OK();
    case tensorflow::DT_UINT32:
      *type = builder.getIntegerType(32, /*isSigned=*/false);
      return Status::OK();
    case tensorflow::DT_UINT64:
      *type = builder.getIntegerType(64, /*isSigned=*/false);
      return Status::OK();
    case tensorflow::DT_BFLOAT16:
      *type = builder.getBF16Type();
      return Status::OK();
    case tensorflow::DT_COMPLEX64:
      *type = ComplexType::get(builder.getF32Type());
      return Status::OK();
    case tensorflow::DT_COMPLEX128:
      *type = ComplexType::get(builder.getF64Type());
      return Status::OK();
#define HANDLE_TF_TYPE(tftype, enumerant, name) \
  case tensorflow::DT_##enumerant:              \
    *type = builder.getType<tftype##Type>();    \
    return Status::OK();
#include "tensorflow/core/ir/types/types.def"

    default:
      return Unimplemented(absl::StrCat(
          "Converting DataType '", DataTypeString(dtype), "' to MLIR Type"));
  }
}

Status ConvertScalarTypeToDataType(Type type, DataType* dtype) {
  if (type.isF16()) {
    *dtype = tensorflow::DT_HALF;
    return Status::OK();
  } else if (type.isF32()) {
    *dtype = tensorflow::DT_FLOAT;
    return Status::OK();
  } else if (type.isF64()) {
    *dtype = tensorflow::DT_DOUBLE;
    return Status::OK();
  } else if (type.isBF16()) {
    *dtype = tensorflow::DT_BFLOAT16;
    return Status::OK();
  } else if (auto itype = type.dyn_cast<IntegerType>()) {
    switch (itype.getWidth()) {
      case 1:
        *dtype = tensorflow::DT_BOOL;
        return Status::OK();
      case 8:
        *dtype =
            itype.isUnsigned() ? tensorflow::DT_UINT8 : tensorflow::DT_INT8;
        return Status::OK();
      case 16:
        *dtype =
            itype.isUnsigned() ? tensorflow::DT_UINT16 : tensorflow::DT_INT16;
        return Status::OK();
      case 32:
        *dtype =
            itype.isUnsigned() ? tensorflow::DT_UINT32 : tensorflow::DT_INT32;
        return Status::OK();
      case 64:
        *dtype =
            itype.isUnsigned() ? tensorflow::DT_UINT64 : tensorflow::DT_INT64;
        return Status::OK();
      default:
        return Unimplemented(
            absl::StrCat("Converting ", debugString(type), " to DataType"));
    }
  } else if (auto complex_type = type.dyn_cast<ComplexType>()) {
    auto etype = complex_type.getElementType();
    if (etype.isF32()) {
      *dtype = tensorflow::DT_COMPLEX64;
      return Status::OK();
    } else if (etype.isF64()) {
      *dtype = tensorflow::DT_COMPLEX128;
      return Status::OK();
    }
    return Unimplemented(
        absl::StrCat("Converting ", debugString(type), " to DataType"));
  }

#define HANDLE_TF_TYPE(tftype, enumerant, name) \
  if (type.isa<tftype##Type>()) {               \
    *dtype = tensorflow::DT_##enumerant;        \
    return Status::OK();                        \
  }
// NOLINTNEXTLINE
#include "tensorflow/core/ir/types/types.def"

  return Unimplemented(
      absl::StrCat("Converting ", debugString(type), " to DataType"));
}

Status ConvertToDataType(Type type, DataType* dtype) {
  if (auto stype = type.dyn_cast<ShapedType>()) {
    TF_RETURN_IF_ERROR(
        ConvertScalarTypeToDataType(stype.getElementType(), dtype));
  } else {
    TF_RETURN_IF_ERROR(ConvertScalarTypeToDataType(type, dtype));
  }
  return Status::OK();
}

void ConvertToMlirShape(const TensorShape& input_shape,
                        SmallVectorImpl<int64_t>* shape) {
  shape->reserve(input_shape.dims());
  for (const auto& d : input_shape) {
    shape->push_back(d.size);
  }
}

Status ConvertToMlirShape(const TensorShapeProto& input_shape,
                          SmallVectorImpl<int64_t>* shape) {
  shape->reserve(input_shape.dim_size());
  auto& dims = input_shape.dim();
  for (auto& d : dims) {
    if (d.size() > std::numeric_limits<int64_t>::max()) {
      return InvalidArgument("Shape element overflows");
    }
    // This isn't really expected, but Grappler is using such shapes for its
    // symbolic shape analysis and it may spill into here.
    if (d.size() < ShapedType::kDynamicSize)
      shape->push_back(ShapedType::kDynamicSize);
    else
      shape->push_back(d.size());
  }
  return Status::OK();
}

tensorflow::StatusOr<Type> ConvertToMlirTensorType(
    const TensorShapeProto& shape, DataType dtype, Builder* builder) {
  Type element_type;
  TF_RETURN_IF_ERROR(ConvertDataType(dtype, *builder, &element_type));
  if (shape.unknown_rank()) {
    return UnrankedTensorType::get(element_type);
  }
  SmallVector<int64_t, 4> shape_dims;
  TF_RETURN_IF_ERROR(ConvertToMlirShape(shape, &shape_dims));
  return RankedTensorType::get(shape_dims, element_type);
}

}  // namespace tfg
}  // namespace mlir
