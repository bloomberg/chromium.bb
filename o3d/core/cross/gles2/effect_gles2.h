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


// This file contains the declaration of the EffectGLES2 class.

#ifndef O3D_CORE_CROSS_GLES2_EFFECT_GLES2_H_
#define O3D_CORE_CROSS_GLES2_EFFECT_GLES2_H_

// Disable compiler warning for openGLES2 calls that require a void* to
// be cast to a GLuint
#if defined(OS_WIN)
#pragma warning(disable : 4312)
#pragma warning(disable : 4311)
#endif

#include <utility>
#include <vector>
#include <map>
#include "core/cross/gles2/gles2_headers.h"
#include "core/cross/effect.h"
#include "core/cross/gles2/utils_gles2.h"

namespace o3d {

class DrawElementGLES2;
class ParamCacheGLES2;
class ParamObject;
class Param;
class ParamTexture;
class RendererGLES2;
class SemanticManager;

// A class to set an effect parameter from an O3D parameter.
class EffectParamHandlerGLES2 : public RefCounted {
 public:
  typedef SmartPointer<EffectParamHandlerGLES2> Ref;
  virtual ~EffectParamHandlerGLES2() { }

  // Sets a GLES2/Cg Effect Parameter by an O3D Param.
  virtual void SetEffectParam(
      RendererGLES2* renderer, CGparameter cg_param) = 0;

  // Resets a GLES2/Cg Effect parameter to default (currently only
  // unbinds textures contained in Sampler params).
  virtual void ResetEffectParam(
      RendererGLES2* renderer, CGparameter cg_param) {}
};

// EffectGLES2 is an implementation of the Effect object for OpenGLES2.  It
// provides the API for setting the vertex and framgent shaders for the
// Effect using the Cg Runtime.  Currently the two shaders can either be
// provided separately as shader code or together in an FX file.
class EffectGLES2 : public Effect {
 public:
  EffectGLES2(ServiceLocator* service_locator, CGcontext cg_context);
  virtual ~EffectGLES2();

  // Reads the vertex and fragment shaders from string in the FX format.
  // It returns true if the shaders were successfully compiled.
  virtual bool LoadFromFXString(const String& effect);

  // Binds the shaders to the device and sets up all the shader parameters using
  // the values from the matching Param's of the param_object.
  void PrepareForDraw(ParamCacheGLES2* param_cache_gl);

  // Removes any pipeline state-changes installed during a draw.
  void PostDraw(ParamCacheGLES2* param_cache_gl);

  // Gets info about the parameters this effect needs.
  // Overriden from Effect.
  virtual void GetParameterInfo(EffectParameterInfoArray* info_array);

  // Gets info about the streams this effect needs.
  // Overriden from Effect.
  virtual void GetStreamInfo(
      EffectStreamInfoArray* info_array);

  // Given a CG_SAMPLER parameter, find the corresponding CG_TEXTURE
  // parameterand from this CG_TEXTURE, find a matching Param by name in a list
  // of ParamObject.
  // TODO(o3d): remove this (OLD path for textures).
  ParamTexture* GetTextureParamFromCgSampler(
      CGparameter cg_sampler,
      const std::vector<ParamObject*> &param_objects);

  CGprogram cg_vertex_program() { return cg_vertex_; }
  CGprogram cg_fragment_program() { return cg_fragment_; }

 private:
  // Loops through all the parameters in the ShapeDataGLES2 and updates the
  // corresponding parameter EffectGLES2 object
  void UpdateShaderUniformsFromEffect(ParamCacheGLES2* param_cache_gl);
  // Undoes the effect of the above.  For now, this unbinds textures.
  void ResetShaderUniforms(ParamCacheGLES2* param_cache_gl);
  void GetShaderParamInfo(CGprogram program,
                          CGenum name_space,
                          std::map<String, EffectParameterInfo>* info_map);
  void GetVaryingVertexShaderParamInfo(
      CGprogram program,
      CGenum name_space,
      std::vector<EffectStreamInfo>* info_array);

  // TODO(o3d): remove these (OLD path for textures).
  void SetTexturesFromEffect(ParamCacheGLES2* param_cache_gl);
  void FillSamplerToTextureMap(const String &effect);
  String GetTextureNameFromSamplerParamName(const String &sampler_name);

  SemanticManager* semantic_manager_;
  RendererGLES2* renderer_;

  CGcontext cg_context_;
  CGprogram cg_vertex_;
  CGprogram cg_fragment_;

  // TODO(o3d): remove this (OLD path for textures).
  std::map<String, String> sampler_to_texture_map_;
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_GLES2_EFFECT_GLES2_H_

