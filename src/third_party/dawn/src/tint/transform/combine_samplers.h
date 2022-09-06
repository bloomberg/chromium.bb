// Copyright 2022 The Tint Authors.
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

#ifndef SRC_TINT_TRANSFORM_COMBINE_SAMPLERS_H_
#define SRC_TINT_TRANSFORM_COMBINE_SAMPLERS_H_

#include <string>
#include <unordered_map>

#include "src/tint/sem/sampler_texture_pair.h"
#include "src/tint/transform/transform.h"

namespace tint::transform {

/// This transform converts all separate texture/sampler refences in a
/// program into combined texture/samplers. This is required for GLSL,
/// which does not support separate texture/samplers.
///
/// It utilizes the texture/sampler information collected by the
/// Resolver and stored on each sem::Function. For each function, all
/// separate texture/sampler parameters in the function signature are
/// removed. For each unique pair, if both texture and sampler are
/// global variables, the function passes the corresponding combined
/// global stored in global_combined_texture_samplers_ at the call
/// site. Otherwise, either the texture or sampler must be a function
/// parameter. In this case, a new parameter is added to the function
/// signature. All separate texture/sampler parameters are removed.
///
/// All texture builtin callsites are modified to pass the combined
/// texture/sampler as the first argument, and separate texture/sampler
/// arguments are removed.
///
/// Note that the sampler may be null, indicating that only a texture
/// reference was required (e.g., textureLoad). In this case, a
/// placeholder global sampler is used at the AST level. This will be
/// combined with the original texture to give a combined global, and
/// the placeholder removed (ignored) by the GLSL writer.
///
/// Note that the combined samplers are actually represented by a
/// Texture node at the AST level, since this contains all the
/// information needed to represent a combined sampler in GLSL
/// (dimensionality, component type, etc). The GLSL writer outputs such
/// (Tint) Textures as (GLSL) Samplers.
class CombineSamplers final : public Castable<CombineSamplers, Transform> {
  public:
    /// A pair of binding points.
    using SamplerTexturePair = sem::SamplerTexturePair;

    /// A map from a sampler/texture pair to a named global.
    using BindingMap = std::unordered_map<SamplerTexturePair, std::string>;

    /// The client-provided mapping from separate texture and sampler binding
    /// points to combined sampler binding point.
    struct BindingInfo final : public Castable<Data, transform::Data> {
        /// Constructor
        /// @param map the map of all (texture, sampler) -> (combined) pairs
        /// @param placeholder the binding point to use for placeholder samplers.
        BindingInfo(const BindingMap& map, const sem::BindingPoint& placeholder);

        /// Copy constructor
        /// @param other the other BindingInfo to copy
        BindingInfo(const BindingInfo& other);

        /// Destructor
        ~BindingInfo() override;

        /// A map of bindings from (texture, sampler) -> combined sampler.
        BindingMap binding_map;

        /// The binding point to use for placeholder samplers.
        sem::BindingPoint placeholder_binding_point;
    };

    /// Constructor
    CombineSamplers();

    /// Destructor
    ~CombineSamplers() override;

  protected:
    /// The PIMPL state for this transform
    struct State;

    /// Runs the transform using the CloneContext built for transforming a
    /// program. Run() is responsible for calling Clone() on the CloneContext.
    /// @param ctx the CloneContext primed with the input program and
    /// ProgramBuilder
    /// @param inputs optional extra transform-specific input data
    /// @param outputs optional extra transform-specific output data
    void Run(CloneContext& ctx, const DataMap& inputs, DataMap& outputs) const override;
};

}  // namespace tint::transform

#endif  // SRC_TINT_TRANSFORM_COMBINE_SAMPLERS_H_
