/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the implementation of the RendererCB::StateManager class,
// including all the state handlers for the command buffer renderer.

#include "core/cross/precompile.h"
#include "core/cross/state.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "core/cross/command_buffer/states_cb.h"
#include "command_buffer/common/gapi_interface.h"

namespace o3d {
using command_buffer::CommandBufferEntry;
using command_buffer::CommandBufferHelper;

namespace {

// Converts values meant to represent a Cull Mode to the corresponding
// command-buffer value.
// Default: CULL_NONE.
command_buffer::o3d::FaceCullMode CullModeToCB(int cull) {
  switch (cull) {
    default:
    case State::CULL_NONE:
      return command_buffer::o3d::kCullNone;
    case State::CULL_CW:
      return command_buffer::o3d::kCullCW;
    case State::CULL_CCW:
      return command_buffer::o3d::kCullCCW;
  }
}

// Converts values meant to represent a Polygon Fill Mode to the corresponding
// command-buffer value.
// Default: kPolygonModeFill.
command_buffer::o3d::PolygonMode FillModeToCB(int fill) {
  switch (fill) {
    case State::POINT:
      return command_buffer::o3d::kPolygonModePoints;
    case State::WIREFRAME:
      return command_buffer::o3d::kPolygonModeLines;
    default:
    case State::SOLID:
      return command_buffer::o3d::kPolygonModeFill;
  }
}

// Converts values meant to represent a Comparison Function to the corresponding
// command-buffer value.
// Default: kAlways.
command_buffer::o3d::Comparison ComparisonToCB(int comparison) {
  switch (comparison) {
    case State::CMP_NEVER:
      return command_buffer::o3d::kNever;
    case State::CMP_LESS:
      return command_buffer::o3d::kLess;
    case State::CMP_EQUAL:
      return command_buffer::o3d::kEqual;
    case State::CMP_LEQUAL:
      return command_buffer::o3d::kLEqual;
    case State::CMP_GREATER:
      return command_buffer::o3d::kGreater;
    case State::CMP_NOTEQUAL:
      return command_buffer::o3d::kNotEqual;
    case State::CMP_GEQUAL:
      return command_buffer::o3d::kGEqual;
    case State::CMP_ALWAYS:
    default:
      return command_buffer::o3d::kAlways;
  }
}

// Converts values meant to represent a Stencil Operation to the corresponding
// command-buffer value.
// Default: kKeep.
command_buffer::o3d::StencilOp StencilOpToCB(int op) {
  switch (op) {
    default:
    case State::STENCIL_KEEP:
      return command_buffer::o3d::kKeep;
    case State::STENCIL_ZERO:
      return command_buffer::o3d::kZero;
    case State::STENCIL_REPLACE:
      return command_buffer::o3d::kReplace;
    case State::STENCIL_INCREMENT_SATURATE:
      return command_buffer::o3d::kIncNoWrap;
    case State::STENCIL_DECREMENT_SATURATE:
      return command_buffer::o3d::kDecNoWrap;
    case State::STENCIL_INVERT:
      return command_buffer::o3d::kInvert;
    case State::STENCIL_INCREMENT:
      return command_buffer::o3d::kIncWrap;
    case State::STENCIL_DECREMENT:
      return command_buffer::o3d::kDecWrap;
  }
}

// Converts values meant to represent a Blending Function to the corresponding
// command-buffer value.
// Default: kBlendFuncOne.
command_buffer::o3d::BlendFunc BlendFuncToCB(int func) {
  switch (func) {
    case State::BLENDFUNC_ZERO:
      return command_buffer::o3d::kBlendFuncZero;
    default:
    case State::BLENDFUNC_ONE:
      return command_buffer::o3d::kBlendFuncOne;
    case State::BLENDFUNC_SOURCE_COLOR:
      return command_buffer::o3d::kBlendFuncSrcColor;
    case State::BLENDFUNC_INVERSE_SOURCE_COLOR:
      return command_buffer::o3d::kBlendFuncInvSrcColor;
    case State::BLENDFUNC_SOURCE_ALPHA:
      return command_buffer::o3d::kBlendFuncSrcAlpha;
    case State::BLENDFUNC_INVERSE_SOURCE_ALPHA:
      return command_buffer::o3d::kBlendFuncInvSrcAlpha;
    case State::BLENDFUNC_DESTINATION_ALPHA:
      return command_buffer::o3d::kBlendFuncDstAlpha;
    case State::BLENDFUNC_INVERSE_DESTINATION_ALPHA:
      return command_buffer::o3d::kBlendFuncInvDstAlpha;
    case State::BLENDFUNC_DESTINATION_COLOR:
      return command_buffer::o3d::kBlendFuncDstColor;
    case State::BLENDFUNC_INVERSE_DESTINATION_COLOR:
      return command_buffer::o3d::kBlendFuncInvDstColor;
    case State::BLENDFUNC_SOURCE_ALPHA_SATUTRATE:
      return command_buffer::o3d::kBlendFuncSrcAlphaSaturate;
  }
}

// Converts values meant to represent a Blending Equation to the corresponding
// command-buffer value.
// Default: kBlendEqAdd.
command_buffer::o3d::BlendEq BlendEqToCB(int eq) {
  switch (eq) {
    default:
    case State::BLEND_ADD:
      return command_buffer::o3d::kBlendEqAdd;
    case State::BLEND_SUBTRACT:
      return command_buffer::o3d::kBlendEqSub;
    case State::BLEND_REVERSE_SUBTRACT:
      return command_buffer::o3d::kBlendEqRevSub;
    case State::BLEND_MIN:
      return command_buffer::o3d::kBlendEqMin;
    case State::BLEND_MAX:
      return command_buffer::o3d::kBlendEqMax;
  }
}

}  // anonymous namespace

// This class wraps StateHandler to make it type-safe.
template <typename T>
class TypedStateHandler : public RendererCB::StateHandler {
 public:
  // Override this function to set a specific state.
  // Parameters:
  //   renderer: The platform specific renderer.
  //   param: A concrete param with state data.
  virtual void SetStateFromTypedParam(RendererCB* renderer, T* param) const = 0;

