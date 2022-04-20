/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_DTENSOR_CC_DTENSOR_DEVICE_UTIL_H_
#define TENSORFLOW_DTENSOR_CC_DTENSOR_DEVICE_UTIL_H_

#include <string>
#include <utility>

#include "tensorflow/c/eager/c_api.h"
#include "tensorflow/c/eager/parallel_device/parallel_device_lib.h"
#include "tensorflow/c/eager/tfe_context_internal.h"
#include "tensorflow/core/common_runtime/composite_device.h"
#include "tensorflow/core/common_runtime/eager/context.h"
#include "tensorflow/core/framework/function.h"
#include "tensorflow/core/framework/function.pb.h"
#include "tensorflow/core/framework/node_def_builder.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/graph/graph.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/fingerprint.h"
#include "tensorflow/dtensor/cc/dstatus.h"
#include "tensorflow/dtensor/cc/tensor_layout.h"

namespace tensorflow {
namespace dtensor {

#define RETURN_STATUS(status, code, message)   \
  {                                            \
    TF_SetStatus((status), (code), (message)); \
    return;                                    \
  }

#define RETURN_C_STATUS_IF_NOT_OK(cpp_status, c_status)                   \
  {                                                                       \
    auto return_if_not_ok_status = (cpp_status);                          \
    if (!return_if_not_ok_status.ok()) {                                  \
      RETURN_STATUS((c_status),                                           \
                    static_cast<TF_Code>(return_if_not_ok_status.code()), \
                    return_if_not_ok_status.error_message().c_str());     \
    }                                                                     \
  }

// Using a counter to uniquify instead of a new block allows `var` to declare a
// new variable.
#define ASSIGN_OR_RETURN_C_STATUS(var, cpp_status, c_status)               \
  ASSIGN_OR_RETURN_C_STATUS_IMPL(                                          \
      TF_STATUS_MACROS_CONCAT_NAME(_dtensor_status_or_value, __COUNTER__), \
      var, cpp_status, c_status)

#define ASSIGN_OR_RETURN_C_STATUS_IMPL(statusor, var, cpp_status, c_status) \
  auto statusor = (cpp_status);                                             \
  RETURN_C_STATUS_IF_NOT_OK(statusor.status(), (c_status));                 \
  var = std::move(statusor.ValueOrDie());

struct TranslatedFunction {
  // Mesh for which specified function will run.
  Mesh function_mesh;

  // StatefulPartitionedCall op to run the mesh function.
  const Node* node_to_execute = nullptr;

  // Maps i-th local input index to input index in global graph.
  std::vector<int> input_index_map;

  // Maps i-th local output to output index of global graph.
  std::vector<int> output_index_map;

  std::string translated_function_name;
  // For resource ops, layouts of resource handles are inferred lazily
  // during SPMD expansion of resource assign ops. In that case,
  // inferred layouts of resource handles are attached to arg nodes
  // of the returned graph.
  std::map<int, Layout> resource_input_layouts;
  // Record some metadata for output of a shape op. This would help recover
  // local shape on future operations over the Tensor.
  std::map<int, Layout> shape_output_metadata;
  std::vector<Layout> output_layouts;
  // Local shapes inferred for function outputs; these may be partially known.
  std::vector<PartialTensorShape> local_output_shapes;
  // Output data types.
  std::vector<TF_DataType> output_dtypes;
};

struct ExecutionFunctions {
  // Stores information about all functions to execute for provided computation.
  std::vector<TranslatedFunction> function_list;
  // Number of device ids args added to translated functions.
  // During translation, we insert one device id arg node per mesh.
  // For a single mesh function, it equals 1.
  // For a multi-mesh function (e.g. pipelining), it equals the number of
  // meshes.
  int num_device_ids;

