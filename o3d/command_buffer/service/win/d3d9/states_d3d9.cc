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


// This file contains the implementation of the state-related GAPID3D9
// functions.

#include <algorithm>
#include "command_buffer/common/cross/o3d_cmd_format.h"
#include "command_buffer/service/win/d3d9/gapi_d3d9.h"

namespace o3d {
namespace command_buffer {
namespace o3d {

namespace {

// Checks that a command_buffer enum matches a D3D enum so that it can be
// converted quickly.
#define CHECK_CB_ENUM_MATCHES_D3D(CB_ENUM, D3D_ENUM)     \
  COMPILE_ASSERT(CB_ENUM + 1 == D3D_ENUM,  \
                 CB_ENUM ## _plus_1_not_ ## D3D_ENUM)

// Converts values from the PolygonMode enum to corresponding D3D values
inline D3DFILLMODE PolygonModeToD3D(PolygonMode fill_mode) {
  DCHECK_LT(fill_mode, kNumPolygonMode);

  // Check that all acceptable values translate to D3D values by adding 1.

  CHECK_CB_ENUM_MATCHES_D3D(kPolygonModePoints, D3DFILL_POINT);
  CHECK_CB_ENUM_MATCHES_D3D(kPolygonModeLines, D3DFILL_WIREFRAME);
  CHECK_CB_ENUM_MATCHES_D3D(kPolygonModeFill, D3DFILL_SOLID);
  return static_cast<D3DFILLMODE>(fill_mode + 1);
}

// Converts values from the FaceCullMode enum to corresponding D3D values
inline D3DCULL FaceCullModeToD3D(FaceCullMode cull_mode) {
  DCHECK_LT(cull_mode, kNumFaceCullMode);

  // Check that all acceptable values translate to D3D values by adding 1.
  CHECK_CB_ENUM_MATCHES_D3D(kCullNone, D3DCULL_NONE);
  CHECK_CB_ENUM_MATCHES_D3D(kCullCW, D3DCULL_CW);
  CHECK_CB_ENUM_MATCHES_D3D(kCullCCW, D3DCULL_CCW);
  return static_cast<D3DCULL>(cull_mode + 1);
}

// Converts values from the Comparison enum to corresponding D3D values
inline D3DCMPFUNC ComparisonToD3D(Comparison comp) {
  DCHECK_LT(comp, kNumComparison);

  // Check that all acceptable values translate to D3D values by adding 1.
  CHECK_CB_ENUM_MATCHES_D3D(kNever, D3DCMP_NEVER);
  CHECK_CB_ENUM_MATCHES_D3D(kLess, D3DCMP_LESS);
  CHECK_CB_ENUM_MATCHES_D3D(kEqual, D3DCMP_EQUAL);
  CHECK_CB_ENUM_MATCHES_D3D(kLEqual, D3DCMP_LESSEQUAL);
  CHECK_CB_ENUM_MATCHES_D3D(kGreater, D3DCMP_GREATER);
  CHECK_CB_ENUM_MATCHES_D3D(kNotEqual, D3DCMP_NOTEQUAL);
  CHECK_CB_ENUM_MATCHES_D3D(kGEqual, D3DCMP_GREATEREQUAL);
  CHECK_CB_ENUM_MATCHES_D3D(kAlways, D3DCMP_ALWAYS);
  return static_cast<D3DCMPFUNC>(comp + 1);
}

// Converts values from the StencilOp enum to corresponding D3D values
inline D3DSTENCILOP StencilOpToD3D(StencilOp stencil_op) {
  DCHECK_LT(stencil_op, kNumStencilOp);

  // Check that all acceptable values translate to D3D values by adding 1.
  CHECK_CB_ENUM_MATCHES_D3D(kKeep, D3DSTENCILOP_KEEP);
  CHECK_CB_ENUM_MATCHES_D3D(kZero, D3DSTENCILOP_ZERO);
  CHECK_CB_ENUM_MATCHES_D3D(kReplace, D3DSTENCILOP_REPLACE);
  CHECK_CB_ENUM_MATCHES_D3D(kIncNoWrap, D3DSTENCILOP_INCRSAT);
  CHECK_CB_ENUM_MATCHES_D3D(kDecNoWrap, D3DSTENCILOP_DECRSAT);
  CHECK_CB_ENUM_MATCHES_D3D(kInvert, D3DSTENCILOP_INVERT);
  CHECK_CB_ENUM_MATCHES_D3D(kIncWrap, D3DSTENCILOP_INCR);
  CHECK_CB_ENUM_MATCHES_D3D(kDecWrap, D3DSTENCILOP_DECR);
  return static_cast<D3DSTENCILOP>(stencil_op + 1);
}

// Converts values from the BlendEq enum to corresponding D3D values
inline D3DBLENDOP BlendEqToD3D(BlendEq blend_eq) {
  DCHECK_LT(blend_eq, kNumBlendEq);
  // Check that all acceptable values translate to D3D values by adding 1.
  CHECK_CB_ENUM_MATCHES_D3D(kBlendEqAdd, D3DBLENDOP_ADD);
  CHECK_CB_ENUM_MATCHES_D3D(kBlendEqSub, D3DBLENDOP_SUBTRACT);
  CHECK_CB_ENUM_MATCHES_D3D(kBlendEqRevSub, D3DBLENDOP_REVSUBTRACT);
  CHECK_CB_ENUM_MATCHES_D3D(kBlendEqMin, D3DBLENDOP_MIN);
  CHECK_CB_ENUM_MATCHES_D3D(kBlendEqMax, D3DBLENDOP_MAX);
  return static_cast<D3DBLENDOP>(blend_eq + 1);
}

// Converts values from the BlendFunc enum to corresponding D3D values
D3DBLEND BlendFuncToD3D(BlendFunc blend_func) {
  // The D3DBLEND enum values don't map 1-to-1 to BlendFunc, so we use a switch
  // here.
  switch (blend_func) {
    case kBlendFuncZero:
      return D3DBLEND_ZERO;
    case kBlendFuncOne:
      return D3DBLEND_ONE;
    case kBlendFuncSrcColor:
      return D3DBLEND_SRCCOLOR;
    case kBlendFuncInvSrcColor:
      return D3DBLEND_INVSRCCOLOR;
    case kBlendFuncSrcAlpha:
      return D3DBLEND_SRCALPHA;
    case kBlendFuncInvSrcAlpha:
      return D3DBLEND_INVSRCALPHA;
    case kBlendFuncDstAlpha:
      return D3DBLEND_DESTALPHA;
    case kBlendFuncInvDstAlpha:
      return D3DBLEND_INVDESTALPHA;
    case kBlendFuncDstColor:
      return D3DBLEND_DESTCOLOR;
    case kBlendFuncInvDstColor:
      return D3DBLEND_INVDESTCOLOR;
    case kBlendFuncSrcAlphaSaturate:
      return D3DBLEND_SRCALPHASAT;
    case kBlendFuncBlendColor:
      return D3DBLEND_BLENDFACTOR;
    case kBlendFuncInvBlendColor:
      return D3DBLEND_INVBLENDFACTOR;
    default:
      DLOG(FATAL) << "Invalid BlendFunc";
      return D3DBLEND_ZERO;
  }
}

// Decodes stencil test function and operations from the bitfield.
void DecodeStencilFuncOps(Uint32 params,
                          Comparison *func,
                          StencilOp *pass,
                          StencilOp *fail,
                          StencilOp *zfail) {
  // Sanity check. The value has already been tested in
  // GAPIDecoder::DecodeSetStencilTest in gapi_decoder.cc.
  DCHECK_EQ(SetStencilTest::Unused1::Get(params), 0);
  // Check that the bitmask get cannot generate values outside of the allowed
  // range.
  COMPILE_ASSERT(SetStencilTest::CWFunc::kMask <
                 kNumComparison,
                 set_stencil_test_CWFunc_may_produce_invalid_values);
  *func = static_cast<Comparison>(SetStencilTest::CWFunc::Get(params));

  COMPILE_ASSERT(SetStencilTest::CWPassOp::kMask <
                 kNumStencilOp,
                 set_stencil_test_CWPassOp_may_produce_invalid_values);
  *pass = static_cast<StencilOp>(SetStencilTest::CWPassOp::Get(params));

  COMPILE_ASSERT(SetStencilTest::CWFailOp::kMask <
                 kNumStencilOp,
                 set_stencil_test_CWFailOp_may_produce_invalid_values);
  *fail = static_cast<StencilOp>(SetStencilTest::CWFailOp::Get(params));

  COMPILE_ASSERT(SetStencilTest::CWZFailOp::kMask <
                 kNumStencilOp,
                 set_stencil_test_CWZFailOp_may_produce_invalid_values);
  *zfail = static_cast<StencilOp>(SetStencilTest::CWZFailOp::Get(params));
}

}  // anonymous namespace

void GAPID3D9::SetScissor(bool enable,
                          unsigned int x,
                          unsigned int y,
                          unsigned int width,
                          unsigned int height) {
  HR(d3d_device_->SetRenderState(D3DRS_SCISSORTESTENABLE,
                                 enable ? TRUE : FALSE));
  RECT rect = {x, y, x + width, y + height};
  HR(d3d_device_->SetScissorRect(&rect));
}

void GAPID3D9::SetPolygonOffset(float slope_factor, float units) {
  HR(d3d_device_->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,
                                 FloatAsDWORD(slope_factor)));
  // TODO: this value is hard-coded currently because we only create a
  // 24-bit depth buffer. Move this to a member of GAPID3D9 if this changes.
  const float kUnitScale = 1.f / (1 << 24);
  HR(d3d_device_->SetRenderState(D3DRS_DEPTHBIAS,
                                 FloatAsDWORD(units * kUnitScale)));
}

void GAPID3D9::SetPointLineRaster(bool line_smooth,
                                  bool point_sprite,
                                  float point_size) {
  HR(d3d_device_->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE,
                                 line_smooth ? TRUE : FALSE));
  HR(d3d_device_->SetRenderState(D3DRS_POINTSPRITEENABLE,
                                 point_sprite ? TRUE : FALSE));
  HR(d3d_device_->SetRenderState(D3DRS_POINTSIZE,
                                 FloatAsDWORD(point_size)));
}

void GAPID3D9::SetPolygonRaster(PolygonMode fill_mode,
                                FaceCullMode cull_mode) {
  HR(d3d_device_->SetRenderState(D3DRS_FILLMODE, PolygonModeToD3D(fill_mode)));
  HR(d3d_device_->SetRenderState(D3DRS_CULLMODE, FaceCullModeToD3D(cull_mode)));
}

void GAPID3D9::SetAlphaTest(bool enable,
                            float reference,
                            Comparison comp) {
  HR(d3d_device_->SetRenderState(D3DRS_ALPHABLENDENABLE,
                                 enable ? TRUE : FALSE));
  HR(d3d_device_->SetRenderState(D3DRS_ALPHAREF,
                                 FloatToClampedByte(reference)));
  HR(d3d_device_->SetRenderState(D3DRS_ALPHAFUNC, ComparisonToD3D(comp)));
}

void GAPID3D9::SetDepthTest(bool enable,
                            bool write_enable,
                            Comparison comp) {
  HR(d3d_device_->SetRenderState(D3DRS_ZENABLE,
                                 enable ? TRUE : FALSE));
  HR(d3d_device_->SetRenderState(D3DRS_ZWRITEENABLE,
                                 write_enable ? TRUE : FALSE));
  HR(d3d_device_->SetRenderState(D3DRS_ZFUNC, ComparisonToD3D(comp)));
}

void GAPID3D9::SetStencilTest(bool enable,
                              bool separate_ccw,
                              unsigned int write_mask,
                              unsigned int compare_mask,
                              unsigned int ref,
                              Uint32 func_ops) {
  HR(d3d_device_->SetRenderState(D3DRS_STENCILENABLE,
                                 enable ? TRUE : FALSE));
  HR(d3d_device_->SetRenderState(D3DRS_STENCILWRITEMASK, write_mask));
  HR(d3d_device_->SetRenderState(D3DRS_STENCILMASK, compare_mask));
  HR(d3d_device_->SetRenderState(D3DRS_STENCILREF, ref));

  Comparison func;
  StencilOp pass;
  StencilOp fail;
  StencilOp zfail;
  DecodeStencilFuncOps(func_ops, &func, &pass, &fail, &zfail);
  HR(d3d_device_->SetRenderState(D3DRS_STENCILFUNC,
                                 ComparisonToD3D(func)));
  HR(d3d_device_->SetRenderState(D3DRS_STENCILPASS,
                                 StencilOpToD3D(pass)));
  HR(d3d_device_->SetRenderState(D3DRS_STENCILFAIL,
                                 StencilOpToD3D(fail)));
  HR(d3d_device_->SetRenderState(D3DRS_STENCILZFAIL,
                                 StencilOpToD3D(zfail)));

  if (separate_ccw) {
    HR(d3d_device_->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, TRUE));
    // Check that the definition of the counter-clockwise func/ops match the
    // clockwise ones, just shifted by 16 bits, so that we can use
    // DecodeStencilFuncOps on both of them.
#define CHECK_CCW_MATCHES_CW(FIELD)                                         \
    COMPILE_ASSERT(SetStencilTest::CW ## FIELD::kLength ==                  \
                   SetStencilTest::CCW ## FIELD::kLength,                   \
                   CCW ## FIELD ## _length_does_not_match_ ## CW ## FIELD); \
    COMPILE_ASSERT(SetStencilTest::CW ## FIELD::kShift + 16 ==              \
                   SetStencilTest::CCW ## FIELD::kShift,                    \
                   CCW ## FIELD ## _shift_does_not_match_ ## CW ## FIELD)
    CHECK_CCW_MATCHES_CW(Func);
    CHECK_CCW_MATCHES_CW(PassOp);
    CHECK_CCW_MATCHES_CW(FailOp);
    CHECK_CCW_MATCHES_CW(ZFailOp);
#undef CHECK_CCW_MATCHES_CW
    // Extract upper 16 bits.
    Uint32 ccw_func_ops = BitField<16, 16>::Get(func_ops);

    DecodeStencilFuncOps(ccw_func_ops, &func, &pass, &fail, &zfail);
    HR(d3d_device_->SetRenderState(D3DRS_CCW_STENCILFUNC,
                                   ComparisonToD3D(func)));
    HR(d3d_device_->SetRenderState(D3DRS_CCW_STENCILPASS,
                                   StencilOpToD3D(pass)));
    HR(d3d_device_->SetRenderState(D3DRS_CCW_STENCILFAIL,
                                   StencilOpToD3D(fail)));
    HR(d3d_device_->SetRenderState(D3DRS_CCW_STENCILZFAIL,
                                   StencilOpToD3D(zfail)));
  } else {
    HR(d3d_device_->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE));
  }
}

void GAPID3D9::SetColorWrite(bool red,
                             bool green,
                             bool blue,
                             bool alpha,
                             bool dither) {
  Uint32 mask = red ? D3DCOLORWRITEENABLE_RED : 0;
  mask |= green ? D3DCOLORWRITEENABLE_GREEN : 0;
  mask |= blue ? D3DCOLORWRITEENABLE_BLUE : 0;
  mask |= alpha ? D3DCOLORWRITEENABLE_ALPHA : 0;
  HR(d3d_device_->SetRenderState(D3DRS_COLORWRITEENABLE, mask));
  HR(d3d_device_->SetRenderState(D3DRS_DITHERENABLE, dither ? TRUE : FALSE));
}

void GAPID3D9::SetBlending(bool enable,
                           bool separate_alpha,
                           BlendEq color_eq,
                           BlendFunc color_src_func,
                           BlendFunc color_dst_func,
                           BlendEq alpha_eq,
                           BlendFunc alpha_src_func,
                           BlendFunc alpha_dst_func) {
  HR(d3d_device_->SetRenderState(D3DRS_ALPHABLENDENABLE,
                                 enable ? TRUE : FALSE));
  HR(d3d_device_->SetRenderState(D3DRS_BLENDOP, BlendEqToD3D(color_eq)));
  HR(d3d_device_->SetRenderState(D3DRS_SRCBLEND,
                                 BlendFuncToD3D(color_src_func)));
  HR(d3d_device_->SetRenderState(D3DRS_DESTBLEND,
                                 BlendFuncToD3D(color_dst_func)));
  if (separate_alpha) {
    HR(d3d_device_->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE));
    HR(d3d_device_->SetRenderState(D3DRS_BLENDOP, BlendEqToD3D(alpha_eq)));
    HR(d3d_device_->SetRenderState(D3DRS_SRCBLEND,
                                   BlendFuncToD3D(alpha_src_func)));
    HR(d3d_device_->SetRenderState(D3DRS_DESTBLEND,
                                   BlendFuncToD3D(alpha_dst_func)));
  } else {
    HR(d3d_device_->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE));
  }
}

void GAPID3D9::SetBlendingColor(const RGBA &color) {
  HR(d3d_device_->SetRenderState(D3DRS_BLENDFACTOR, RGBAToD3DCOLOR(color)));
}

}  // namespace o3d
}  // namespace command_buffer
}  // namespace o3d
