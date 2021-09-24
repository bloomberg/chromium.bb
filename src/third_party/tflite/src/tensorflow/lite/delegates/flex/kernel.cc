/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

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
#include "tensorflow/lite/delegates/flex/kernel.h"

#include "flatbuffers/flexbuffers.h"  // from @flatbuffers
#include "tensorflow/core/common_runtime/eager/context.h"
#include "tensorflow/core/common_runtime/eager/execute.h"
#include "tensorflow/core/common_runtime/eager/tensor_handle.h"
#include "tensorflow/core/framework/node_def.pb.h"
#include "tensorflow/core/framework/node_def_util.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/lite/builtin_ops.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/context_util.h"
#include "tensorflow/lite/core/api/profiler.h"
#include "tensorflow/lite/delegates/flex/delegate.h"
#include "tensorflow/lite/delegates/flex/delegate_data.h"
#include "tensorflow/lite/delegates/flex/util.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/minimal_logging.h"
#include "tensorflow/lite/string_type.h"

// Note: this is part of TF Lite's Flex delegation code which is to be
// completed soon.

// This is the TF Lite op that is created by the flex delegate to handle
// execution of a supported subgraph. The usual flow is that the delegate
// informs the interpreter of supported nodes in a graph, and each supported
// subgraph is replaced with one instance of this kernel.
//
// The kernel is initialized with TfLiteDelegateParams from which we retrieve
// the global EagerContext and BufferMap, as well as a list of inputs and
// outputs to the subgraph. Those are used to build the OpData, with a list of
// TensorFlow Ops that should be executed in order (which we call an OpNode).
//
// For each node included in the subgraph, we query the interpreter and
// retrieve the associated NodeDef, which is then used to configure the
// corresponding TensorFlow/Eager Op.

using tensorflow::shape_inference::DimensionHandle;
using tensorflow::shape_inference::InferenceContext;
using tensorflow::shape_inference::ShapeAndType;
using tensorflow::shape_inference::ShapeHandle;

const std::string GetDimsDebugString(const TfLiteIntArray* dims) {
  return absl::StrCat("[", absl::StrJoin(tflite::TfLiteIntArrayView(dims), ","),
                      "]");
}

namespace tflite {
namespace flex {

struct OpNode;

// Represents the origin of a given tensor as a reference to the output
// of an upstream node.
struct TensorSource {
  OpNode* node;
  int node_output_index;
};

// A list of inputs of a given node of the TensorFlow/Eager graph.
class OpInputs {
 public:
  explicit OpInputs(const TfLiteIntArray* indexes) {
    for (int index : TfLiteIntArrayView(indexes)) {
      inputs_.push_back(index);
    }
    forwardable_.resize(inputs_.size());
  }
  ~OpInputs() {}

  int Size() const { return inputs_.size(); }

  int TfLiteIndex(int i) const { return inputs_[i]; }

  // Given a map relating tensors to the node that originates them, populate a
  // list of sources for the tensors in this class.
  void InitializeTensorSources(
      const std::map<int, TensorSource>& tflite_tensor_sources) {
    sources_.clear();
    for (int i : inputs_) {
      auto it = tflite_tensor_sources.find(i);
      if (it == tflite_tensor_sources.end()) {
        sources_.push_back({nullptr, 0});
      } else {
        sources_.push_back(it->second);
      }
    }
  }

  void SetForwardable(int i, bool v) { forwardable_[i] = v; }

  bool IsForwardable(int i) const { return forwardable_[i]; }

  TensorSource GetTensorSource(int i) const { return sources_[i]; }

 private:
  std::vector<int> inputs_;
  std::vector<TensorSource> sources_;

  // List of tensors that can be used by TF in its forwarding optimization.
  // Doing so allows an input tensor to be modified and used as the output
  // tensor. The delegate takes care of not holding any references to tensors
  // in this list while Eager is executing the corresponding op.
  std::vector<int> forwardable_;
};

// A list of outputs of a given node of the TensorFlow/Eager graph, along with
// the actual outputs of the EagerOperation.
class OpOutputs {
 public:
  explicit OpOutputs(const TfLiteIntArray* indexes) {
    for (int index : TfLiteIntArrayView(indexes)) {
      outputs_.push_back(index);
    }
    vector_.resize(outputs_.size());
  }
  ~OpOutputs() { ResetTensorHandles(); }