  // Mesh fingerprint of function_list. Set only when ExecutionFunctions refers
  // to a function for performance reason, since an eager op doesn't use it.
  uint64 function_mesh_fingerprint = 0;
};

// TODO(yujingzhang): move FingerprintCat128 to tensorflow/platform.
inline tensorflow::Fprint128 FingerprintCat128(const tensorflow::Fprint128& a,
                                               const tensorflow::Fprint128& b) {
  return {tensorflow::FingerprintCat64(a.low64, b.low64),
          tensorflow::FingerprintCat64(a.high64, b.high64)};
}

inline tensorflow::Fprint128 FingerprintCat128(const tensorflow::Fprint128& a,
                                               const int64 b) {
  auto x = tensorflow::FingerprintCat64(a.low64, b);
  return {x, tensorflow::FingerprintCat64(a.high64, x)};
}

struct DTensorOperation {
  // For both fields: not owned. lifetime covers the whole usage.
  const char* name;
  const FunctionDef* function_def;

  inline bool is_func() const { return function_def != nullptr; }
};

// Contains a mesh bundled with a parallel device over all of the devices in
// that mesh.
class MeshWithParallelDevice {
 public:
  MeshWithParallelDevice(
      const Mesh& mesh_config,
      std::unique_ptr<parallel_device::ParallelDevice> parallel_device,
      const std::string& composite_device_name = "")
      : mesh_config_(mesh_config),
        parallel_device_(std::move(parallel_device)),
        composite_device_name_(composite_device_name),
        // Device IDs are constructed lazily because we don't have a context
        // until we start executing ops.
        device_ids_tensor_(nullptr) {}

  // A parallel tensor containing scalar integer device IDs for underlying
  // devices, each placed on its corresponding device.
  //
  // TODO(allenl): It would be nice if DeviceID worked as an op inside the
  // function's graph. Then we wouldn't need to feed it as an argument.
  parallel_device::ParallelTensor* DeviceIDs(TFE_Context* context,
                                             TF_Status* status) const;
  const parallel_device::ParallelDevice& parallel_device() const {
    return *parallel_device_;
  }

  const dtensor::Mesh& mesh_config() const { return mesh_config_; }

  // Creates a CompositeDevice in eager context if it not exists.
  // Called when parallel_device_ contains a subset of global devices, e.g.
  // pipelining is enabled.
  StatusOr<CompositeDevice*> FindOrCreateCompositeDevice(TFE_Context* context) {
    if (composite_device_ == nullptr && !composite_device_name_.empty()) {
      if (mesh_config_.global_devices().empty()) {
        return errors::InvalidArgument(
            "Expect non-empty global devices when creating a CompositeDevice.");
      }
      TF_RETURN_IF_ERROR(ContextFromInterface(tensorflow::unwrap(context))
                             ->FindOrCreateCompositeDevice(
                                 mesh_config_.global_devices(),
                                 composite_device_name_, &composite_device_));
    }
    return composite_device_;
  }

  CompositeDevice* composite_device() const { return composite_device_; }

 private:
  dtensor::Mesh mesh_config_;
  std::unique_ptr<parallel_device::ParallelDevice> parallel_device_;

  // Set when parallel_device_ contains a subset of global devices, e.g.
  // pipelining is enabled.
  const std::string composite_device_name_;
  // A tensorflow::Device that represents underlying devices of
  // parallel_device_. Set when composite_device_name_ is not empty.
  CompositeDevice* composite_device_ = nullptr;  // owned by eager context

  // Constructed lazily; contains a parallel tensor with scalar integer device
  // IDs for each device.
  mutable std::unique_ptr<parallel_device::ParallelTensor> device_ids_tensor_;
};

enum TensorType {
  kDense = 0,
  kResource = 1,
  kSparse = 2,
};

class TensorWithLayout {
 public:
  // Broadcast a single non-parallel tensor onto `mesh` with a fully replicated
  // sharding spec. Does not take ownership of `tensor`.
  static std::unique_ptr<TensorWithLayout> Broadcast(
      TFE_Context* context, TFE_TensorHandle* tensor,
      const MeshWithParallelDevice& mesh,
      const std::string& dtensor_device_name, TF_Status* status);

  // Given an already-parallel tensor, wraps it with a mesh and a layout.
  static StatusOr<std::unique_ptr<TensorWithLayout>> Wrap(
      std::unique_ptr<parallel_device::ParallelTensor> tensor,
      const MeshWithParallelDevice& mesh, const Layout& layout);

