// Copyright 2021 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_TINT_TRANSFORM_DECOMPOSE_MEMORY_ACCESS_H_
#define SRC_TINT_TRANSFORM_DECOMPOSE_MEMORY_ACCESS_H_

#include <string>

#include "src/tint/ast/internal_attribute.h"
#include "src/tint/transform/transform.h"

// Forward declarations
namespace tint {
class CloneContext;
}  // namespace tint

namespace tint::transform {

/// DecomposeMemoryAccess is a transform used to replace storage and uniform
/// buffer accesses with a combination of load, store or atomic functions on
/// primitive types.
class DecomposeMemoryAccess final : public Castable<DecomposeMemoryAccess, Transform> {
  public:
    /// Intrinsic is an InternalAttribute that's used to decorate a stub function
    /// so that the HLSL transforms this into calls to
    /// `[RW]ByteAddressBuffer.Load[N]()` or `[RW]ByteAddressBuffer.Store[N]()`,
    /// with a possible cast.
    class Intrinsic final : public Castable<Intrinsic, ast::InternalAttribute> {
      public:
        /// Intrinsic op
        enum class Op {
            kLoad,
            kStore,
            kAtomicLoad,
            kAtomicStore,
            kAtomicAdd,
            kAtomicSub,
            kAtomicMax,
            kAtomicMin,
            kAtomicAnd,
            kAtomicOr,
            kAtomicXor,
            kAtomicExchange,
            kAtomicCompareExchangeWeak,
        };

        /// Intrinsic data type
        enum class DataType {
            kU32,
            kF32,
            kI32,
            kVec2U32,
            kVec2F32,
            kVec2I32,
            kVec3U32,
            kVec3F32,
            kVec3I32,
            kVec4U32,
            kVec4F32,
            kVec4I32,
        };

        /// Constructor
        /// @param program_id the identifier of the program that owns this node
        /// @param o the op of the intrinsic
        /// @param sc the storage class of the buffer
        /// @param ty the data type of the intrinsic
        Intrinsic(ProgramID program_id, Op o, ast::StorageClass sc, DataType ty);
        /// Destructor
        ~Intrinsic() override;

        /// @return a short description of the internal attribute which will be
        /// displayed as `@internal(<name>)`
        std::string InternalName() const override;

        /// Performs a deep clone of this object using the CloneContext `ctx`.
        /// @param ctx the clone context
        /// @return the newly cloned object
        const Intrinsic* Clone(CloneContext* ctx) const override;

        /// @return true if op is atomic
        bool IsAtomic() const;

        /// The op of the intrinsic
        const Op op;

        /// The storage class of the buffer this intrinsic operates on
        ast::StorageClass const storage_class;

        /// The type of the intrinsic
        const DataType type;
    };

    /// Constructor
    DecomposeMemoryAccess();
    /// Destructor
    ~DecomposeMemoryAccess() override;

    /// @param program the program to inspect
    /// @param data optional extra transform-specific input data
    /// @returns true if this transform should be run for the given program
    bool ShouldRun(const Program* program, const DataMap& data = {}) const override;

  protected:
    /// Runs the transform using the CloneContext built for transforming a
    /// program. Run() is responsible for calling Clone() on the CloneContext.
    /// @param ctx the CloneContext primed with the input program and
    /// ProgramBuilder
    /// @param inputs optional extra transform-specific input data
    /// @param outputs optional extra transform-specific output data
    void Run(CloneContext& ctx, const DataMap& inputs, DataMap& outputs) const override;

    struct State;
};

}  // namespace tint::transform

#endif  // SRC_TINT_TRANSFORM_DECOMPOSE_MEMORY_ACCESS_H_
