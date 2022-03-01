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

#include "tensorflow/compiler/tf2tensorrt/trt_convert_api.h"

#include <iostream>
#include <string>
#include <vector>

#include "absl/strings/str_join.h"
#include "tensorflow/compiler/tf2tensorrt/convert/utils.h"
#include "tensorflow/compiler/tf2tensorrt/utils/trt_lru_cache.h"
#include "tensorflow/core/common_runtime/device.h"
#include "tensorflow/core/common_runtime/device_mgr.h"
#include "tensorflow/core/common_runtime/graph_constructor.h"
#include "tensorflow/core/grappler/clusters/virtual_cluster.h"
#include "tensorflow/core/grappler/grappler_item.h"
#include "tensorflow/core/grappler/grappler_item_builder.h"
#include "tensorflow/core/grappler/optimizers/meta_optimizer.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/protobuf/meta_graph.pb.h"
#include "tensorflow/core/public/session.h"

#if GOOGLE_CUDA && GOOGLE_TENSORRT

namespace tensorflow {

namespace tensorrt {
namespace {

Status RunGrappler(const MetaGraphDef& meta_graph_def,
                   const std::vector<std::string>& input_names,
                   const std::vector<std::string>& output_names,
                   const ConfigProto& config_proto, GraphDef* out_graph_def) {
  grappler::ItemConfig item_config;

  for (const string& name : input_names) {
    item_config.feed_nodes.insert(name);
  }
  for (const string& name : output_names) {
    item_config.fetch_nodes.insert(name);
  }

  std::unique_ptr<grappler::GrapplerItem> item =
      grappler::GrapplerItemFromMetaGraphDef("tf_graph", meta_graph_def,
                                             item_config);
  if (!item) {
    return tensorflow::errors::Internal(
        "Failed to create grappler item from MetaGraphDef.");
  }

  TF_RETURN_IF_ERROR(grappler::RunMetaOptimizer(
      std::move(*item), config_proto, nullptr, nullptr, out_graph_def));
  VLOG(2) << "Grappler finished\n";
  return Status::OK();
}

Status ImportGraphDefToSession(Session* session, const GraphDef& graph_def,
                               const string& prefix) {
  ImportGraphDefOptions opts;
  opts.prefix = prefix;
  Graph graph(OpRegistry::Global());
  TF_RETURN_IF_ERROR(ImportGraphDef(opts, graph_def, &graph, nullptr));
  GraphDef new_graph_def;
  graph.ToGraphDef(&new_graph_def);
  TF_RETURN_IF_ERROR(session->Extend(new_graph_def));
  return Status::OK();
}

Status GetTrtRewriterConfig(const TfTrtConversionParams& params,
                            RewriterConfig* opt_config) {
  opt_config->set_meta_optimizer_iterations(tensorflow::RewriterConfig::ONE);
  opt_config->set_min_graph_nodes(-1);  // do not skip small graphs

  // Initial transformations before TensorRTOptimizer is called
  opt_config->add_optimizers("function");
  opt_config->add_optimizers("constfold");
  opt_config->add_optimizers("layout");
  opt_config->add_optimizers("constfold");

  // Parameters for TensorRTOptimizer
  auto trt_optimizer = opt_config->add_custom_optimizers();
  trt_optimizer->set_name("TensorRTOptimizer");

  auto trt_parameter_map = trt_optimizer->mutable_parameter_map();
  (*trt_parameter_map)["is_dynamic_op"].set_b(true);
  (*trt_parameter_map)["minimum_segment_size"].set_i(
      params.minimum_segment_size);
  string prec_string;
  TF_RETURN_IF_ERROR(
      TrtPrecisionModeToName(params.precision_mode, &prec_string));
  (*trt_parameter_map)["precision_mode"].set_s(prec_string);
  (*trt_parameter_map)["max_batch_size"].set_i(1);
  (*trt_parameter_map)["max_workspace_size_bytes"].set_i(
      params.max_workspace_size_bytes);
  (*trt_parameter_map)["max_cached_engines"].set_i(params.max_cached_engines);
  (*trt_parameter_map)["use_calibration"].set_b(params.use_calibration);
  (*trt_parameter_map)["profile_strategy"].set_s(
      ProfileStrategyToName(params.profile_strategy));
  (*trt_parameter_map)["use_implicit_batch"].set_b(!params.use_dynamic_shape);
  (*trt_parameter_map)["_allow_build_at_runtime"].set_b(
      params.allow_build_at_runtime);
  return Status::OK();
}

// Runs TRTOptimizer grappler pass.
Status RunTfTrt(const MetaGraphDef& meta_graph_def,
                const std::vector<std::string>& input_names,
                const std::vector<std::string>& output_names,
                const RewriterConfig& rewriter_config,
                GraphDef* segmented_graph_def) {
  ConfigProto config_proto;
  config_proto.mutable_graph_options()->mutable_rewrite_options()->CopyFrom(
      rewriter_config);

  VLOG(4) << "Setting up Grappler parameters\n" << config_proto.DebugString();

  TF_RETURN_IF_ERROR(RunGrappler(meta_graph_def, input_names, output_names,
                                 config_proto, segmented_graph_def));

  return Status::OK();
}

// Sets the _profile_generation mode attribute of all TRTEngineOp nodes in the
// graph to mode.
Status SetProfileGenerationMode(GraphDef* graph_def, bool mode) {
  VLOG(3) << "Setting _profile_generation_mode=" << mode;
  std::string op{"TRTEngineOp"};
  for (auto& node : *(graph_def->mutable_node())) {
    if (!op.compare(node.op())) {
      auto* attr = node.mutable_attr();
      AttrValue profile_generation_mode;
      profile_generation_mode.set_b(mode);
      (*attr)["_profile_generation_mode"] = profile_generation_mode;
    }
  }
  return Status::OK();
}

Status RunSession(Session* session, const std::vector<std::string>& input_names,
                  const std::vector<std::string>& output_names,
                  const std::vector<Tensor>& input_tensors,
                  string prefix = "") {
  std::vector<std::pair<std::string, tensorflow::Tensor>> input_pairs;
  std::vector<std::string> prefixed_output_names;
  auto prefixed_name = [](std::string prefix, std::string name) {
    return prefix.size() > 0 ? absl::StrJoin({prefix, name}, "/") : name;
  };
  for (int i = 0; i < input_names.size(); i++) {
    input_pairs.push_back(
        {prefixed_name(prefix, input_names.at(i)), input_tensors.at(i)});
  }
  for (int i = 0; i < output_names.size(); i++) {
    prefixed_output_names.push_back(prefixed_name(prefix, output_names.at(i)));
  }
  std::vector<tensorflow::Tensor> output_tensors;
  for (int i = 0; i < output_names.size(); i++) {
    output_tensors.push_back({});
  }
  VLOG(3) << "TF-TRT Build mode: running inference\n";
  TF_RETURN_IF_ERROR(
      session->Run(input_pairs, prefixed_output_names, {}, &output_tensors));
  return Status::OK();
}

// Runs the model to create the engines. In dynamic shape mode, before creating
// the engines, we provide shapes to define optimization profiles.
Status Build(GraphDef& segmented_graph_def,
             const std::vector<std::string>& input_names,
             const std::vector<std::string>& output_names,
             const std::vector<std::vector<tensorflow::Tensor>>& inputs,
             Session* session, const TfTrtConversionParams params) {
  VLOG(2) << "Building the model";
  bool need_collect_profiles = params.use_dynamic_shape && inputs.size() > 1;
  if (need_collect_profiles) {
    TF_RETURN_IF_ERROR(SetProfileGenerationMode(&segmented_graph_def, true));
  }
  TF_RETURN_IF_ERROR(session->Create(segmented_graph_def));
  string prefix = "";
  if (need_collect_profiles) {
    for (auto const& input : inputs) {
      TF_RETURN_IF_ERROR(RunSession(session, input_names, output_names, input));
    }
    prefix = "TrtBuildStep";
    TF_RETURN_IF_ERROR(SetProfileGenerationMode(&segmented_graph_def, false));
    VLOG(3) << "Importing graph with _profile_generation_mode disabled";
    TF_RETURN_IF_ERROR(
        ImportGraphDefToSession(session, segmented_graph_def, prefix));
  }
  TF_RETURN_IF_ERROR(
      RunSession(session, input_names, output_names, *inputs.begin(), prefix));
  return Status::OK();
}

// Returns the resource manager associated with the node.
Status GetResourceManager(const NodeDef& node, Session* session,
                          ResourceMgr** rm) {
  const DeviceMgr* device_mgr;
  TF_RETURN_IF_ERROR(session->LocalDeviceManager(&device_mgr));
  Device* device;
  string device_name = node.device().empty()
                           ? "/job:localhost/replica:0/task:0/device:GPU:0"
                           : node.device();
  TF_RETURN_IF_ERROR(device_mgr->LookupDevice(device_name, &device));
  *rm = device->resource_manager();
  return Status::OK();
}

// Looks up the cache resurce associated with the TRT node.
Status GetEngineCacheResource(const NodeDef& node, Session* session,
                              TRTEngineCacheResource** resource) {
  ResourceMgr* rm;
  TF_RETURN_IF_ERROR(GetResourceManager(node, session, &rm));

  absl::string_view resource_name = node.name();
  size_t last_slash = resource_name.find_last_of('/');
  if (last_slash != absl::string_view::npos) {
    resource_name.remove_prefix(last_slash + 1);
  }
  const std::string container(kTfTrtContainerName);
  *resource = nullptr;
  TF_RETURN_IF_ERROR(
      rm->Lookup(container, std::string(resource_name), resource));
  if (resource == nullptr || (*resource)->cache_.size() == 0) {
    return errors::Internal("Engine cache not found for", resource_name);
  }
  return Status::OK();
}

// Looks up the engine from the engine cache, and serializes the engine.
Status ReadSerializedEngine(
    const NodeDef& node, Session* session,
    TrtUniquePtrType<nvinfer1::IHostMemory>* engine_data) {
  TRTEngineCacheResource* resource;
  TF_RETURN_IF_ERROR(GetEngineCacheResource(node, session, &resource));
  core::ScopedUnref unref_cache_res(resource);
  if (resource->cache_.size() > 1) {
    return errors::Internal(
        "Multiple engines found, but we can only serialize one");
  }
  const std::unique_ptr<EngineContext>& engine =
      resource->cache_.begin()->second;
  if (!engine) {
    return errors::Internal("Engine not found for", node.name());
  }

  if (engine->cuda_engine) {
    // Serialize the engine.
    engine_data->reset(engine->cuda_engine->serialize());
  } else {
    LOG(WARNING) << "Engine cache contains nullptr";
  }

  return Status::OK();
}

// Saves the TRT engines as attributes of the TRTEngineOp nodes.
Status ConvertToStaticEngine(const GraphDef graph_def,
                             GraphDef* static_graph_def, Session* session) {
  static_graph_def->CopyFrom(graph_def);
  VLOG(1) << "Saving TRT engines as static engine";
  std::string op{"TRTEngineOp"};
  for (auto& node : *(static_graph_def->mutable_node())) {
    if (!op.compare(node.op())) {
      VLOG(2) << "Saving TRT engine for " << node.name()
              << ", device: " << node.device();
      TrtUniquePtrType<nvinfer1::IHostMemory> engine_data;
      TF_RETURN_IF_ERROR(ReadSerializedEngine(node, session, &engine_data));
      auto* attr = node.mutable_attr();
      AttrValue static_engine;
      static_engine.set_b(true);
      AttrValue engine_string;
      if (engine_data) {
        engine_string.set_s(engine_data->data(), engine_data->size());
      }
      (*attr)["static_engine"] = static_engine;
      (*attr)["serialized_segment"] = engine_string;
    }
  }
  return Status::OK();
}

Status ValidateConversionParams(const TfTrtConversionParams& p, int n_inputs) {
  if (p.precision_mode == TrtPrecisionMode::INT8 && p.use_calibration) {
    return errors::InvalidArgument(
        "Calibration not yet implemented through the C++ interface. Please use "
        "our Python API for calibration.");
  }
  if (p.convert_to_static_engine && n_inputs == 0) {
    return errors::InvalidArgument(
        "TRT Engine needs to be built before we can convert it to static "
        "engine. Please provide input data to build the model.");
  }
  if (!p.convert_to_static_engine && n_inputs >= 0) {
    // After the conversion, the session that was used to build the engines
    // will be destroyed. If we do not convert the engine to static engine,
    // then we loose the engines.
    //
    // TODO(tfeher): Provide a way to save dynamic engines and remove this
    // warning.
    LOG(WARNING)
        << "Skipping build mode because we cannot save the "
           "engines. Use convert_to_static_engines=true conversion "
           "parameter to enable build mode and save the engines in the graph.";
  }
  if (!p.allow_build_at_runtime && n_inputs == 0) {
    LOG(WARNING)
        << "TRT will not be used since allow_build_at_runtime is disabled and "
           "no inputs are provided to build during conversion.";
  }
  return Status::OK();
}
}  // namespace

StatusOr<GraphDef> ConvertAndBuild(
    const GraphDef& frozen_graph_def, const std::vector<string>& input_names,
    const std::vector<string>& output_names,
    const std::vector<std::vector<tensorflow::Tensor>>& inputs,
    const TfTrtConversionParams& conv_params) {
  TF_RETURN_IF_ERROR(ValidateConversionParams(conv_params, inputs.size()));
  MetaGraphDef meta_graph;
  meta_graph.mutable_graph_def()->CopyFrom(frozen_graph_def);

  RewriterConfig rewriter_config;
  TF_RETURN_IF_ERROR(GetTrtRewriterConfig(conv_params, &rewriter_config));
  GraphDef segmented_graph_def;
  TF_RETURN_IF_ERROR(RunTfTrt(meta_graph, input_names, output_names,
                              rewriter_config, &segmented_graph_def));

  GraphDef output;

  if (inputs.size() > 0 && conv_params.convert_to_static_engine) {
    // The TRTOptimization pass has inserted placeholder TRTEngineOps. Here we
    // trigger conversion by inferring the graph.
    std::unique_ptr<tensorflow::Session> session(
        tensorflow::NewSession(tensorflow::SessionOptions()));
    if (!session.get()) {
      return errors::Internal("Failed to create build session");
    }

    TF_RETURN_IF_ERROR(Build(segmented_graph_def, input_names, output_names,
                             inputs, session.get(), conv_params));

    TF_RETURN_IF_ERROR(
        ConvertToStaticEngine(segmented_graph_def, &output, session.get()));
  } else {
    output.CopyFrom(segmented_graph_def);
  }
  VLOG(1) << "TF-TRT conversion finished";
  return output;
}

}  // namespace tensorrt
}  // namespace tensorflow

#endif  // GOOGLE_CUDA && GOOGLE_TENSORRT