  // A dummy TensorWithLayout without holding a ParallelTensor.
  static std::unique_ptr<TensorWithLayout> Dummy(
      const std::vector<int64_t>& local_shape, const TF_DataType dtype,
      const MeshWithParallelDevice& mesh, const Layout& layout);

  virtual ~TensorWithLayout() {}

  virtual const Layout& layout() const { return layout_; }

  virtual TensorType tensor_type() const { return TensorType::kDense; }

  virtual TF_DataType dtype() const {
    if (dtype_.has_value()) {
      return dtype_.value();
    } else {
      return tensor_->dtype();
    }
  }

  // Small constant value optimization for non-resource-handle tensors.
  virtual void set_const_value(const NodeDef& const_node) {
    const_value_.emplace(const_node);
  }

  // Clears the cached const value if present.
  void reset_const_value() { const_value_.reset(); }

  // Encodes the NodeDef via provided builder, if applicable.
  virtual void EncodeAttributes(tensorflow::NodeDefBuilder& builder) const {}

  virtual tensorflow::Fprint128 CacheKey() const;

  // Updates layout for this Tensor.
  virtual void UpdateLayout(const Layout& new_layout, TF_Status* status) {
    TF_SetStatus(status, TF_INTERNAL,
                 "Attempt to update layout on non-resource-handle");
  }

  // Update shape and dtype.
  virtual void UpdateShapeAndDType(const TensorShapeProto& shape,
                                   const DataType& dtype, TF_Status* status) {
    TF_SetStatus(status, TF_INTERNAL,
                 "Attempt to update shape and layout on non-resource-handle");
  }

  virtual TFE_TensorHandle* get_tensor(size_t index) const {
    return tensor()->tensor(index);
  }

  virtual size_t num_tensors() const { return tensor()->num_tensors(); }

  virtual parallel_device::ParallelTensor* tensor() const {
    return tensor_.get();
  }

  // Returns a string which includes just the value and layout of the tensor.
  virtual std::string SummarizeValue() const;
  // Returns a string which includes `SummarizeValue` along with shape and type
  // information.
  virtual std::string DebugString() const;

  void set_input_layout_for_shape_op_result(const Layout& layout) {
    input_layout_for_shape_op_result_.emplace(layout);
  }

  const absl::optional<Layout> shape_metadata_layout() const {
    return input_layout_for_shape_op_result_;
  }

  const MeshWithParallelDevice& mesh() const { return mesh_; }

  // Compute global shape from layout & local tensor shape.
  //
  // For replicated layout tensors, global shape is simply the shape of local
  // tensors on each device. For sharded tensor, this is the global shape
  // encodes layout & local shape on each device.
  const std::vector<int64_t> global_shape() const {
    return layout().GlobalShapeFromLocalShape(local_shape());
  }

  const std::vector<int64_t> local_shape() const { return local_shape_; }

  const absl::optional<NodeDef> const_value() const { return const_value_; }

 protected:
  TensorWithLayout(std::unique_ptr<parallel_device::ParallelTensor> tensor,
                   const MeshWithParallelDevice& mesh, const Layout& layout,
                   std::vector<int64_t> local_shape,
                   absl::optional<TF_DataType> dtype = absl::nullopt,
                   absl::optional<NodeDef> const_value = absl::nullopt)
      : tensor_(std::move(tensor)),
        layout_(layout),
        mesh_(mesh),
        const_value_(std::move(const_value)),
        local_shape_(local_shape),
        dtype_(dtype) {}

  std::unique_ptr<parallel_device::ParallelTensor> tensor_;

  Layout layout_;

  const MeshWithParallelDevice& mesh_;

  // Optionally holds the value of a small, non-resource tensor. Small constants
  // are directly folded into the SPMD graph instead of being passed as inputs.
  // This provides extra information to the layout propagation and SPMD passes
  // during op-by-op execution. (For example, the reduction indices for Sum,
  // target shapes for Rng/Reshape, etc).
  absl::optional<NodeDef> const_value_;

