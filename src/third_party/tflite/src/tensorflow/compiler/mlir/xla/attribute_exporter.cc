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

#include "tensorflow/compiler/mlir/xla/attribute_exporter.h"

#include "tensorflow/compiler/mlir/hlo/include/mlir-hlo/Dialect/lhlo_gpu/IR/lhlo_gpu_ops.h"
#include "tensorflow/compiler/xla/types.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"
#include "tensorflow/stream_executor/dnn.h"

namespace xla {

ConvolutionDimensionNumbers ConvertConvDimensionNumbers(
    mlir::mhlo::ConvDimensionNumbersAttr input) {
  ConvolutionDimensionNumbers output;

  output.set_input_batch_dimension(input.getInputBatchDimension());
  output.set_input_feature_dimension(input.getInputFeatureDimension());
  for (auto v : input.getInputSpatialDimensions()) {
    output.add_input_spatial_dimensions(v);
  }

  output.set_kernel_input_feature_dimension(
      input.getKernelInputFeatureDimension());
  output.set_kernel_output_feature_dimension(
      input.getKernelOutputFeatureDimension());

  for (auto v : input.getKernelSpatialDimensions()) {
    output.add_kernel_spatial_dimensions(v);
  }

  output.set_output_batch_dimension(input.getOutputBatchDimension());
  output.set_output_feature_dimension(input.getOutputFeatureDimension());

  for (auto v : input.getOutputSpatialDimensions()) {
    output.add_output_spatial_dimensions(v);
  }

  return output;
}

StatusOr<stream_executor::dnn::ActivationMode> ConvertConvActivationMode(
    mlir::lmhlo_gpu::Activation activation) {
  switch (activation) {
    case mlir::lmhlo_gpu::Activation::None:
      return stream_executor::dnn::kNone;
    case mlir::lmhlo_gpu::Activation::Sigmoid:
      return stream_executor::dnn::kSigmoid;
    case mlir::lmhlo_gpu::Activation::Tanh:
      return stream_executor::dnn::kTanh;
    case mlir::lmhlo_gpu::Activation::Relu:
      return stream_executor::dnn::kRelu;
    case mlir::lmhlo_gpu::Activation::Relu6:
      return stream_executor::dnn::kRelu6;
    case mlir::lmhlo_gpu::Activation::ReluX:
      return stream_executor::dnn::kReluX;
    case mlir::lmhlo_gpu::Activation::BandPass:
      return stream_executor::dnn::kBandPass;
    default:
      return InternalError("Unexpected activation");
  }
}

// Convert replica group from MLIR encoding to HLO.
// See HloFunctionImporter::ConvertReplicaGroups for the MLIR encoding.
StatusOr<std::vector<ReplicaGroup>> ConvertReplicaGroups(
    mlir::DenseIntElementsAttr input) {
  mlir::RankedTensorType type =
      input.getType().dyn_cast<mlir::RankedTensorType>();
  if (!type || type.getRank() != 2 ||
      !type.getElementType().isInteger(/*width=*/64)) {
    return InternalError("Execpted replica group to be a rank 2 tensor of i64");
  }
  // rank 0 is num_groups, rank 1 is group size.
  auto replica_group_values_it = input.getValues<uint64_t>().begin();
  std::vector<ReplicaGroup> replica_groups(type.getDimSize(0));
  for (ReplicaGroup& group : replica_groups) {
    for (int64_t element_idx = 0; element_idx < type.getDimSize(1);
         ++element_idx, ++replica_group_values_it) {
      // For replica group attribute, -1 indicates padding added by
      // HloFunctionImporter::ConvertReplicaGroups. This should always be at the
      // end and can be dropped when converting back to XLA HLO ReplicaGroups.
      if (*replica_group_values_it != -1) {
        group.add_replica_ids(*replica_group_values_it);
      }
    }
  }
  return replica_groups;
}

// Convert a (N, 2) dense attribute to a list of tuples. This is the way padding
// and source-target pairs are defined in HLO.
StatusOr<std::vector<std::pair<int64_t, int64_t>>> ConvertNx2Attribute(
    llvm::Optional<mlir::DenseIntElementsAttr> optional_attr) {
  if (!optional_attr.hasValue())
    return std::vector<std::pair<int64_t, int64_t>>{};
  mlir::DenseIntElementsAttr attr = *optional_attr;
  auto type = attr.getType().dyn_cast<mlir::RankedTensorType>();
  if (!type || type.getRank() != 2 || type.getShape()[1] != 2)
    return InternalError("expected Nx2 attribute to be a tensor of shape Nx2");
  auto it = attr.getValues<int64_t>().begin();
  std::vector<std::pair<int64_t, int64_t>> out(attr.getNumElements() / 2);
  for (auto& item : out) {
    int64_t first = *it;
    ++it;
    int64_t second = *it;
    ++it;
    item = {first, second};
  }
  return out;
}

StatusOr<FftType> ConvertFftType(llvm::StringRef type_string) {
  llvm::Optional<mlir::mhlo::FftType> type =
      mlir::mhlo::symbolizeEnum<mlir::mhlo::FftType>(type_string);
  if (!type) return InvalidArgument("Unknown FFT type %s", type_string.str());

  switch (*type) {
    case mlir::mhlo::FftType::FFT:
      return xla::FftType::FFT;
    case mlir::mhlo::FftType::IFFT:
      return xla::FftType::IFFT;
    case mlir::mhlo::FftType::RFFT:
      return xla::FftType::RFFT;
    case mlir::mhlo::FftType::IRFFT:
      return xla::FftType::IRFFT;
    default:
      return InvalidArgument("Unknown FFT type enum #%d", *type);
  }
}

StatusOr<TriangularSolveOptions::Transpose> ConvertTranspose(
    llvm::StringRef transpose_string) {
  llvm::Optional<mlir::mhlo::Transpose> transpose =
      mlir::mhlo::symbolizeTranspose(transpose_string);
  if (!transpose)
    return InvalidArgument("Unknown transpose type %s", transpose_string.str());

  switch (*transpose) {
    case mlir::mhlo::Transpose::NO_TRANSPOSE:
      return TriangularSolveOptions::NO_TRANSPOSE;
    case mlir::mhlo::Transpose::TRANSPOSE:
      return TriangularSolveOptions::TRANSPOSE;
    case mlir::mhlo::Transpose::ADJOINT:
      return TriangularSolveOptions::ADJOINT;
    case mlir::mhlo::Transpose::TRANSPOSE_INVALID:
      return TriangularSolveOptions::TRANSPOSE_INVALID;
    default:
      return InvalidArgument("Unknown transpose enum value #%d", *transpose);
  }
}

StatusOr<xla::CustomCallApiVersion> ConvertCustomCallApiVersion(
    mlir::mhlo::CustomCallApiVersion api_version) {
  switch (api_version) {
    case mlir::mhlo::CustomCallApiVersion::API_VERSION_UNSPECIFIED:
      return xla::CustomCallApiVersion::API_VERSION_UNSPECIFIED;
    case mlir::mhlo::CustomCallApiVersion::API_VERSION_ORIGINAL:
      return xla::CustomCallApiVersion::API_VERSION_ORIGINAL;
    case mlir::mhlo::CustomCallApiVersion::API_VERSION_STATUS_RETURNING:
      return xla::CustomCallApiVersion::API_VERSION_STATUS_RETURNING;
    default:
      return InvalidArgument("Unknown CustomCallApiVersion enum value #%d",
                             api_version);
  }
}

}  // namespace xla
