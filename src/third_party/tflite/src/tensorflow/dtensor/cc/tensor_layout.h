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

#ifndef TENSORFLOW_DTENSOR_CC_TENSOR_LAYOUT_H_
#define TENSORFLOW_DTENSOR_CC_TENSOR_LAYOUT_H_

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/strings/string_view.h"
#include "tensorflow/core/common_runtime/device_mgr.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/dtensor/cc/dstatus.h"
#include "tensorflow/dtensor/proto/layout.pb.h"
#include "tensorflow/stream_executor/lib/statusor.h"

// Definitions for DTensor mesh & layout.
//
// A mesh describes how a set of devices is partitioned.
// A layout describes how a distributed tensor is partitioned across a mesh (and
// thus across devices). Defining tensor layouts in terms of mesh dimensions
// allows us to efficiently determine the communication required when computing
// an operation with tensors of different layouts.
namespace tensorflow {
namespace dtensor {

// The location of a device in a mesh.
//
// Each device has a unique location in the mesh, which is indicated by the
// offset in each mesh dimension. e.g. a mesh:
//
// [x:4, y:3, z:2]
//
// Must consist of 24 devices placed densely into the corresponding 3D space.
using DeviceLocation = absl::InlinedVector<int64, 4>;

// A shard refers to a partition of a tensor. Shards are arranged in
// ShardVectors that contains a list of Shards and a list of integers
// representing the number of shards in each dimension.
//
// Example: layout = sharding_specs:x,y, mesh:|x=2,y=2|. This can be represented
// with a ShardVector:
//          - shards = (1,1), (1,2), (2,1), (2,2)
//          - num_shards_per_dim = (2,2).
//
// The number of elements in each shard matches the tensor rank.
using Shard = std::vector<int>;

struct ShardVector {
  bool operator==(const ShardVector& other) const;
  bool operator!=(const ShardVector& other) const { return !(*this == other); }
  std::string ToString() const;

  bool ContainsShard(const Shard& shard) const;

  std::vector<Shard> shards;
  std::vector<int> num_shards_per_dim;
};

struct MeshDimension {
  MeshDimension(const std::string& name, int64 size)
      : name(std::move(name)), size(size) {}
  MeshDimension() = default;

  std::string name;
  int64 size;
};

class Mesh {
 public:
  // Failed serialized strings are represented with en empty string, therefore
  // we use this string representation of an empty mesh instead to avoid
  // confusion.
  static constexpr const char* kEmptyMeshString = "empty_mesh";
  static Mesh Empty();
  // Parses from MeshProto.
  static StatusOr<Mesh> ParseFromProto(const MeshProto& proto);
  // Parses from a human readable string version of the mesh, currently used
  // to represent meshes in MLIR:
  //  mesh = <name|List[MeshDim]|List[GlobalId]|List[LocalId]|List[Devices]>
  //
  // Example:
  //  mesh =
  //  <name|x=2,y=2|0,1,2,3|0,1,2,3|/job:localhost/task:0/device:CPU:0,/job:localhost/task:0/device:CPU:1,/job:localhost/task:0/device:CPU:2,/job:localhost/task:0/device:CPU:3>
  static StatusOr<Mesh> FromString(const std::string& str);
  // Creates mesh without specific devices associated to it (aka abstract mesh).
  // This is an experimental API. Use only if strictly needed.
  static StatusOr<Mesh> GetAbstractMesh(
      const std::string& name, const std::vector<MeshDimension>& mesh_dims);
  // Creates fully defined mesh.
  static StatusOr<Mesh> GetMesh(
      const std::string& name, const std::vector<MeshDimension>& mesh_dims,
      const std::vector<std::int64_t>& global_device_ids,
      const std::vector<std::int64_t>& local_device_ids,
      const std::vector<std::string>& local_devices,
      const std::vector<std::string>& global_devices);

  Mesh() = default;

  bool IsEmpty() const;

  // Device information methods.
  std::string device_type() const;
  std::vector<std::string> hosts() const;

  bool is_cpu_mesh() const { return device_type() == "CPU"; }
  bool is_epu_mesh() const { return device_type() == "EPU"; }
  bool is_tpu_mesh() const { return device_type() == "TPU"; }

  // Returns the index of MeshDimension in mesh where the mesh dimension name is
  // `mesh_name`.
  int GetMeshDimIndexWithName(const std::string& mesh_name) const;
  bool IsMeshDim(const std::string& dim_name) const;