  // Stores information about which of the tensors in this class are also
  // outputs of the sugbraph.
  void InitializeGraphOutputs(const std::set<int>& subgraph_outputs) {
    subgraph_outputs_.clear();
    for (int i : outputs_) {
      subgraph_outputs_.push_back(subgraph_outputs.count(i) > 0);
    }
  }

  // Returns true if the tensor given by index 'i' is an output of the entire
  // subgraph.
  bool IsSubgraphOutput(int i) const { return subgraph_outputs_[i]; }

  // Returns a handle to a given tensor and, optionally, remove it from the
  // internal vector.
  tensorflow::TensorHandle* GetHandle(int i, bool remove) {
    auto* handle = vector_[i];
    if (!remove) {
      handle->Ref();
    } else {
      // Don't increase the ref-count. Instead, simply take it out of the
      // vector.
      vector_[i] = nullptr;
    }
    return handle;
  }

  int Size() const { return outputs_.size(); }

  int TfLiteIndex(int i) const { return outputs_[i]; }

  // Carefully unreference all the handles in the eager output vector.
  void ResetTensorHandles() {
    for (int i = 0; i < vector_.size(); ++i) {
      if (vector_[i]) {
        vector_[i]->Unref();
        vector_[i] = nullptr;
      }
    }
  }

  tensorflow::gtl::InlinedVector<tensorflow::TensorHandle*, 2>*
  GetTensorHandles() {
    return &vector_;
  }

 private:
  std::vector<int> outputs_;
  std::vector<bool> subgraph_outputs_;
  tensorflow::gtl::InlinedVector<tensorflow::TensorHandle*, 2> vector_;
};

// A single node within the larger 'op'. Note that this kernel executes many
// TensorFlow ops within a single TF Lite op.
class OpNode {
 public:
  OpNode(const TfLiteIntArray* inputs, const TfLiteIntArray* outputs)
      : inputs_(inputs), outputs_(outputs) {}
  ~OpNode() {
    if (op_) ClearEagerInputs();
  }

  const string& name() const { return name_; }
  void set_name(const string& name) { name_ = name; }

  int index() const { return index_; }
  void set_index(int index) { index_ = index; }

  const tensorflow::NodeDef& nodedef() const { return nodedef_; }
  const tensorflow::OpRegistrationData* op_reg_data() const {
    return op_reg_data_;
  }

  const OpInputs& inputs() const { return inputs_; }
  OpInputs* mutable_inputs() { return &inputs_; }

  const OpOutputs& outputs() const { return outputs_; }
  OpOutputs* mutable_outputs() { return &outputs_; }

  int NumInputs() const { return inputs_.Size(); }
  int NumOutputs() const { return outputs_.Size(); }

  tensorflow::EagerOperation* op() { return op_.get(); }

  tensorflow::Status InitializeNodeDef(const void* custom_initial_data,
                                       int custom_initial_data_size) {
    if (!custom_initial_data) {
      return tensorflow::errors::Internal(
          "Cannot convert empty data into a valid NodeDef");
    }
    // The flexbuffer contains a vector where the first elements is the
    // op name and the second is a serialized NodeDef.
    const flexbuffers::Vector& v =
        flexbuffers::GetRoot(
            reinterpret_cast<const uint8_t*>(custom_initial_data),
            custom_initial_data_size)
            .AsVector();

    name_ = v[0].AsString().str();
    if (!nodedef_.ParseFromString(v[1].AsString().str())) {
      nodedef_.Clear();
      return tensorflow::errors::Internal(
          "Failed to parse data into a valid NodeDef");
    }

    // Fill NodeDef with defaults if it's a valid op.
    TF_RETURN_IF_ERROR(
        tensorflow::OpRegistry::Global()->LookUp(nodedef_.op(), &op_reg_data_));
    AddDefaultsToNodeDef(op_reg_data_->op_def, &nodedef_);

    return tensorflow::Status::OK();
  }

  // Build thew new EagerOperation. In case of error, the returned 'op' is
  // guaranteed to be 'nullptr'.
  tensorflow::Status BuildEagerOp(
      tensorflow::EagerContext* eager_context,
      tensorflow::CancellationManager* cancellation_manager) {
    op_.reset(new tensorflow::EagerOperation(eager_context));
    TF_RETURN_IF_ERROR(op_->Reset(name_.c_str(), nullptr, false, nullptr));
    if (op_->is_function()) {
      op_.reset();
      return tensorflow::errors::NotFound(
          "Operation '", name_,
          "' is not registered.  (while processing attributes of '", name_,
          "')");
    }

    op_->MutableAttrs()->NumInputs(inputs_.Size());
    for (const auto& attr : nodedef_.attr()) {
      op_->MutableAttrs()->Set(attr.first, attr.second);
    }

    // Precalculating a cache key saves about 10% of inference time for very
    // small models.
    op_->MutableAttrs()->CacheKey(op_->DeviceName());

    op_->SetCancellationManager(cancellation_manager);

    return tensorflow::Status::OK();
  }

