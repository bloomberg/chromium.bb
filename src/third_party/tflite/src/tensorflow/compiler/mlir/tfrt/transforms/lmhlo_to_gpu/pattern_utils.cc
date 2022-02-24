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

#include "tensorflow/compiler/mlir/tfrt/transforms/lmhlo_to_gpu/pattern_utils.h"

#include "tfrt/basic_kernels/opdefs/basic_kernels.h"  // from @tf_runtime

namespace tensorflow {

cudaDataType_t MlirTypeToCudaDataType(mlir::Type type) {
  if (type.isF16()) return CUDA_R_16F;
  if (type.isF32()) return CUDA_R_32F;
  if (type.isF64()) return CUDA_R_64F;
  if (auto complex_type = type.dyn_cast<mlir::ComplexType>()) {
    auto element_type = complex_type.getElementType();
    if (element_type.isF32()) return CUDA_C_32F;
    if (element_type.isF64()) return CUDA_C_64F;
  }
  if (type.isSignlessInteger(/*width=*/8)) return CUDA_R_8I;
  if (type.isSignlessInteger(/*width=*/32)) return CUDA_R_32I;
  llvm_unreachable("unsupported type");
}

cublasComputeType_t MlirTypeToCublasComputeType(mlir::Type type) {
  if (type.isF16()) return CUBLAS_COMPUTE_16F;
  if (type.isF32()) return CUBLAS_COMPUTE_32F;
  if (type.isF64()) return CUBLAS_COMPUTE_64F;
  if (type.isSignlessInteger(/*width=*/32)) return CUBLAS_COMPUTE_32I;
  llvm_unreachable("unsupported type");
}

cudnnDataType_t MlirTypeToCudnnDataType(mlir::Type type) {
  if (type.isF16()) return CUDNN_DATA_HALF;
  if (type.isBF16()) return CUDNN_DATA_BFLOAT16;
  if (type.isF32()) return CUDNN_DATA_FLOAT;
  if (type.isF64()) return CUDNN_DATA_DOUBLE;
  if (type.isSignlessInteger(/*width=*/8)) return CUDNN_DATA_INT8;
  if (type.isSignlessInteger(/*width=*/32)) return CUDNN_DATA_INT32;
  if (type.isSignlessInteger(/*width=*/64)) return CUDNN_DATA_INT64;
  if (type.isUnsignedInteger(/*width=*/8)) return CUDNN_DATA_UINT8;
  llvm_unreachable("unsupported type");
}

cudnnDataType_t MlirTypeToCudnnDataType(mlir::Type type,
                                        se::dnn::DataLayout data_layout) {
  switch (data_layout) {
    case se::dnn::DataLayout::kBatchDepthYX4:
      if (type.isSignlessInteger(/*width=*/8)) {
        return CUDNN_DATA_INT8x4;
      }
      if (type.isUnsignedInteger(/*width=*/8)) {
        return CUDNN_DATA_UINT8x4;
      }
      break;
    case se::dnn::DataLayout::kBatchDepthYX32:
      if (type.isSignlessInteger(/*width=*/32)) {
        return CUDNN_DATA_INT8x32;
      }
      break;
    default:
      break;
  }
  return MlirTypeToCudnnDataType(type);
}

cudnnDataType_t MlirTypeToCudnnDataType(mlir::Type type,
                                        se::dnn::FilterLayout filter_layout) {
  switch (filter_layout) {
    case se::dnn::FilterLayout::kOutputInputYX4:
      if (type.isSignlessInteger(/*width=*/8)) {
        return CUDNN_DATA_INT8x4;
      }
      if (type.isUnsignedInteger(/*width=*/8)) {
        return CUDNN_DATA_UINT8x4;
      }
      break;
    case se::dnn::FilterLayout::kOutputInputYX32:
      if (type.isSignlessInteger(/*width=*/8)) {
        return CUDNN_DATA_INT8x32;
      }
      break;
    default:
      break;
  }
  return MlirTypeToCudnnDataType(type);
}

mlir::Value MakeScalingFactorConstant(mlir::OpBuilder& builder,
                                      mlir::Location loc, mlir::Type type,
                                      llvm::APFloat value_real,
                                      llvm::APFloat value_imaginary) {
  bool losesInfo = false;
  if (type.isF32()) {
    value_real.convert(llvm::APFloat::IEEEsingle(),
                       llvm::RoundingMode::NearestTiesToEven, &losesInfo);
    return builder.create<tfrt::compiler::ConstantF32Op>(loc, value_real);
  }
  if (type.isF64()) {
    value_real.convert(llvm::APFloat::IEEEdouble(),
                       llvm::RoundingMode::NearestTiesToEven, &losesInfo);
    return builder.create<tfrt::compiler::ConstantF64Op>(loc, value_real);
  }
  if (type.isa<mlir::ComplexType>()) {
    auto element_type = type.cast<ComplexType>().getElementType();
    if (element_type.isF32()) {
      value_real.convert(llvm::APFloat::IEEEsingle(),
                         llvm::RoundingMode::NearestTiesToEven, &losesInfo);
      value_imaginary.convert(llvm::APFloat::IEEEsingle(),
                              llvm::RoundingMode::NearestTiesToEven,
                              &losesInfo);
      return builder.create<tfrt::compiler::ConstantComplexF32Op>(
          loc, value_real, value_imaginary);
    }
    if (element_type.isF64()) {
      value_real.convert(llvm::APFloat::IEEEdouble(),
                         llvm::RoundingMode::NearestTiesToEven, &losesInfo);
      value_imaginary.convert(llvm::APFloat::IEEEdouble(),
                              llvm::RoundingMode::NearestTiesToEven,
                              &losesInfo);
      return builder.create<tfrt::compiler::ConstantComplexF64Op>(
          loc, value_real, value_imaginary);
    }
  }
  if (type.isSignlessInteger(/*width=*/32)) {
    llvm::APSInt value_int;
    bool is_exact = false;
    value_real.convertToInteger(
        value_int, llvm::RoundingMode::NearestTiesToEven, &is_exact);
    return builder.create<tfrt::compiler::ConstantI32Op>(
        loc, value_int.getExtValue());
  }

  llvm_unreachable("unsupported type");
}

mlir::Value MakeBitPatternConstant(mlir::OpBuilder& builder, mlir::Location loc,
                                   mlir::Type type, uint32_t bit_pattern) {
  llvm::APInt value(32, bit_pattern);
  if (type.isSignlessInteger(/*width=*/32)) {
    return builder.create<tfrt::compiler::ConstantI32Op>(loc,
                                                         value.getZExtValue());
  }
  if (type.isUnsignedInteger(/*width=*/32)) {
    return builder.create<tfrt::compiler::ConstantUI32Op>(loc,
                                                          value.getZExtValue());
  }
  if (type.isF32()) {
    llvm::APFloat value_float(value.bitsToFloat());  // Like reinterpret_cast.
    return builder.create<tfrt::compiler::ConstantF32Op>(loc, value_float);
  }

  llvm_unreachable("unsupported type");
}

}  // namespace tensorflow