  const std::string& name() const { return name_; }
  const MeshDimension& dim(int64 index) const { return mesh_dims_[index]; }
  const std::string& dim_name(int64 index) const {
    return mesh_dims_[index].name;
  }

  // Consumes a location in the mesh and returns its corresponding index in
  // the flattened list of devices.
  int64 GetFlattenedCoordinate(const DeviceLocation& loc) const;
  // Takes an index in the flattened list of devices and returns a location
  // in the mesh.
  StatusOr<const DeviceLocation> device_location(int offset) const;

  std::vector<MeshDimension> dims() const { return mesh_dims_; }

  // Parses names of local_devices according to TF's Device Name Utils.
  StatusOr<const std::vector<DeviceNameUtils::ParsedName>> ParsedDevices()
      const;
  absl::Span<const std::string> local_devices() const { return local_devices_; }
  absl::Span<const int64_t> local_device_ids() const {
    return local_device_ids_;
  }

  int64_t min_global_device_id() const {
    DCHECK(!global_device_ids_.empty());
    return *std::min_element(global_device_ids_.begin(),
                             global_device_ids_.end());
  }

  absl::Span<const int64_t> global_device_ids() const {
    return global_device_ids_;
  }

  const std::vector<std::string>& global_devices() const {
    return global_devices_;
  }

  // Returns whether the mesh is a remote mesh.
  bool is_remote() const {
    return local_device_ids_.empty() && !global_device_ids_.empty();
  }

  // Returns index of given dim_name in the mesh.
  StatusOr<int32> idx_for_dim(absl::string_view dim_name) const;

  // Returns size of mesh dimension.
  StatusOr<int64> dim_size(absl::string_view name) const;

  // Returns list of mesh dimension sizes.
  std::vector<int64> dim_sizes() const;

  int64 num_devices() const;
  int64 rank() const;
  int64 size() const;

  std::string ToString() const;

  // Global unique fingerprint. Same on different workers.
  uint64 GlobalFingerprint() const;

  bool operator==(const Mesh& b) const;
  bool operator!=(const Mesh& b) const { return !((*this) == b); }
  bool operator<(const Mesh& b) const {
    return this->ToString() < b.ToString();
  }

  template <typename H>
  friend H AbslHashValue(H h, const Mesh& m) {
    return H::combine(std::move(h), m.ToString());
  }

  MeshProto ToProto() const;

  // A map from mesh names to their corresponding core ID mappings. The core ID
  // mapping is stored as a vector. The i-th element in the vector is the ID of
  // the core represented by global device ID of i in this mesh.
  //
  // The entry stored under the empty name key (the so-called "default mapping"
  // in some comments) is special. Is is always set at the end of TPU
  // initialization. It represents the mapping for any mesh whose global device
  // IDs follow TF task-device ordinals. Legacy and test meshes created without
  // using the `create_tpu_mesh` helper follow that rule and can use this entry.
  static std::map<std::string, std::vector<int>>& tpu_core_ids();

  // The host mesh associated with any user-defined TPU mesh.
  static std::string& tpu_host_mesh();

 private:
  std::string name_;
  std::vector<MeshDimension> mesh_dims_;
  std::vector<std::string> local_devices_;
  std::vector<int64_t> local_device_ids_;
  std::vector<int64_t> global_device_ids_;
  std::vector<std::string> global_devices_;
};

class Layout {
 public:
  static constexpr const char* kUnshardedDim = "unsharded";
  // This spec should only be used to express no preferred sharding in the
  // Layout propagation algorithm.
  static constexpr const char* kAny = "any";
  // Failed serialized strings are represented with en empty string, therefore
  // we use this string representation of an empty layout instead to avoid
  // confusion.
  static constexpr const char* kEmptyLayoutString = "empty_layout";
  // Used for the relayout operation, to allow relayout act as an identity on
  // the layout for the given dimension.
  static constexpr const char* kMatch = "match";

  // Returns empty layout.
  static Layout Empty();

  // Parses from LayoutProto.
  static StatusOr<Layout> FromProto(const LayoutProto& proto);
  // Parses from a human readable string version of the layout, currently used
  // to represent layouts in MLIR:
  //  layout = <sharding_specs:List[specs] mesh:name|List[MeshDim]|
  //  List[GlobalId]|List[LocalId]|List[Devices]>
  //
  // Example:
  //  layout = <sharding_specs:x,not_sharded mesh:name|x=2,y=2|0,1,2,3|0,1,2,3|
  //  /job:localhost/task:0/device:CPU:0,/job:localhost/task:0/device:CPU:1,
  //  /job:localhost/task:0/device:CPU:2,/job:localhost/task:0/device:CPU:3>
  static StatusOr<Layout> FromString(std::string layout_str);
  static Layout ReplicatedOnMesh(const Mesh& mesh, int rank);
  static Layout AnyOnMesh(const Mesh& mesh, int rank);