  void ClearEagerInputs() { op_->Clear(); }

  tensorflow::Status BuildEagerInputs(const BufferMap* buffer_map) {
    absl::InlinedVector<tensorflow::TensorHandle*, 4>* op_inputs;
    TF_RETURN_IF_ERROR(op_->MutableTensorHandleInputs(&op_inputs));
    for (int i = 0; i < inputs_.Size(); ++i) {
      int input_index = inputs_.TfLiteIndex(i);
      TensorSource s = inputs_.GetTensorSource(i);
      if (!s.node) {
        // This input is not produced by this Eager subgraph (it could be a TF
        // Lite native buffer, or could be produced by a separater subgraph). We
        // need to fetch it from the delegate's buffer_map.
        if (!buffer_map->HasTensor(input_index)) {
          return tensorflow::errors::Internal(
              "Cannot read from invalid tensor index ", input_index);
        }
        tensorflow::TensorHandle* handle =
            tensorflow::TensorHandle::CreateLocalHandle(
                buffer_map->GetTensor(input_index));
        op_inputs->push_back(handle);
      } else {
        // If this is a forwardable tensor, we will remove it from the previous
        // op's list, giving TF the opportunity to reuse its buffer.
        bool unref_handle = inputs_.IsForwardable(i);
        auto* handle =
            s.node->outputs_.GetHandle(s.node_output_index, unref_handle);
        op_inputs->push_back(handle);
      }
    }
    return tensorflow::Status::OK();
  }

  tensorflow::Status PersistEagerOutputs(BufferMap* buffer_map) {
    auto* handles = outputs_.GetTensorHandles();
    for (int i = 0; i < outputs_.Size(); ++i) {
      if (outputs_.IsSubgraphOutput(i)) {
        const tensorflow::Tensor* tensor = nullptr;
        TF_RETURN_IF_ERROR(handles->at(i)->Tensor(&tensor));
        buffer_map->SetFromTensorFlow(outputs_.TfLiteIndex(i), *tensor);
      }
    }
    return tensorflow::Status::OK();
  }

 private:
  OpNode(const OpNode&) = delete;
  OpNode& operator=(const OpNode&) = delete;

  // The name of the TensorFlow op to execute.
  string name_;
  // Index of this node into TF Lite's operator list.
  int index_;
  // The corresponding NodeDef, containing the attributes for the op.
  tensorflow::NodeDef nodedef_;
  // The corresponding OpRegistrationData pointer.
  const tensorflow::OpRegistrationData* op_reg_data_;
  // List of inputs, as TF Lite tensor indices.
  OpInputs inputs_;
  // List of outputs, as TF Lite tensor indices.
  OpOutputs outputs_;

