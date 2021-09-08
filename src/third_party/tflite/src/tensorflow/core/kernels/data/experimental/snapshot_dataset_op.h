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

#ifndef TENSORFLOW_CORE_KERNELS_DATA_EXPERIMENTAL_SNAPSHOT_DATASET_OP_H_
#define TENSORFLOW_CORE_KERNELS_DATA_EXPERIMENTAL_SNAPSHOT_DATASET_OP_H_

#include "absl/container/flat_hash_map.h"
#include "tensorflow/core/framework/dataset.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/op_requires.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/kernels/data/captured_function.h"
#include "tensorflow/core/kernels/data/dataset_utils.h"
#include "tensorflow/core/kernels/data/experimental/snapshot_util.h"
#include "tensorflow/core/kernels/data/name_utils.h"
#include "tensorflow/core/lib/gtl/map_util.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/mutex.h"
#include "tensorflow/core/platform/path.h"
#include "tensorflow/core/platform/random.h"
#include "tensorflow/core/platform/thread_annotations.h"

namespace tensorflow {
namespace data {
namespace experimental {

class SnapshotDatasetV2Op : public UnaryDatasetOpKernel {
 public:
  static constexpr const char* const kDatasetType = "Snapshot";
  static constexpr const char* const kOutputTypes = "output_types";
  static constexpr const char* const kOutputShapes = "output_shapes";
  static constexpr const char* const kCompression = "compression";
  static constexpr const char* const kReaderFunc = "reader_func";
  static constexpr const char* const kShardFunc = "shard_func";
  static constexpr const char* const kReaderFuncOtherArgs =
      "reader_func_other_args";
  static constexpr const char* const kShardFuncOtherArgs =
      "shard_func_other_args";
  static constexpr const char* const kReaderFuncTarguments =
      "Treader_func_args";
  static constexpr const char* const kShardFuncTarguments = "Tshard_func_args";

  explicit SnapshotDatasetV2Op(OpKernelConstruction* ctx);

 protected:
  void MakeDataset(OpKernelContext* ctx, DatasetBase* input,
                   DatasetBase** output) override;

 private:
  static constexpr const int kFileFormatVersion = 2;

  class Dataset;

  const int graph_def_version_;
  DataTypeVector output_types_;
  std::vector<PartialTensorShape> output_shapes_;

  std::string compression_;

  std::shared_ptr<FunctionMetadata> reader_func_metadata_;
  std::shared_ptr<FunctionMetadata> shard_func_metadata_;
};

}  // namespace experimental
}  // namespace data
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_KERNELS_DATA_EXPERIMENTAL_SNAPSHOT_DATASET_OP_H_