  // Optionally holds the original input layout for a shape Op returned Tensor.
  // This is used to preserve information for a shape op output so that future
  // uses could recover local shape.
  // TODO(hthu,allenl,xiejw): Move this into a separate class for clarity.
  absl::optional<Layout> input_layout_for_shape_op_result_ = absl::nullopt;

  // The local shape of tensors placed on each of `tensor_`'s component devices.
  std::vector<int64_t> local_shape_;

  absl::optional<TF_DataType> dtype_;
};

// Extension of TensorWithLayout which holds resource handle with layout.
//
// The major differences are
// 1. The layout, shape, dtype are lazily set as they are unavailable upon
//    creation.
// 2. Small const optimization should be disabled.
class ResourceHandleWithLayout : public TensorWithLayout {
 public:
  // The layout of uninitialized resource tensors, or the layout of the tensor
  // contained in an initialized resource.
  const Layout& layout() const override {
    return dereferenced_layout_.has_value() ? dereferenced_layout_.value()
                                            : layout_;
  }

  TensorType tensor_type() const override { return TensorType::kResource; }

  void set_const_value(const NodeDef& const_node) override {
    // Just a no-op for resource handle. Maybe we should error out.
  }

  void EncodeAttributes(tensorflow::NodeDefBuilder& builder) const override;

  tensorflow::Fprint128 CacheKey() const override;

  void UpdateLayout(const Layout& new_layout, TF_Status* status) override;

  void UpdateShapeAndDType(const TensorShapeProto& shape, const DataType& dtype,
                           TF_Status* status) override {
    set_dereferenced_shape(shape);
    set_dereferenced_dtype(dtype);
  }

  void set_dereferenced_shape(const TensorShapeProto& shape) {
    dereferenced_shape_.emplace(shape);
  }
  void set_dereferenced_dtype(const DataType& dtype) {
    dereferenced_dtype_.emplace(dtype);
  }

  const absl::optional<TensorShapeProto>& dereferenced_shape() const {
    return dereferenced_shape_;
  }
  const absl::optional<DataType>& dereferenced_dtype() const {
    return dereferenced_dtype_;
  }

 public:
  ResourceHandleWithLayout(
      std::unique_ptr<parallel_device::ParallelTensor> tensor,
      const MeshWithParallelDevice& mesh, const Layout& layout,
      std::vector<int64_t> local_shape)
      : TensorWithLayout(std::move(tensor), mesh, layout, local_shape,
                         TF_RESOURCE) {}

 private:
  // The layout of the tensor pointed to by this handle, if any.
  absl::optional<Layout> dereferenced_layout_;
  // The shape and dtype of the tensor pointed to by this resource tensor.
  absl::optional<TensorShapeProto> dereferenced_shape_;
  absl::optional<DataType> dereferenced_dtype_;
};

// TensorWithLayout for SparseTensors.
//
// The main difference between this and TensorWithLayout is this
// contains 3 lists of tensors as opposed to one (values, indices, shapes).
// The shapes of the SparseTensors will always be the dense view of the shapes,
// and thus will have no difference with the TensorWithLayout in terms of
// shapes.
class SparseTensorWithLayout : public TensorWithLayout {
 public:
  static StatusOr<std::unique_ptr<TensorWithLayout>> Wrap(
      std::unique_ptr<parallel_device::ParallelTensor> indices_tensor,
      std::unique_ptr<parallel_device::ParallelTensor> values_tensor,
      std::unique_ptr<parallel_device::ParallelTensor> shapes_tensor,
      const MeshWithParallelDevice& mesh, const Layout& layout,
      std::vector<int64_t> local_shape);

  // A dummy TensorWithLayout without holding a ParallelTensor.
  static std::unique_ptr<TensorWithLayout> Dummy(
      const std::vector<int64_t>& local_shape,
      const MeshWithParallelDevice& mesh, const Layout& layout) {
    return std::unique_ptr<TensorWithLayout>(new SparseTensorWithLayout(
        /*indices=*/nullptr, /*values=*/nullptr, /*dense_shapes=*/nullptr, mesh,
        layout, local_shape));
  }