  std::unique_ptr<tensorflow::EagerOperation> op_;
};

// Executes the TensorFlow op given by 'op_name', with the attributes specified
// in 'nodedef'. Inputs and outputs are given as indices into the 'buffer_map'.
tensorflow::Status ExecuteFlexOp(TfLiteContext* context, BufferMap* buffer_map,
                                 OpNode* node_data) {
  TF_RETURN_WITH_CONTEXT_IF_ERROR(node_data->BuildEagerInputs(buffer_map),
                                  " (while executing '", node_data->name(),
                                  "' via Eager)");

  node_data->mutable_outputs()->ResetTensorHandles();
  int num_retvals = node_data->NumOutputs();
  TF_RETURN_WITH_CONTEXT_IF_ERROR(
      node_data->op()->Execute(
          absl::MakeSpan(
              reinterpret_cast<tensorflow::AbstractTensorHandle**>(
                  node_data->mutable_outputs()->GetTensorHandles()->data()),
              num_retvals),
          &num_retvals),
      " (while executing '", node_data->name(), "' via Eager)");

  if (num_retvals != node_data->NumOutputs()) {
    return tensorflow::errors::Internal(
        "Unexpected number of outputs from EagerExecute");
  }

  TF_RETURN_IF_ERROR(node_data->PersistEagerOutputs(buffer_map));

  node_data->ClearEagerInputs();

  return tensorflow::Status::OK();
}

// The larger 'op', which contains all the nodes in a supported subgraph.
struct OpData {
  tensorflow::EagerContext* eager_context;
  tensorflow::CancellationManager* cancellation_manager;
  BufferMap* buffer_map;
  std::vector<std::unique_ptr<OpNode>> nodes;
  std::vector<int> subgraph_inputs;
  std::vector<int> subgraph_outputs;
};

DelegateKernel::DelegateKernel() : op_data_(new OpData) {}
DelegateKernel::~DelegateKernel() {}

TfLiteStatus DelegateKernel::Init(TfLiteContext* context,
                                  const TfLiteDelegateParams* params) {
  auto* flex_delegate_data =
      reinterpret_cast<FlexDelegate*>(params->delegate->data_)->mutable_data();
  op_data_->eager_context = flex_delegate_data->GetEagerContext();
  op_data_->cancellation_manager = flex_delegate_data->GetCancellationManager();
  op_data_->buffer_map = flex_delegate_data->GetBufferMap(context);

  CHECK(params->output_tensors);
  std::set<int> output_set;
  for (auto tensor_index : TfLiteIntArrayView(params->output_tensors)) {
    op_data_->subgraph_outputs.push_back(tensor_index);
    output_set.insert(tensor_index);
  }

  CHECK(params->input_tensors);
  for (auto tensor_index : TfLiteIntArrayView(params->input_tensors)) {
    op_data_->subgraph_inputs.push_back(tensor_index);
  }

  op_data_->nodes.reserve(params->nodes_to_replace->size);

  CHECK(params->nodes_to_replace);
  tensorflow::Status status;
  for (auto node_index : TfLiteIntArrayView(params->nodes_to_replace)) {
    TfLiteNode* node;
    TfLiteRegistration* reg;
    context->GetNodeAndRegistration(context, node_index, &node, &reg);

    op_data_->nodes.emplace_back(new OpNode(node->inputs, node->outputs));
    OpNode& node_data = *op_data_->nodes.back();

    node_data.set_index(node_index);
    node_data.set_name("");

    status = node_data.InitializeNodeDef(node->custom_initial_data,
                                         node->custom_initial_data_size);
    if (!status.ok()) break;
    status = node_data.BuildEagerOp(op_data_->eager_context,
                                    op_data_->cancellation_manager);
    if (!status.ok()) break;
  }

  TF_LITE_ENSURE_STATUS(ConvertStatus(context, status));

  // Given a TfLite tensor index, return the OpNode that produces it,
  // along with it index into that OpNodes list of outputs.
  std::map<int, TensorSource> tflite_tensor_sources;

  // Find out how each tensor is produced. This does not account for
  // tensors that are not produce by eager ops.
  for (auto& node_data : op_data_->nodes) {
    node_data->mutable_outputs()->InitializeGraphOutputs(output_set);
    for (int i = 0; i < node_data->outputs().Size(); ++i) {
      int output_index = node_data->outputs().TfLiteIndex(i);
      tflite_tensor_sources[output_index] = TensorSource{node_data.get(), i};
    }
  }

  // For each node, resolve the inputs, so we can keep pointers to the nodes
  // that produces them.
  for (auto& node_data : op_data_->nodes) {
    node_data->mutable_inputs()->InitializeTensorSources(tflite_tensor_sources);
  }
  return kTfLiteOk;
}

TfLiteStatus DelegateKernel::Prepare(TfLiteContext* context, TfLiteNode* node) {
  TF_LITE_ENSURE_MSG(
      context, op_data_->eager_context != nullptr,
      "Failed to initialize eager context. This often happens when a CPU "
      "device has not been registered, presumably because some symbols from "
      "tensorflow/core:core_cpu_impl were not linked into the binary.");

  // We will keep track of the number of references to each tensor in the
  // graph, so we can make them "forwardable" if there is only one reference.
  std::map<int, int> tensor_ref_count;

  // Whenever we find a constant tensor, insert it in the buffer map.
  BufferMap* buffer_map = op_data_->buffer_map;
  for (auto tensor_index : op_data_->subgraph_inputs) {
    TfLiteTensor* tensor = &context->tensors[tensor_index];
    if (IsConstantTensor(tensor)) {
      if (!tensor->data_is_stale || !buffer_map->HasTensor(tensor_index)) {
        buffer_map->SetFromTfLite(tensor_index, tensor);
      }
    }

    // Input tensors should never be forwarded so we increment their ref counts
    // twice: once for this graph and another for the possibility of them being
    // used by another subgraph, or being an output of the full graph.
    tensor_ref_count[tensor_index] += 2;
  }

  const bool shapes_are_valid =
      (ValidateOutputTensorShapeConsistency(context) == kTfLiteOk);
  if (shapes_are_valid) {
    TFLITE_LOG(tflite::TFLITE_LOG_INFO,
               "FlexDelegate: All tensor shapes are consistent.");
  } else {
    TFLITE_LOG(tflite::TFLITE_LOG_WARNING,
               "FlexDelegate: Some tensor shapes are inconsistent.");
  }

  // All output tensors are allocated by TensorFlow/Eager, so we
  // mark them as kTfLiteDynamic.
  for (auto tensor_index : op_data_->subgraph_outputs) {
    if (!shapes_are_valid) {
      SetTensorToDynamic(&context->tensors[tensor_index]);
    }
    ++tensor_ref_count[tensor_index];
  }

  for (const auto& node_data : op_data_->nodes) {
    if (node_data->nodedef().op().empty()) {
      context->ReportError(context, "Invalid NodeDef in Flex op '%s'",
                           node_data->name().c_str());
      return kTfLiteError;
    }
    TF_LITE_ENSURE(context, node_data->op());

    for (int i = 0; i < node_data->inputs().Size(); ++i) {
      ++tensor_ref_count[node_data->inputs().TfLiteIndex(i)];
    }
  }

  // All tensors that are referenced exactly once are marked as "forwardable",
  // meaning that we will allow TensorFlow to reuse its buffer as the output of
  // an op.
  for (auto& node_data : op_data_->nodes) {
    for (int i = 0; i < node_data->inputs().Size(); ++i) {
      bool f = (tensor_ref_count[node_data->inputs().TfLiteIndex(i)] == 1);
      node_data->mutable_inputs()->SetForwardable(i, f);
    }
  }

  return kTfLiteOk;
}

TfLiteStatus DelegateKernel::ValidateOutputTensorShapeConsistency(
    TfLiteContext* context) const {
  for (const auto& node_data : op_data_->nodes) {
    auto op_name = node_data->name().c_str();
    // Create an InferenceContext object.
    auto num_inputs = node_data->inputs().Size();
    std::vector<const tensorflow::Tensor*> input_tensors_vector(num_inputs,
                                                                nullptr);
    InferenceContext c(
        TF_GRAPH_DEF_VERSION, node_data->nodedef(),
        node_data->op_reg_data()->op_def, std::vector<ShapeHandle>(num_inputs),
        input_tensors_vector, {},
        std::vector<std::unique_ptr<std::vector<ShapeAndType>>>());

    // Set input_shapes for ShapeInferenceFn.
    for (int i = 0; i < num_inputs; ++i) {
      const auto input_tensor_index = node_data->inputs().TfLiteIndex(i);
      TfLiteTensor* tfl_tensor = &context->tensors[input_tensor_index];
      // Provide constant input tensors since some op ("RFFT") needs it to
      // calculate the output shape.
      if (IsConstantTensor(tfl_tensor)) {
        input_tensors_vector[i] =
            op_data_->buffer_map->GetTensorPtr(input_tensor_index);
      }
      const auto dims_array = tfl_tensor->dims;
      std::vector<DimensionHandle> dims(dims_array->size);
      for (int j = 0; j < dims_array->size; ++j) {
        dims[j] = c.MakeDim(dims_array->data[j]);
      }
      c.SetInput(i, c.MakeShape(dims));
    }
    c.set_input_tensors(input_tensors_vector);

    tensorflow::Status status = c.construction_status();
    if (!status.ok()) {
      TFLITE_LOG(tflite::TFLITE_LOG_WARNING,
                 "Shape construction failed for op '%s'", op_name);
      return kTfLiteError;
    }

    // Run ShapeInferenceFn to calculate output shapes.
    if (node_data->op_reg_data()->shape_inference_fn == nullptr) {
      TFLITE_LOG(tflite::TFLITE_LOG_WARNING,
                 "No shape inference function exists for op '%s'", op_name);
      return kTfLiteError;
    }
    status = c.Run(node_data->op_reg_data()->shape_inference_fn);

    // Compare calculated output shapes with node_data->outputs
    auto num_outputs = node_data->outputs().Size();
    if (num_outputs != c.num_outputs()) {
      TFLITE_LOG(tflite::TFLITE_LOG_WARNING,
                 "Number of output tensors are mismatched for op '%s' %d != %d",
                 op_name, num_outputs, c.num_outputs());
      return kTfLiteError;
    }
    for (int i = 0; i < num_outputs; ++i) {
      const auto output_tensor_index = node_data->outputs().TfLiteIndex(i);
      TfLiteTensor* tfl_tensor = &context->tensors[output_tensor_index];
      // tfl_tensor->dims only has valid information if the given model is
      // converted by the MLIR converter. Also when ResizeInputTensor() is
      // called the dims information becomes invalid.
      const std::string tfl_shape_string = GetDimsDebugString(tfl_tensor->dims);
      const std::string calculated_shape_string = c.DebugString(c.output(i));
      // Getting a shape string via c.DebugString() is the easiest way to get
      // the shape information of the given ShapeHandle for now.
      // TODO(b/169017408): Find a better approach without using debug string.
      if (tfl_shape_string != calculated_shape_string) {
        TFLITE_LOG(tflite::TFLITE_LOG_WARNING,
                   "op '%s' output%d tensor#%d shape mismatch for  %s != %s",
                   op_name, i, output_tensor_index, tfl_shape_string.c_str(),
                   calculated_shape_string.c_str());
        return kTfLiteError;
      }
    }
  }
  return kTfLiteOk;
}

TfLiteStatus DelegateKernel::Eval(TfLiteContext* context, TfLiteNode* node) {
  BufferMap* buffer_map = op_data_->buffer_map;

  // Insert a tensor in the buffer map for all inputs that are not constant.
  // Constants were handled in Prepare() already.
  for (auto tensor_index : op_data_->subgraph_inputs) {
    TfLiteTensor* tensor = &context->tensors[tensor_index];
    if (!IsConstantTensor(tensor)) {
      // If this tensor is part of an earlier TF subgraph we should not add it
      // to the BufferMap again, because TF already knows about it and its
      // contents are kept automatically up-to-date.
      if (!tensor->data_is_stale || !buffer_map->HasTensor(tensor_index)) {
        buffer_map->SetFromTfLite(tensor_index, tensor);
      }
    }
  }

  // Execute the TensorFlow Ops sequentially.
  for (auto& node_data : op_data_->nodes) {
    TFLITE_SCOPED_DELEGATE_OPERATOR_PROFILE(
        reinterpret_cast<Profiler*>(context->profiler),
        node_data->name().c_str(), node_data->index());

    if (op_data_->cancellation_manager != nullptr &&
        op_data_->cancellation_manager->IsCancelled()) {
      TF_LITE_KERNEL_LOG(context, "Client requested cancel during Invoke()");
      return kTfLiteError;
    }

    auto status = ExecuteFlexOp(context, buffer_map, node_data.get());
    TF_LITE_ENSURE_OK(context, ConvertStatus(context, status));
  }

  for (auto tensor_index : op_data_->subgraph_outputs) {
    if (!buffer_map->HasTensor(tensor_index)) {
      context->ReportError(context, "Cannot write to invalid tensor index %d",
                           tensor_index);
      return kTfLiteError;
    }

    // Copy TF tensor data to TFL allocated buffer for non dynamic tensors.
    // For dynamic tensors, copy shape and put buffer_handle for the later
    // CopyFromBufferHandle() call.
    TfLiteTensor* tensor = &context->tensors[tensor_index];
    const tensorflow::Tensor& tf_tensor = buffer_map->GetTensor(tensor_index);
    if (tensor->allocation_type == kTfLiteDynamic) {
      TF_LITE_ENSURE_OK(context, CopyShapeAndType(context, tf_tensor, tensor));
      tensor->buffer_handle = tensor_index;
      tensor->data_is_stale = true;
      continue;
    }
    // If the tensor isn't dynamic, we can copy data directly to the buffer of
    // the tensor. Before copying the data, check if the target buffer has
    // expected size.
    if (tf_tensor.NumElements() != NumElements(tensor) ||
        tf_tensor.TotalBytes() != tensor->bytes) {
      TF_LITE_KERNEL_LOG(
          context, "Tensor: %s(%d) buffer size mismatch %zu(%lld) != %ld(%ld)",
          tensor->name, tensor_index, tf_tensor.TotalBytes(),
          tf_tensor.NumElements(), tensor->bytes, NumElements(tensor));
      return kTfLiteError;
    }
    tensorflow::StringPiece t_data = tf_tensor.tensor_data();
    memcpy(tensor->data.raw, t_data.data(), t_data.size());
  }

  return kTfLiteOk;
}

}  // namespace flex
}  // namespace tflite