  // Gets Class of State's Parameter
  virtual const ObjectBase::Class* GetClass() const {
    return T::GetApparentClass();
  }

 private:
  // Calls SetStateFromTypedParam if the Param type is the correct type.
  // Parameters:
  //   renderer: The renderer.
  //   param: A param with state data.
  virtual void SetState(Renderer* renderer, Param* param) const {
    RendererCB *renderer_cb = down_cast<RendererCB *>(renderer);
    // This is safe because State guarntees Params match by type.
    DCHECK(param->IsA(T::GetApparentClass()));
    SetStateFromTypedParam(renderer_cb, down_cast<T*>(param));
  }
};

// A template that generates a handler for enable/disable states.
// Template Parameters:
//   bit_field: the BitField type to access the proper bit in the
//   command-buffer argument value.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
template <typename bit_field>
class EnableStateHandler : public TypedStateHandler<ParamBoolean> {
 public:
  EnableStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamBoolean* param) const {
    bit_field::Set(value_, param->value() ? 1 : 0);
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// A template that generates a handler for bit field states.
// Template Parameters:
//   bit_field: the BitField type to access the proper bit field.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
template <typename bit_field>
class BitFieldStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  BitFieldStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    bit_field::Set(value_, param->value());
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// A handler for the color write state.
class ColorWriteStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  ColorWriteStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    using command_buffer::o3d::SetColorWrite;
    int mask = param->value();
    SetColorWrite::AllColorsMask::Set(value_, mask);
    renderer->SetWriteMask(mask);
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// A template that generates a handler for full-size values (uint32, int32,
// float).
// Template Parameters:
//   ParamType: the type of the parameter.
// Parameters:
//   value: a pointer to the argument value as the proper type.
//   dirty: a pointer to the dirty bit.
template <typename ParamType>
class ValueStateHandler : public TypedStateHandler<ParamType> {
 public:
  typedef typename ParamType::DataType DataType;
  ValueStateHandler(DataType *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamType* param) const {
    *value_ = param->value();
    *dirty_ = true;
  }
 private:
  DataType *value_;
  bool *dirty_;
};

// Handler for Cull Mode.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
class CullModeStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  CullModeStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    using command_buffer::o3d::SetPolygonRaster;
    SetPolygonRaster::CullMode::Set(value_, CullModeToCB(param->value()));
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// Handler for Polygon Fill Mode.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
class FillModeStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  FillModeStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    using command_buffer::o3d::SetPolygonRaster;
    SetPolygonRaster::FillMode::Set(value_, FillModeToCB(param->value()));
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// A template that generates a handler for comparison functions.
// Template Parameters:
//   bit_field: the BitField type to access the proper bits.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
template <typename bit_field>
class ComparisonStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  ComparisonStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    bit_field::Set(value_, ComparisonToCB(param->value()));
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// A template that generates a handler for stencil operations.
// Template Parameters:
//   bit_field: the BitField type to access the proper bits.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
template <typename bit_field>
class StencilOpStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  StencilOpStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    bit_field::Set(value_, StencilOpToCB(param->value()));
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// A template that generates a handler for blend equations.
// Template Parameters:
//   bit_field: the BitField type to access the proper bits.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
template <typename bit_field>
class BlendFuncStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  BlendFuncStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    bit_field::Set(value_, BlendFuncToCB(param->value()));
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// A template that generates a handler for blend equations.
// Template Parameters:
//   bit_field: the BitField type to access the proper bits.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
template <typename bit_field>
class BlendEqStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  BlendEqStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    bit_field::Set(value_, BlendEqToCB(param->value()));
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// Adds all the state handlers for all the states. The list of handlers must
// match in names and types the list in Renderer::AddDefaultStates()
// (in renderer.cc).
void RendererCB::StateManager::AddStateHandlers(RendererCB *renderer) {
  // Point/Line raster
  {
    bool *dirty = point_line_helper_.dirty_ptr();
    using command_buffer::o3d::SetPointLineRaster;
    SetPointLineRaster& cmd = point_line_helper_.command();
    renderer->AddStateHandler(
        State::kLineSmoothEnableParamName,
        new EnableStateHandler<
            SetPointLineRaster::LineSmoothEnable>(&cmd.enables, dirty));
    renderer->AddStateHandler(
        State::kPointSpriteEnableParamName,
        new EnableStateHandler<
            SetPointLineRaster::PointSpriteEnable>(&cmd.enables, dirty));
    renderer->AddStateHandler(State::kPointSizeParamName,
                              new ValueStateHandler<ParamFloat>(
                                  &cmd.point_size, dirty));
  }

  // Polygon Raster
  {
    bool *dirty = poly_raster_helper_.dirty_ptr();
    using command_buffer::o3d::SetPolygonRaster;
    SetPolygonRaster& cmd = poly_raster_helper_.command();
    renderer->AddStateHandler(State::kCullModeParamName,
                              new CullModeStateHandler(&cmd.fill_cull, dirty));
    renderer->AddStateHandler(State::kFillModeParamName,
                              new FillModeStateHandler(&cmd.fill_cull, dirty));
  }

  // Polygon Offset
  {
    bool *dirty = poly_offset_helper_.dirty_ptr();
    command_buffer::o3d::SetPolygonOffset& cmd =
        poly_offset_helper_.command();
    renderer->AddStateHandler(
        State::kPolygonOffset1ParamName,
        new ValueStateHandler<ParamFloat>(&cmd.slope_factor, dirty));
    renderer->AddStateHandler(
        State::kPolygonOffset2ParamName,
        new ValueStateHandler<ParamFloat>(&cmd.units, dirty));
  }

  // Alpha test
  {
    using command_buffer::o3d::SetAlphaTest;
    SetAlphaTest& cmd = alpha_test_helper_.command();
    bool *dirty = alpha_test_helper_.dirty_ptr();
    renderer->AddStateHandler(
        State::kAlphaTestEnableParamName,
        new EnableStateHandler<SetAlphaTest::Enable>(&cmd.func_enable, dirty));
    renderer->AddStateHandler(
        State::kAlphaComparisonFunctionParamName,
        new ComparisonStateHandler<SetAlphaTest::Func>(&cmd.func_enable, dirty));
    renderer->AddStateHandler(
        State::kAlphaReferenceParamName,
        new ValueStateHandler<ParamFloat>(&cmd.value, dirty));
  }

  // Depth Test
  {
    bool *dirty = depth_test_helper_.dirty_ptr();
    using command_buffer::o3d::SetDepthTest;
    SetDepthTest& cmd = depth_test_helper_.command();
    renderer->AddStateHandler(
        State::kZWriteEnableParamName,
        new EnableStateHandler<SetDepthTest::WriteEnable>(&cmd.func_enable, dirty));
    renderer->AddStateHandler(
        State::kZEnableParamName,
        new EnableStateHandler<SetDepthTest::Enable>(&cmd.func_enable, dirty));
    renderer->AddStateHandler(
        State::kZComparisonFunctionParamName,
        new ComparisonStateHandler<SetDepthTest::Func>(&cmd.func_enable, dirty));
  }

  // Stencil Test
  {
    bool *dirty = stencil_test_helper_.dirty_ptr();
    using command_buffer::o3d::SetStencilTest;
    SetStencilTest& cmd = stencil_test_helper_.command();
    renderer->AddStateHandler(
        State::kStencilEnableParamName,
        new EnableStateHandler<SetStencilTest::Enable>(&cmd.stencil_args0, dirty));
    renderer->AddStateHandler(
        State::kTwoSidedStencilEnableParamName,
        new EnableStateHandler<
            SetStencilTest::SeparateCCW>(&cmd.stencil_args0, dirty));
    renderer->AddStateHandler(
        State::kStencilReferenceParamName,
        new BitFieldStateHandler<
            SetStencilTest::ReferenceValue>(&cmd.stencil_args0, dirty));
    renderer->AddStateHandler(
        State::kStencilMaskParamName,
        new BitFieldStateHandler<
            SetStencilTest::CompareMask>(&cmd.stencil_args0, dirty));
    renderer->AddStateHandler(
        State::kStencilWriteMaskParamName,
        new BitFieldStateHandler<
            SetStencilTest::WriteMask>(&cmd.stencil_args0, dirty));

    renderer->AddStateHandler(
        State::kStencilComparisonFunctionParamName,
        new ComparisonStateHandler<
            SetStencilTest::CWFunc>(&cmd.stencil_args1, dirty));
    renderer->AddStateHandler(
        State::kStencilPassOperationParamName,
        new StencilOpStateHandler<
            SetStencilTest::CWPassOp>(&cmd.stencil_args1, dirty));
    renderer->AddStateHandler(
        State::kStencilFailOperationParamName,
        new StencilOpStateHandler<
            SetStencilTest::CWFailOp>(&cmd.stencil_args1, dirty));
    renderer->AddStateHandler(
        State::kStencilZFailOperationParamName,
        new StencilOpStateHandler<
            SetStencilTest::CWZFailOp>(&cmd.stencil_args1, dirty));

    renderer->AddStateHandler(
        State::kCCWStencilComparisonFunctionParamName,
        new ComparisonStateHandler<
            SetStencilTest::CCWFunc>(&cmd.stencil_args1, dirty));

    renderer->AddStateHandler(
        State::kCCWStencilPassOperationParamName,
        new StencilOpStateHandler<
            SetStencilTest::CCWPassOp>(&cmd.stencil_args1, dirty));
    renderer->AddStateHandler(
        State::kCCWStencilFailOperationParamName,
        new StencilOpStateHandler<
            SetStencilTest::CCWFailOp>(&cmd.stencil_args1, dirty));
    renderer->AddStateHandler(
        State::kCCWStencilZFailOperationParamName,
        new StencilOpStateHandler<
            SetStencilTest::CCWZFailOp>(&cmd.stencil_args1, dirty));
  }

  // Blending
  {
    bool *dirty = blending_helper_.dirty_ptr();
    using command_buffer::o3d::SetBlending;
    SetBlending& cmd = blending_helper_.command();
    renderer->AddStateHandler(
        State::kAlphaBlendEnableParamName,
        new EnableStateHandler<SetBlending::Enable>(&cmd.blend_settings, dirty));
    renderer->AddStateHandler(
        State::kSeparateAlphaBlendEnableParamName,
        new EnableStateHandler<SetBlending::SeparateAlpha>(&cmd.blend_settings, dirty));

    renderer->AddStateHandler(
        State::kSourceBlendFunctionParamName,
        new BlendFuncStateHandler<
            SetBlending::ColorSrcFunc>(&cmd.blend_settings, dirty));
    renderer->AddStateHandler(
        State::kDestinationBlendFunctionParamName,
        new BlendFuncStateHandler<
            SetBlending::ColorDstFunc>(&cmd.blend_settings, dirty));
    renderer->AddStateHandler(
        State::kBlendEquationParamName,
        new BlendEqStateHandler<
            SetBlending::ColorEq>(&cmd.blend_settings, dirty));
    renderer->AddStateHandler(
        State::kSourceBlendAlphaFunctionParamName,
        new BlendFuncStateHandler<
            SetBlending::AlphaSrcFunc>(&cmd.blend_settings, dirty));
    renderer->AddStateHandler(
        State::kDestinationBlendAlphaFunctionParamName,
        new BlendFuncStateHandler<
            SetBlending::AlphaDstFunc>(&cmd.blend_settings, dirty));
    renderer->AddStateHandler(
        State::kBlendAlphaEquationParamName,
        new BlendEqStateHandler<
            SetBlending::AlphaEq>(&cmd.blend_settings, dirty));
  }

  // Color Write
  {
    bool *dirty = color_write_helper_.dirty_ptr();
    using command_buffer::o3d::SetColorWrite;
    SetColorWrite& cmd = color_write_helper_.command();
    renderer->AddStateHandler(
        State::kDitherEnableParamName,
        new EnableStateHandler<SetColorWrite::DitherEnable>(&cmd.flags, dirty));
    renderer->AddStateHandler(
        State::kColorWriteEnableParamName,
        new ColorWriteStateHandler(&cmd.flags, dirty));
  }
}

void RendererCB::StateManager::ValidateStates(CommandBufferHelper *helper) {
  point_line_helper_.Validate(helper);
  poly_offset_helper_.Validate(helper);
  poly_raster_helper_.Validate(helper);
  alpha_test_helper_.Validate(helper);
  depth_test_helper_.Validate(helper);
  stencil_test_helper_.Validate(helper);
  color_write_helper_.Validate(helper);
  blending_helper_.Validate(helper);
}

}  // namespace o3d