  // Returns a layout for the transposed matrix for given layout. This assumes
  // that only the last two dimensions are used for matrix computation and all
  // dimensions before are batch dimensions.
  static StatusOr<Layout> Transposed2D(const Layout& layout);
  static bool IsUnshardedDimension(const absl::string_view name) {
    return name == kUnshardedDim;
  }
  static bool IsShardedDimension(const absl::string_view name) {
    return !IsUnshardedDimension(name);
  }
  static bool IsUnshardedSpec(const ShardingSpec& spec) {
    return IsUnshardedDimension(spec.sharding_spec());
  }
  static bool IsShardedSpec(const ShardingSpec& spec) {
    return !IsUnshardedDimension(spec.sharding_spec());
  }
  static StatusOr<Layout> GetLayout(
      const std::vector<std::string>& sharding_spec_strs, const Mesh& mesh);
  static StatusOr<Layout> GetLayout(
      const std::vector<ShardingSpec>& sharding_specs, const Mesh& mesh);

  // Makes a new layout from this one dropping the given dimensions.
  // If keep_dims is true, the dimensions are replicated rather than
  // deleted.
  Layout GetLayoutWithReducedDims(const absl::flat_hash_set<int>& reduced_dims,
                                  bool keep_dims) const;

  // Truncates a layout at the front or back, depending on the value of end.
  // end = false returns the layout upto the split point,
  // end = true returns the layout from the split point.
  Layout Truncate(int64 split_point, bool end = false) const;

  // Left or right pad the layout to a max rank.
  Layout LeftPad(int64 rank) const;

  bool IsFullyReplicated() const;
  bool IsLastDimReplicated() const;
  // Checks that the last N-1 dimensions are replicated
  bool IsBatchParallel() const;
  // Checks that the dimensions from [-non_batch_rank, end) are replicaed
  bool IsBatchParallel(int non_batch_rank) const;
  bool IsEmpty() const;

  // Compute global shape using the layout and provided local_shape.
  std::vector<int64_t> GlobalShapeFromLocalShape(
      const std::vector<int64_t>& local_shape) const;

  std::vector<int64_t> LocalShapeFromGlobalShape(
      absl::Span<const int64_t> global_shape) const;
  PartialTensorShape LocalShapeFromGlobalShape(
      const PartialTensorShape& global_shape) const;

  int64 rank() const { return sharding_specs_.size(); }
  int64 num_devices() const { return mesh_.num_devices(); }
  size_t num_shards_for_dim(const ShardingSpec& dim) const;
  std::vector<int32> num_shards() const;

  const ShardingSpec& dim(int64 idx) const { return sharding_specs_[idx]; }
  absl::Span<const ShardingSpec> sharding_specs() const {
    return sharding_specs_;
  }

  // Creates a mesh of unique shards.
  Mesh ReducedMesh() const;

  // Computes the corresponding shard vector to this layout.
  ShardVector GetShardVector() const;

  // Map hosts to shards.
  std::map<std::string, ShardVector> HostShardMap() const;

  // Returns sharding specs in string form.
  std::vector<std::string> sharding_spec_strs() const;

  // Creates human readable string version of a layout.
  std::string ToString() const;
  LayoutProto ToProto() const;

  StatusOr<const DeviceLocation> device_location(int64 device_id) const {
    return mesh_.device_location(device_id);
  }

  void set_mesh(Mesh mesh) { mesh_ = mesh; }

  const Mesh& mesh() const { return mesh_; }
  const std::string& sharding_spec(int idx) const;

  bool operator==(const Layout& b) const;
  bool operator!=(const Layout& b) const { return !((*this) == b); }
  bool operator<(const Layout& b) const {
    return this->ToString() < b.ToString();
  }

 private:
  std::vector<ShardingSpec> sharding_specs_;
  Mesh mesh_;
};

// Takes two layouts and concatenates their TensorDimensions. If the meshes for
// the two layouts are different or both layouts are using the same mesh
// dimension returns an error rather than a layout.
StatusOr<Layout> ConcatenateLayouts(const Layout& layout_a,
                                    const Layout& layout_b);

}  // namespace dtensor
}  // namespace tensorflow

#endif  // TENSORFLOW_DTENSOR_CC_TENSOR_LAYOUT_H_