  void set_const_value(const NodeDef& const_node) override {
    // No-op for SparseTensors, consider erroring out.
  }

  // Add attribute '_sparse' to the NodeDefBuilder so that the mlir::Value
  // that originate from SparseTensorWithLayout are marked as '_sparse'.
  void EncodeAttributes(tensorflow::NodeDefBuilder& builder) const override {
    builder.Attr("_sparse", true);
  }

  TensorType tensor_type() const override { return TensorType::kSparse; }

  size_t num_tensors() const override { return 3 * indices()->num_tensors(); }

  TFE_TensorHandle* get_tensor(size_t index) const override;

  std::string SummarizeValue() const override;

  std::string DebugString() const override;

  TF_DataType dtype() const override;

  parallel_device::ParallelTensor* indices() const { return indices_.get(); }

  parallel_device::ParallelTensor* values() const { return values_.get(); }

  parallel_device::ParallelTensor* dense_shapes() const {
    return dense_shapes_.get();
  }

 protected:
  SparseTensorWithLayout(
      std::unique_ptr<parallel_device::ParallelTensor> indices,
      std::unique_ptr<parallel_device::ParallelTensor> values,
      std::unique_ptr<parallel_device::ParallelTensor> dense_shapes,
      const MeshWithParallelDevice& mesh, const Layout& layout,
      std::vector<int64_t> local_shape,
      absl::optional<TF_DataType> dtype = absl::nullopt,
      absl::optional<NodeDef> const_value = absl::nullopt)
      : TensorWithLayout(nullptr, mesh, layout, local_shape),
        indices_(std::move(indices)),
        values_(std::move(values)),
        dense_shapes_(std::move(dense_shapes)) {}
  std::unique_ptr<parallel_device::ParallelTensor> indices_;
  std::unique_ptr<parallel_device::ParallelTensor> values_;
  std::unique_ptr<parallel_device::ParallelTensor> dense_shapes_;
};

template <typename T>
std::string ShapeToDebugString(const std::vector<T> shape_vector) {
  std::vector<tensorflow::int64> cast_shape(shape_vector.begin(),
                                            shape_vector.end());
  tensorflow::PartialTensorShape shape;
  if (!tensorflow::PartialTensorShape::MakePartialShape(
           cast_shape.data(), cast_shape.size(), &shape)
           .ok()) {
    return "<error displaying shape>";
  } else {
    return shape.DebugString();
  }
}

// Returns the shape of a given tensor.
std::vector<int64_t> TensorShapeAsVector(TFE_TensorHandle* tensor,
                                         TF_Status* status);

// Creates a Graph with _Arg and _Retval nodes surrounding an
// `operation_name`-type node.
Status PrepareGraphForMlir(
    const std::vector<TensorWithLayout*>& inputs,
    const DTensorOperation& doperation,
    const tensorflow::FunctionLibraryDefinition& flib_def,
    const NameAttrList& attributes,
    const absl::optional<Layout>& default_layout, tensorflow::Graph* graph,
    std::vector<PartialTensorShape>* global_output_shapes,
    std::vector<const Layout*>* output_layouts);

// Returns set of functions to run to execute DTensor computation.
StatusOr<ExecutionFunctions> IdentifyAllFunctionsToExecute(
    const tensorflow::Graph& graph,
    const std::vector<PartialTensorShape>& global_output_shapes);

// For functions with control outputs, add identity nodes between
// StatefulPartitionedCall and _Retvals, in order to preserve control output
// dependencies after StatefulPartitionedCall is inlined at runtime.
// Consider calling this in PrepareGraphForMlir, once the identity nodes won't
// be dropped during MLIR lowering.
// TODO(b/171265131): fix the underlying issue to avoid inserting identity
// nodes.
Status MaybeInsertIdentityNodes(const FunctionDef* function_def, Graph* graph);

// Add DTensor specific function attributes to be compatible with eager runtime.
void AddDTensorFunctionAttr(FunctionDef& function_def);

}  // namespace dtensor
}  // namespace tensorflow

#endif  // TENSORFLOW_DTENSOR_CC_DTENSOR_DEVICE_UTIL_H_
