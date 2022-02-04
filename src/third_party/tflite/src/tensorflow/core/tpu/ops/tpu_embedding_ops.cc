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

#include <array>
#include <string>

#include "absl/strings/str_cat.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/core/framework/attr_value.pb.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/lib/strings/stringprintf.h"
#include "tensorflow/core/protobuf/tpu/tpu_embedding_configuration.pb.h"
#include "tensorflow/core/tpu/ops/tpu_embedding_shape_util.h"
#include "tensorflow/core/tpu/tpu_embedding_optimization_parameters_utils.h"

namespace tensorflow {

using shape_inference::InferenceContext;
using shape_inference::ShapeHandle;
using tensorflow::tpu::TPUEmbeddingConfiguration;

REGISTER_OP("_ExecuteTPUEmbeddingPartitioner")
    .Output("common_config: string")
    .Attr("config: string")
    .SetIsStateful()
    .SetShapeFn([](InferenceContext* c) -> Status {
      string config_string;
      TF_RETURN_IF_ERROR(c->GetAttr("config", &config_string));
      TPUEmbeddingConfiguration config;
      TF_RET_CHECK(config.ParseFromString(config_string));
      if (config.mode() == TPUEmbeddingConfiguration::UNSPECIFIED) {
        return errors::InvalidArgument(
            "TPUEmbeddingConfiguration.mode is INVALID.  Must be INFERENCE, "
            "TRAINING, or BACKWARD_PASS_ONLY");
      }
      c->set_output(0, c->Scalar());
      return ::tensorflow::Status::OK();
    })
    .Doc(R"doc(

An op that executes the TPUEmbedding partitioner on the central configuration
device and computes the HBM size (in bytes) required for TPUEmbedding operation.

common_config: A string-encoded tpu_embedding::CommonConfiguration proto
containing metadata about the TPUEmbedding partitioner output and
the HBM size (in bytes) required for operation.
config: An TPUEmbeddingConfiguration proto serialized to a string,
describing the desired TPUEmbedding configuration.
)doc");

REGISTER_OP("_ConfigureTPUEmbeddingMemory")
    .Input("common_config: string")
    .Output("task_host_config: string")
    .Attr("config: string")
    .SetIsStateful()
    .SetShapeFn([](InferenceContext* c) -> Status {
      string config_string;
      TF_RETURN_IF_ERROR(c->GetAttr("config", &config_string));
      TPUEmbeddingConfiguration config;
      TF_RET_CHECK(config.ParseFromString(config_string));
      if (config.mode() == TPUEmbeddingConfiguration::UNSPECIFIED) {
        return errors::InvalidArgument(
            "TPUEmbeddingConfiguration.mode is INVALID.  Must be INFERENCE, "
            "TRAINING, or BACKWARD_PASS_ONLY");
      }
      TF_RET_CHECK(c->num_inputs() == 1);
      // Validate that all the input shape is compatible.
      ShapeHandle input(c->Scalar());
      TF_RETURN_IF_ERROR(c->Merge(c->input(0), input, &input));
      c->set_output(0, c->Scalar());
      return ::tensorflow::Status::OK();
    })
    .Doc(R"doc(

An op that configures the TPUEmbedding software on a host.

common_config: A string-encoded tpu_embedding CommonConfiguration proto
containing metadata about the TPUEmbedding partitioner output and the HBM
size (in bytes) required for operation.
task_host_config: A string-encoded tpu_embedding PerHostConfiguration proto
containing metadata about the memory allocations reserved for TPUEmbedding.
config: An TPUEmbeddingConfiguration proto serialized to a string,
describing the desired TPUEmbedding configuration.
)doc");

REGISTER_OP("_ConfigureTPUEmbeddingHost")
    .Input("common_config: string")
    .Input("task_host_config: N * string")
    .Output("host_config: string")
    .Attr("N: int >= 1")
    .Attr("config: string")
    .SetIsStateful()
    .SetShapeFn([](InferenceContext* c) -> Status {
      string config_string;
      TF_RETURN_IF_ERROR(c->GetAttr("config", &config_string));
      TPUEmbeddingConfiguration config;
      TF_RET_CHECK(config.ParseFromString(config_string));
      if (config.mode() == TPUEmbeddingConfiguration::UNSPECIFIED) {
        return errors::InvalidArgument(
            "TPUEmbeddingConfiguration.mode is INVALID.  Must be INFERENCE, "
            "TRAINING, or BACKWARD_PASS_ONLY");
      }
      TF_RET_CHECK(c->num_inputs() > 0);
      ShapeHandle input(c->Scalar());
      TF_RETURN_IF_ERROR(c->Merge(c->input(0), input, &input));
      c->set_output(0, c->Scalar());
      return ::tensorflow::Status::OK();
    })
    .Doc(R"doc(

An op that configures the TPUEmbedding software on a host.

common_config: A string-encoded tpu_embedding CommonConfiguration proto
containing metadata about the TPUEmbedding partitioner output.
task_host_config: A string-encoded tpu_embedding PerHostConfiguration proto from
each host containing metadata about the memory allocations reserved for
TPUEmbedding.
config: An TPUEmbeddingConfiguration proto serialized to a string,
describing the desired TPUEmbedding configuration.
)doc");

REGISTER_OP("_ConnectInterTPUEmbeddingCommunication")
    .Input("host_config: N * string")
    .Attr("N: int >= 1")
    .SetIsStateful()
    .SetShapeFn([](InferenceContext* c) -> Status {
      TF_RET_CHECK(c->num_inputs() > 0);
      ShapeHandle input(c->Scalar());
      // Validate that all the inputs are compatible with the correct
      // vector shape.
      for (int i = 0; i < c->num_inputs(); ++i) {
        TF_RETURN_IF_ERROR(c->Merge(c->input(i), input, &input));
      }
      return ::tensorflow::Status::OK();
    })
    .Doc(R"doc(

An op that sets up communication between TPUEmbedding host software instances after
ConfigureTPUEmbeddingHost has been called on each host.

host_config: A string-encoded tpu_embedding PerHostConfiguration proto read
from each host containing metadata about the RPC port used for communication
with that host.
)doc");

REGISTER_OP("_FinalizeTPUEmbeddingSystemConfiguration")
    .Input("host_config: N * string")
    .Attr("N: int >= 1")
    .SetIsStateful()
    .SetShapeFn([](InferenceContext* c) -> Status {
      ShapeHandle input(c->Scalar());
      // Validate that all the inputs are compatible with the correct
      // vector shape.
      for (int i = 0; i < c->num_inputs(); ++i) {
        TF_RETURN_IF_ERROR(c->Merge(c->input(i), input, &input));
      }
      return ::tensorflow::Status::OK();
    })
    .Doc(R"doc(

An op that finalizes the TPUEmbedding system configuration after
ConfigureTPUEmbeddingHost has been called on each host.

host_config: A string-encoded tpu_embedding PerHostConfiguration proto read
from each host containing metadata about the HBM base byte address reserved for
the TPUEmbedding on that host.
)doc");

// After configuring the TPU system (detailed in tpu_configuration_ops.cc),
// you may, if desired, run _ExecuteTPUEmbeddingPartitioner,
// _ConfigureTPUEmbeddingHost, _ConnectInterTPUEmbeddingCommunication and
// _FinalizeTPUEmbeddingSystemConfiguration Ops to configure the TPUEmbeddings.
//
// 1) The _ExecuteTPUEmbeddingPartitioner Op runs on TPU_SYSTEM of task 0. It
//    runs the embedding layer partitioner and computes the HBM size (in bytes)
//    needed for TPUEmbedding operation. Note that this Op does not need to wait
//    until the entire TPU system is configured, rather it only needs to wait
//    till _ConfigureDistributedTPU completes. Also stores the HBM size and the
//    embedding partitioner output in the system metadata where it can be used
//    while compiling embedding Ops for TPU.
// 2) The _ConfigureTPUEmbeddingMemory Op runs on TPU:0 of all tasks. Using the
//    output of the _ExecuteTPUEmbeddingPartitioner Op, it allocates HBM memory,
//    initializes the TPUEmbeddingManager and store the HBM buffer
//    configuration.
// 3) The _ConfigureTPUEmbeddingHost Op runs on TPU:0 of all tasks. Using the
//    output of the _ExecuteTPUEmbeddingPartitioner Op, it builds the program
//    that executes on the TPUEmbeddings, configures the TPUEmbedding hardware
//    and sets up the TPUEmbedding host software. Using the output of the
//    _ConfigureTPUEmbeddingMemory Op that it receives from all tasks, it
//    checks that HBM segment sizes are equal, and combines each task's
//    allocation info to create a global map of HBM base addresses. It uses that
//    to initialize the TPUEmbeddingManager, and also provides the hostname:port
//    for inter-TPUEmbedding agreement of minibatch sizing.
// 4) The _ConnectInterTPUEmbeddingCommunication Op runs on TPU:0 of all tasks.
//    It uses the hostname:port output from all _ConfigureTPUEmbeddingHost Ops
//    to form all-to-all connections between all tasks for inter-TPUEmbedding
//    agreement.
// 5) The _FinalizeTPUEmbeddingSystemConfiguration Op runs on TPU_SYSTEM of
//    task 0. It takes as input the outputs from all _ConfigureTPUEmbeddingHost
//    Ops and validates that the HBM base address (in bytes) used for
//    TPUEmbedding operation is the same. Also stores the common HBM base
//    address in the system metadata where it can be used while compiling
//    embedding Ops.

std::vector<std::string> GetPerTableLoadOptimizationParametersOps() {
  VLOG(1) << "GetPerTableLoadOptimizationParametersOps ";
  std::vector<std::string> result;
  for (tpu::OptimizationAlgorithm alg : tpu::GetOptimizationAlgorithms()) {
    const auto alg_name = tpu::GetOptimizationAlgorithmName(alg);
    const auto op_name =
        absl::StrCat("LoadTPUEmbedding", alg_name, "Parameters");
    result.push_back(op_name);
    // Some of these don't actually exist. Specifically
    // 'CenteredRMSProp.*GradAccumDebug' and
    // 'MDLAdagradLight.*GradAccumDebug''. Its ok to include them here as they
    // are only used to check for ops during re-write passes.
    const auto op_name_debug =
        absl::StrCat("LoadTPUEmbedding", alg_name, "ParametersGradAccumDebug");
    result.push_back(op_name_debug);
  }
  return result;
}

std::vector<std::string> GetPerTableRetrieveOptimizationParametersOps() {
  VLOG(1) << "GetPerTableRetrieveOptimizationParametersOps ";
  std::vector<std::string> result;
  for (tpu::OptimizationAlgorithm alg : tpu::GetOptimizationAlgorithms()) {
    const auto alg_name = tpu::GetOptimizationAlgorithmName(alg);
    const auto op_name =
        absl::StrCat("RetrieveTPUEmbedding", alg_name, "Parameters");
    result.push_back(op_name);
    // Some of these don't actually exist. Specifically
    // 'CenteredRMSProp.*GradAccumDebug' and
    // 'MDLAdagradLight.*GradAccumDebug'. Its ok to include them here as they
    // are only used to check for ops during re-write passes.
    const auto op_name_debug = absl::StrCat("RetrieveTPUEmbedding", alg_name,
                                            "ParametersGradAccumDebug");
    result.push_back(op_name_debug);
  }
  return result;
}

static_assert(tpu::kMaxAuxiliaryParameterCount == 7,
              "Need to update parameter count in "
              "LoadAllTPUEmbeddingParameters and "
              "RetrieveAllTPUEmbeddingParameters ops if "
              "kMaxAuxiliaryParameterCount changes");

REGISTER_OP("LoadAllTPUEmbeddingParameters")
    .Input("parameters: NumTables * float")
    .Input("auxiliary1: NumTables * float")
    .Input("auxiliary2: NumTables * float")
    .Input("auxiliary3: NumTables * float")
    .Input("auxiliary4: NumTables * float")
    .Input("auxiliary5: NumTables * float")
    .Input("auxiliary6: NumTables * float")
    .Input("auxiliary7: NumTables * float")
    .Attr("NumTables: int")
    .Attr("config: string")
    .Attr("num_shards: int")
    .Attr("shard_id: int")
    .SetIsStateful()
    .SetShapeFn([](InferenceContext* c) -> Status {
      string config_string;
      TF_RETURN_IF_ERROR(c->GetAttr("config", &config_string));
      TPUEmbeddingConfiguration config;
      TF_RET_CHECK(config.ParseFromString(config_string));
      int num_shards;
      TF_RETURN_IF_ERROR(c->GetAttr("num_shards", &num_shards));
      int shard_id;
      TF_RETURN_IF_ERROR(c->GetAttr("shard_id", &shard_id));
      std::vector<TensorShapeProto> table_shapes;
      TF_RETURN_IF_ERROR(tpu::TpuEmbeddingShapeUtil::ComputeTableShapes(
          config, shard_id, num_shards, &table_shapes));
      std::array<std::vector<ShapeHandle>, tpu::kMaxAuxiliaryParameterCount + 1>
          accumulators;
      TF_RETURN_IF_ERROR(c->input("parameters", &accumulators[0]));
      for (int i = 1; i <= tpu::kMaxAuxiliaryParameterCount; ++i) {
        TF_RETURN_IF_ERROR(
            c->input(absl::StrCat("auxiliary", i), &accumulators[i]));
      }
      TF_RET_CHECK(accumulators[0].size() == table_shapes.size());
      // This should be enforced by Tensorflow's type system.
      for (int i = 1; i <= tpu::kMaxAuxiliaryParameterCount; ++i) {
        CHECK_EQ(accumulators[0].size(), accumulators[i].size());  // Crash OK
      }
      for (int table_id = 0; table_id < accumulators[0].size(); ++table_id) {
        // Verify shapes have rank 2 and are compatible when they are required
        // to be valid.
        ShapeHandle parameter_shape;
        TF_RETURN_IF_ERROR(
            c->WithRank(accumulators[0][table_id], 2, &parameter_shape));

        std::vector<tpu::StateVariableSpecification> state_variable_specs;
        Status status = tpu::GetOptimizationAlgorithmStateVariables(
            config.table_descriptor(table_id).optimization_parameters(),
            &state_variable_specs);
        TF_RET_CHECK(status.ok());

        for (int i = 1; i < state_variable_specs.size(); ++i) {
          ShapeHandle accumulator_i_shape;
          TF_RETURN_IF_ERROR(
              c->WithRank(accumulators[i][table_id], 2, &accumulator_i_shape));
          ShapeHandle merged;
          TF_RETURN_IF_ERROR(c->Merge(accumulators[0][table_id],
                                      accumulator_i_shape, &merged));
          // Verify shapes are compatible with the shapes specified in
          // the config.
          ShapeHandle from_config;
          TF_RETURN_IF_ERROR(
              c->MakeShapeFromShapeProto(table_shapes[table_id], &from_config));
          ShapeHandle merged_with_config;
          TF_RETURN_IF_ERROR(
              c->Merge(merged, from_config, &merged_with_config));
        }
        // Ensure that other state variables are empty to catch bugs in
        // CombineTPUEmbeddingLoadRetrievePass output or manually-written
        // equivalent code.
        for (int i = state_variable_specs.size();
             i < tpu::kMaxAuxiliaryParameterCount + 1; ++i) {
          ShapeHandle accumulator_i_shape;
          TF_RETURN_IF_ERROR(c->WithRankAtLeast(accumulators[i][table_id], 1,
                                                &accumulator_i_shape));
          shape_inference::DimensionHandle dim;
          TF_RETURN_IF_ERROR(
              c->WithValue(c->NumElements(accumulator_i_shape), 0, &dim));
        }
      }
      return tensorflow::Status::OK();
    });

REGISTER_OP("RetrieveAllTPUEmbeddingParameters")
    .Output("parameters: NumTables * float")
    .Output("auxiliary1: NumTables * float")
    .Output("auxiliary2: NumTables * float")
    .Output("auxiliary3: NumTables * float")
    .Output("auxiliary4: NumTables * float")
    .Output("auxiliary5: NumTables * float")
    .Output("auxiliary6: NumTables * float")
    .Output("auxiliary7: NumTables * float")
    .Attr("NumTables: int")
    .Attr("config: string")
    .Attr("num_shards: int")
    .Attr("shard_id: int")
    .SetIsStateful()
    .SetShapeFn([](InferenceContext* c) -> Status {
      string config_string;
      TF_RETURN_IF_ERROR(c->GetAttr("config", &config_string));
      TPUEmbeddingConfiguration config;
      TF_RET_CHECK(config.ParseFromString(config_string));
      int num_shards;
      TF_RETURN_IF_ERROR(c->GetAttr("num_shards", &num_shards));
      int shard_id;
      TF_RETURN_IF_ERROR(c->GetAttr("shard_id", &shard_id));
      std::vector<TensorShapeProto> table_shapes;
      TF_RETURN_IF_ERROR(tpu::TpuEmbeddingShapeUtil::ComputeTableShapes(
          config, shard_id, num_shards, &table_shapes));
      int num_tables;
      TF_RETURN_IF_ERROR(c->GetAttr("NumTables", &num_tables));
      TF_RET_CHECK(num_tables == table_shapes.size());
      TF_RET_CHECK(num_tables == config.table_descriptor_size());
      for (int i = 0; i < tpu::kMaxAuxiliaryParameterCount + 1; ++i) {
        std::vector<ShapeHandle> output_handles;
        for (int table_id = 0; table_id < table_shapes.size(); ++table_id) {
          std::vector<tpu::StateVariableSpecification> state_variable_specs;
          TF_RETURN_IF_ERROR(tpu::GetOptimizationAlgorithmStateVariables(
              config.table_descriptor(table_id).optimization_parameters(),
              &state_variable_specs));

          TensorShapeProto output_shape_proto =
              (i < state_variable_specs.size()
                   ? table_shapes[table_id]
                   : tpu::TpuEmbeddingShapeUtil::MakeEmpty2DShape());
          ShapeHandle output_shape;
          TF_RETURN_IF_ERROR(
              c->MakeShapeFromShapeProto(output_shape_proto, &output_shape));
          output_handles.push_back(output_shape);
        }
        if (i == 0) {
          TF_RETURN_IF_ERROR(c->set_output("parameters", output_handles));
        } else {
          TF_RETURN_IF_ERROR(
              c->set_output(absl::StrCat("auxiliary", i), output_handles));
        }
      }
      return ::tensorflow::Status::OK();
    });

REGISTER_OP("EnqueueTPUEmbeddingBatch")
    .Input("batch: N * string")
    .Input("mode_override: string")
    .Attr("N: int")
    .Attr("device_ordinal: int = -1")
    .Attr("combiners: list(string) = []")
    .SetIsStateful()
    .SetShapeFn([](InferenceContext* c) -> Status {
      std::vector<std::string> combiners;
      TF_RETURN_IF_ERROR(c->GetAttr("combiners", &combiners));
      int n;
      TF_RETURN_IF_ERROR(c->GetAttr("N", &n));
      if (!combiners.empty() && combiners.size() != n) {
        return errors::InvalidArgument("Invalid length of combiners. Have ",
                                       combiners.size(), " but expected 0 or ",
                                       n);
      }

      return Status::OK();
    });

}  // namespace tensorflow
