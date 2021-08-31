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

// Configuration for distributed TPU jobs

#include "tensorflow/core/tpu/graph_rewrite/distributed_tpu_configuration_rewrite_pass.h"

#include <unordered_map>

#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/core/common_runtime/device_set.h"
#include "tensorflow/core/common_runtime/function.h"
#include "tensorflow/core/common_runtime/graph_constructor.h"
#include "tensorflow/core/common_runtime/optimization_registry.h"
#include "tensorflow/core/framework/node_def_builder.h"
#include "tensorflow/core/framework/partial_tensor_shape.h"
#include "tensorflow/core/graph/graph.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/lib/strings/str_util.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/public/session_options.h"
#include "tensorflow/core/tpu/graph_rewrite/distributed_tpu_rewrite_helpers.h"
#include "tensorflow/core/tpu/kernels/tpu_compile_op_options.h"
#include "tensorflow/core/tpu/tpu_init_mode.h"
#include "tensorflow/core/util/device_name_utils.h"
#include "tensorflow/core/util/dump_graph.h"

namespace tensorflow {
namespace {

constexpr char kIdentityOp[] = "Identity";
constexpr char kConfigureOp[] = "ConfigureDistributedTPU";
constexpr char kInternalConfigureOp[] = "_ConfigureDistributedTPU";
constexpr char kWaitOp[] = "_WaitForDistributedTPU";
constexpr char kHostConfigureOp[] = "_InitializeHostForDistributedTPU";
constexpr char kGlobalTPUArrayOp[] = "_SetGlobalTPUArray";
constexpr char kShutdownOp[] = "ShutdownDistributedTPU";
constexpr char kInternalShutdownOp[] = "_ShutdownDistributedTPU";
constexpr char kHostDisconnectOp[] = "_DisconnectHostFromDistributedTPUSystem";
constexpr char kEmbeddingConfigurationAttr[] = "embedding_config";
constexpr int kDefaultStartupTimeout = 20;

Status AddConfigurationNode(const string& configuration_device_name,
                            int number_of_hosts, Graph* graph,
                            bool enable_whole_mesh_compilations,
                            Node** configuration_node) {
  NodeDef config_def;
  config_def.set_name(graph->NewName("configure_distributed_tpu"));
  config_def.set_op(kInternalConfigureOp);
  config_def.set_device(configuration_device_name);
  AddNodeAttr("N", number_of_hosts, &config_def);
  AddNodeAttr("enable_whole_mesh_compilations", enable_whole_mesh_compilations,
              &config_def);
  // TODO(shikharagarwal): Fill with appropriate original node debug info.

  Status status;
  *configuration_node = graph->AddNode(config_def, &status);
  if (!status.ok()) {
    return status;
  }
  (*configuration_node)->set_assigned_device_name(configuration_device_name);
  return Status::OK();
}

Status AddHostConfigNode(const string& host_device_name,
                         Node* configuration_node, Graph* graph,
                         bool enable_whole_mesh_compilations,
                         Node** host_configuration_node) {
  NodeDef host_config_def;
  host_config_def.set_name(graph->NewName("configure_tpu_host"));
  host_config_def.set_op(kHostConfigureOp);
  host_config_def.set_device(host_device_name);
  AddNodeAttr("enable_whole_mesh_compilations", enable_whole_mesh_compilations,
              &host_config_def);
  MergeDebugInfo(NodeDebugInfo(configuration_node->def()), &host_config_def);

  Status status;
  *host_configuration_node = graph->AddNode(host_config_def, &status);
  if (!status.ok()) {
    return status;
  }
  (*host_configuration_node)->set_assigned_device_name(host_device_name);
  graph->AddEdge(configuration_node, 0, *host_configuration_node, 0);
  return Status::OK();
}

Status AddWaitNode(const string& configuration_device_name,
                   const std::vector<Node*>& host_configuration_nodes,
                   Graph* graph, Node** wait_node) {
  NodeDef wait_def;
  wait_def.set_name(graph->NewName("wait_for_distributed_tpu_system"));
  wait_def.set_op(kWaitOp);
  wait_def.set_device(configuration_device_name);
  AddNodeAttr("N", static_cast<int32>(host_configuration_nodes.size()),
              &wait_def);
  AddNodeAttr("startup_timeout_sec", kDefaultStartupTimeout, &wait_def);
  if (!host_configuration_nodes.empty()) {
    MergeDebugInfo(NodeDebugInfo(host_configuration_nodes[0]->def()),
                   &wait_def);
  }

  Status status;
  *wait_node = graph->AddNode(wait_def, &status);
  if (!status.ok()) {
    return status;
  }
  (*wait_node)->set_assigned_device_name(configuration_device_name);
  // Get the inputs from the host configuration nodes.
  for (int i = 0; i < host_configuration_nodes.size(); ++i) {
    graph->AddEdge(host_configuration_nodes[i], 0, *wait_node, i);
  }
  return Status::OK();
}

Status AddGlobalTPUArrayNode(const string& host_device_name, Node* wait_node,
                             Graph* graph, Node** global_tpu_array_node) {
  NodeDef global_tpu_array_def;
  global_tpu_array_def.set_name(graph->NewName("set_global_tpu_array"));
  global_tpu_array_def.set_op(kGlobalTPUArrayOp);
  global_tpu_array_def.set_device(host_device_name);
  MergeDebugInfo(NodeDebugInfo(wait_node->def()), &global_tpu_array_def);

  Status status;
  *global_tpu_array_node = graph->AddNode(global_tpu_array_def, &status);
  if (!status.ok()) {
    return status;
  }
  (*global_tpu_array_node)->set_assigned_device_name(host_device_name);
  graph->AddEdge(wait_node, 0, *global_tpu_array_node, 0);
  return Status::OK();
}

Status AddSynchronizationNode(
    const NodeDef& sync_node_def, const string& device_name,
    const std::vector<Node*>& global_array_id_nodes, Node* wait_node,
    const std::vector<DistributedTPURewriteHelpers::OutputDependency>&
        output_dependencies,
    Graph* graph) {
  NodeDef sync_def;
  sync_def.set_name(sync_node_def.name());
  sync_def.set_op(kIdentityOp);
  sync_def.set_device(device_name);
  AddNodeAttr("T", DT_STRING, &sync_def);
  MergeDebugInfo(NodeDebugInfo(sync_node_def), &sync_def);

  Status status;
  Node* sync_node = graph->AddNode(sync_def, &status);
  if (!status.ok()) {
    return status;
  }
  sync_node->set_assigned_device_name(device_name);
  // Add control edges from the global array id nodes.
  for (auto node : global_array_id_nodes) {
    graph->AddControlEdge(node, sync_node);
  }
  // Forward the data from the wait node.
  graph->AddEdge(wait_node, 0, sync_node, 0);
  // Replace the output edges.
  for (const DistributedTPURewriteHelpers::OutputDependency& dep :
       output_dependencies) {
    if (dep.dst_input == Graph::kControlSlot) {
      graph->AddControlEdge(sync_node, dep.dst);
    } else {
      graph->AddEdge(sync_node, dep.src_output, dep.dst, dep.dst_input);
    }
  }
  return Status::OK();
}


Status AddShutdownNode(
    const NodeDef& shutdown_node_def, const string& shutdown_device_name,
    const std::vector<DistributedTPURewriteHelpers::OutputDependency>&
        output_dependencies,
    Graph* graph, Node** shutdown_node) {
  NodeDef shutdown_def;
  shutdown_def.set_name(shutdown_node_def.name());
  shutdown_def.set_op(kInternalShutdownOp);
  shutdown_def.set_device(shutdown_device_name);
  MergeDebugInfo(NodeDebugInfo(shutdown_node_def), &shutdown_def);

  Status status;
  *shutdown_node = graph->AddNode(shutdown_def, &status);
  if (!status.ok()) {
    return status;
  }
  (*shutdown_node)->set_assigned_device_name(shutdown_device_name);
  // Replace the output control edges.
  for (const DistributedTPURewriteHelpers::OutputDependency& dep :
       output_dependencies) {
    if (dep.dst_input != Graph::kControlSlot) {
      return errors::Internal("Shutdown node had non-control edge output");
    }
    graph->AddControlEdge(*shutdown_node, dep.dst);
  }
  return Status::OK();
}

Status AddHostDisconnectNode(const string& host_device_name,
                             const std::vector<Node*>& input_dependencies,
                             Node* post_disconnect_node, int output_index,
                             Graph* graph) {
  NodeDef host_disconnect_def;
  host_disconnect_def.set_name(graph->NewName("disconnect_tpu_host"));
  host_disconnect_def.set_op(kHostDisconnectOp);
  host_disconnect_def.set_device(host_device_name);
  MergeDebugInfo(NodeDebugInfo(post_disconnect_node->def()),
                 &host_disconnect_def);

  Status status;
  Node* host_disconnect_node = graph->AddNode(host_disconnect_def, &status);
  if (!status.ok()) {
    return status;
  }
  host_disconnect_node->set_assigned_device_name(host_device_name);
  // Replace the input control edges.
  for (Node* src_node : input_dependencies) {
    graph->AddControlEdge(src_node, host_disconnect_node);
  }
  if (output_index == -1) {
    graph->AddControlEdge(host_disconnect_node, post_disconnect_node);
  } else {
    graph->AddEdge(host_disconnect_node, 0, post_disconnect_node, output_index);
  }
  return Status::OK();
}

}  // namespace

Status DistributedTPUConfigurationRewritePass::Run(
    const GraphOptimizationPassOptions& options) {
  VLOG(1) << "DistributedTPUConfigurationRewritePass::Run";

  Graph* graph = options.graph->get();

  if (VLOG_IS_ON(1)) {
    DumpGraphToFile("distributed_tpu_configuration_before", *graph,
                    options.flib_def);
  }

  // This pass can only run in the session master, which should fill
  // in the device_set field to the options.
  TF_RET_CHECK(options.device_set != nullptr);

  TF_RETURN_IF_ERROR(
      DistributedTPURewriteHelpers::ForConfigurationNodeMatchingType(
          kConfigureOp, graph, *options.device_set,
          [](const NodeDef& configuration_node_def,
             const string& configuration_device_name,
             const std::vector<Device*>& host_devices,
             const std::vector<Node*>& input_dependencies,
             const std::vector<DistributedTPURewriteHelpers::OutputDependency>&
                 output_dependencies,
             Graph* graph) -> Status {
            const std::string& embedding_attr_string = GetNodeAttrString(
                AttrSlice(configuration_node_def), kEmbeddingConfigurationAttr);

            if (!embedding_attr_string.empty()) {
              return errors::InvalidArgument("embedding_config must be empty.");
            }

            bool is_global_init = false;
            bool enable_whole_mesh_compilations = false;
            TF_RETURN_IF_ERROR(GetNodeAttr(configuration_node_def,
                                           "is_global_init", &is_global_init));
            TryGetNodeAttr(configuration_node_def,
                           "enable_whole_mesh_compilations",
                           &enable_whole_mesh_compilations);
            TF_RETURN_IF_ERROR(SetTPUInitMode(
                is_global_init ? TPUInitMode::kGlobal : TPUInitMode::kRegular));

            bool compilation_failure_closes_chips;
            TF_RETURN_IF_ERROR(GetNodeAttr(configuration_node_def,
                                           "compilation_failure_closes_chips",
                                           &compilation_failure_closes_chips));
            internal::SetTpuCompilationFailureClosesChips(
                compilation_failure_closes_chips);

            // Add the global TPU system configuration node.
            Node* configuration_node;
            TF_RETURN_IF_ERROR(AddConfigurationNode(
                configuration_device_name, host_devices.size(), graph,
                enable_whole_mesh_compilations, &configuration_node));

            // Add the host disconnect nodes.
            for (int i = 0; i < host_devices.size(); ++i) {
              const auto host_device = host_devices[i];
              TF_RETURN_IF_ERROR(
                  AddHostDisconnectNode(host_device->name(), input_dependencies,
                                        configuration_node, i, graph));
            }

            // Add the host configuration nodes.
            std::vector<Node*> host_configuration_nodes;
            for (const auto host_device : host_devices) {
              Node* host_configuration_node;
              TF_RETURN_IF_ERROR(AddHostConfigNode(
                  host_device->name(), configuration_node, graph,
                  enable_whole_mesh_compilations, &host_configuration_node));
              host_configuration_nodes.push_back(host_configuration_node);
            }

            // Add the node to wait for the system configuration to
            // stabilize. Use the name of the original dummy Op in case it was
            // the target of a Session::Run call.
            Node* wait_node;
            TF_RETURN_IF_ERROR(AddWaitNode(configuration_device_name,
                                           host_configuration_nodes, graph,
                                           &wait_node));

            // Add the nodes to set the global TPU ids at each host.
            std::vector<Node*> global_array_id_nodes;
            for (const auto host_device : host_devices) {
              Node* global_array_id_node;
              TF_RETURN_IF_ERROR(AddGlobalTPUArrayNode(host_device->name(),
                                                       wait_node, graph,
                                                       &global_array_id_node));
              global_array_id_nodes.push_back(global_array_id_node);
            }

            if (host_devices.empty()) {
              return errors::InvalidArgument("TPU job contains no CPU devices");
            }
            TF_RET_CHECK(!host_devices.empty());

            TF_RETURN_IF_ERROR(AddSynchronizationNode(
                configuration_node_def, host_devices.front()->name(),
                global_array_id_nodes, wait_node, output_dependencies, graph));

            return Status::OK();
          }));

  if (VLOG_IS_ON(1)) {
    DumpGraphToFile("distributed_tpu_configuration_after", *graph,
                    options.flib_def);
  }

  VLOG(1) << "DistributedTPUConfigurationRewritePass::Run() finished";
  return Status::OK();
}

Status DistributedTPUShutdownRewritePass::Run(
    const GraphOptimizationPassOptions& options) {
  VLOG(1) << "DistributedTPUShutdownRewritePass::Run";

  Graph* graph = options.graph->get();

  if (VLOG_IS_ON(1)) {
    DumpGraphToFile("distributed_tpu_shutdown_before", *graph,
                    options.flib_def);
  }

  // This pass can only run in the session master, which should fill
  // in the device_set field to the options.
  TF_RET_CHECK(options.device_set != nullptr);

  TF_RETURN_IF_ERROR(
      DistributedTPURewriteHelpers::ForConfigurationNodeMatchingType(
          kShutdownOp, graph, *options.device_set,
          [](const NodeDef& shutdown_node_def,
             const string& shutdown_device_name,
             const std::vector<Device*>& host_devices,
             const std::vector<Node*>& input_dependencies,
             const std::vector<DistributedTPURewriteHelpers::OutputDependency>&
                 output_dependencies,
             Graph* graph) -> Status {
            Node* shutdown_node;
            TF_RETURN_IF_ERROR(
                AddShutdownNode(shutdown_node_def, shutdown_device_name,
                                output_dependencies, graph, &shutdown_node));

            // Add the host disconnect nodes.
            for (const auto host_device : host_devices) {
              TF_RETURN_IF_ERROR(
                  AddHostDisconnectNode(host_device->name(), input_dependencies,
                                        shutdown_node, -1, graph));
            }

            return Status::OK();
          }));

  if (VLOG_IS_ON(1)) {
    DumpGraphToFile("distributed_tpu_shutdown_after", *graph, options.flib_def);
  }

  VLOG(1) << "DistributedTPUShutdownRewritePass::Run() finished";
  return Status::OK();
}

}  // namespace tensorflow
