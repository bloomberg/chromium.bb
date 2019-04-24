// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl2_compute_rendering_context_base.h"

#include <algorithm>
#include <memory>

#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/webgl_any.h"
#include "third_party/blink/renderer/modules/webgl/webgl_program.h"
#include "third_party/blink/renderer/modules/webgl/webgl_uniform_location.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

WebGL2ComputeRenderingContextBase::WebGL2ComputeRenderingContextBase(
    CanvasRenderingContextHost* host,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    bool using_gpu_compositing,
    const CanvasContextCreationAttributesCore& requested_attributes)
    : WebGL2RenderingContextBase(host,
                                 std::move(context_provider),
                                 using_gpu_compositing,
                                 requested_attributes,
                                 Platform::kWebGL2ComputeContextType) {}

void WebGL2ComputeRenderingContextBase::DestroyContext() {
  WebGL2RenderingContextBase::DestroyContext();
}

void WebGL2ComputeRenderingContextBase::InitializeNewContext() {
  DCHECK(!isContextLost());

  WebGL2RenderingContextBase::InitializeNewContext();
}

void WebGL2ComputeRenderingContextBase::dispatchCompute(GLuint numGroupsX,
                                                        GLuint numGroupsY,
                                                        GLuint numGroupsZ) {
  ContextGL()->DispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
}

ScriptValue WebGL2ComputeRenderingContextBase::getProgramInterfaceParameter(
    ScriptState* script_state,
    WebGLProgram* program,
    GLenum program_interface,
    GLenum pname) {
  if (!ValidateWebGLProgramOrShader("getProgramInterfaceParameter", program))
    return ScriptValue::CreateNull(script_state);

  switch (pname) {
    case GL_ACTIVE_RESOURCES:
    case GL_MAX_NAME_LENGTH:
    case GL_MAX_NUM_ACTIVE_VARIABLES: {
      GLint value = 0;
      ContextGL()->GetProgramInterfaceiv(
          ObjectOrZero(program), program_interface, pname, &value);
      return WebGLAny(script_state, value);
    }
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getProgramInterfaceParameter",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state);
  }
}

GLuint WebGL2ComputeRenderingContextBase::getProgramResourceIndex(
    WebGLProgram* program,
    GLenum program_interface,
    const String& name) {
  if (!ValidateWebGLProgramOrShader("getProgramResourceIndex", program))
    return 0;

  return ContextGL()->GetProgramResourceIndex(
      ObjectOrZero(program), program_interface, name.Utf8().data());
}

String WebGL2ComputeRenderingContextBase::getProgramResourceName(
    WebGLProgram* program,
    GLenum program_interface,
    GLuint index) {
  if (!ValidateWebGLProgramOrShader("getProgramResourceName", program))
    return String();

  GLint max_name_length = -1;
  ContextGL()->GetProgramInterfaceiv(ObjectOrZero(program), program_interface,
                                     GL_MAX_NAME_LENGTH, &max_name_length);
  if (max_name_length <= 0) {
    SynthesizeGLError(GL_INVALID_VALUE, "getProgramResourceName",
                      "invalid program interface");
    return String();
  }
  auto name = std::make_unique<GLchar[]>(max_name_length);

  GLsizei length = 0;
  ContextGL()->GetProgramResourceName(ObjectOrZero(program), program_interface,
                                      index, max_name_length, &length,
                                      name.get());
  if (length <= 0)
    return String();
  return String(name.get(), static_cast<uint32_t>(length));
}

base::Optional<Vector<ScriptValue>>
WebGL2ComputeRenderingContextBase::getProgramResource(
    ScriptState* script_state,
    WebGLProgram* program,
    GLenum program_interface,
    GLuint index,
    const Vector<GLenum>& props) {
  if (!ValidateWebGLProgramOrShader("getProgramResource", program))
    return base::nullopt;

  Vector<GLenum> auxiliary_props;
  Vector<GLint> auxiliary_params;
  Vector<GLenum> extended_props;
  Vector<GLint> extended_params;
  for (GLenum prop : props) {
    switch (prop) {
      // Handle props with fixed-length return values.
      case GL_ARRAY_SIZE:
      case GL_ARRAY_STRIDE:
      case GL_ATOMIC_COUNTER_BUFFER_INDEX:
      case GL_BLOCK_INDEX:
      case GL_BUFFER_BINDING:
      case GL_BUFFER_DATA_SIZE:
      case GL_IS_ROW_MAJOR:
      case GL_LOCATION:
      case GL_MATRIX_STRIDE:
      case GL_NAME_LENGTH:
      case GL_NUM_ACTIVE_VARIABLES:
      case GL_OFFSET:
      case GL_REFERENCED_BY_COMPUTE_SHADER:
      case GL_REFERENCED_BY_FRAGMENT_SHADER:
      case GL_REFERENCED_BY_VERTEX_SHADER:
      case GL_TOP_LEVEL_ARRAY_SIZE:
      case GL_TOP_LEVEL_ARRAY_STRIDE:
      case GL_TYPE:
        extended_props.push_back(prop);
        extended_params.push_back(0);
        break;

      // Handle props with variable-length return values. For these props, their
      // lengths will be queried first by constructing |auxiliary_props| as the
      // following, and |extended_params| will be adequately sized for the whole
      // result after that.
      case GL_ACTIVE_VARIABLES:
        auxiliary_props.push_back(GL_NUM_ACTIVE_VARIABLES);
        auxiliary_params.push_back(-1);
        extended_props.push_back(GL_ACTIVE_VARIABLES);
        break;

      default:
        SynthesizeGLError(GL_INVALID_ENUM, "getProgramResource",
                          "invalid program resource property");
        return base::nullopt;
    }
  }
  extended_props.PrependVector(auxiliary_props);
  extended_params.PrependVector(auxiliary_params);

  if (auxiliary_props.size()) {
    ContextGL()->GetProgramResourceiv(ObjectOrZero(program), program_interface,
                                      index, auxiliary_props.size(),
                                      auxiliary_props.data(),
                                      auxiliary_params.size(), nullptr,
                                      auxiliary_params.data());
    for (GLint n : auxiliary_params) {
      extended_params.resize(extended_params.size() + std::max(n, 0));
    }
  }

  GLsizei length = 0;
  ContextGL()->GetProgramResourceiv(ObjectOrZero(program), program_interface,
                                    index, extended_props.size(),
                                    extended_props.data(),
                                    extended_params.size(), &length,
                                    extended_params.data());
  if (length <= 0) {
    return base::nullopt;
  }
  for (wtf_size_t i = 0; i < auxiliary_params.size(); ++i) {
    // The returned lengths really should not differ from the previous ones.
    CHECK_EQ(extended_params[i], auxiliary_params[i]);
  }

  // Interpret the returned values and construct the result array. The type of
  // each array element is the natural type for the requested property.
  Vector<ScriptValue> result;
  wtf_size_t auxiliary_param_index = 0;
  wtf_size_t extended_param_index = auxiliary_params.size();
  for (GLenum prop : props) {
    switch (prop) {
      case GL_IS_ROW_MAJOR:
      case GL_REFERENCED_BY_COMPUTE_SHADER:
      case GL_REFERENCED_BY_FRAGMENT_SHADER:
      case GL_REFERENCED_BY_VERTEX_SHADER: {
        bool value = extended_params[extended_param_index];
        result.push_back(WebGLAny(script_state, value));
        ++extended_param_index;
        break;
      }
      case GL_ARRAY_STRIDE:
      case GL_ATOMIC_COUNTER_BUFFER_INDEX:
      case GL_BLOCK_INDEX:
      case GL_MATRIX_STRIDE:
      case GL_OFFSET: {
        int value = extended_params[extended_param_index];
        result.push_back(WebGLAny(script_state, value));
        ++extended_param_index;
        break;
      }
      case GL_LOCATION: {
        int value = extended_params[extended_param_index];
        result.push_back(
            WrapLocation(script_state, value, program, program_interface));
        ++extended_param_index;
        break;
      }
      case GL_ARRAY_SIZE:
      case GL_BUFFER_BINDING:
      case GL_BUFFER_DATA_SIZE:
      case GL_NAME_LENGTH:
      case GL_NUM_ACTIVE_VARIABLES:
      case GL_TOP_LEVEL_ARRAY_SIZE:
      case GL_TOP_LEVEL_ARRAY_STRIDE: {
        unsigned value = extended_params[extended_param_index];
        result.push_back(WebGLAny(script_state, value));
        ++extended_param_index;
        break;
      }
      case GL_TYPE: {
        GLenum value = extended_params[extended_param_index];
        result.push_back(WebGLAny(script_state, value));
        ++extended_param_index;
        break;
      }
      case GL_ACTIVE_VARIABLES: {
        DOMUint32Array* value = DOMUint32Array::Create(
            reinterpret_cast<unsigned*>(&extended_params[extended_param_index]),
            auxiliary_params[auxiliary_param_index]);
        result.push_back(WebGLAny(script_state, value));
        extended_param_index += auxiliary_params[auxiliary_param_index];
        ++auxiliary_param_index;
        break;
      }
      default:
        NOTREACHED();
    }
  }

  return result;
}

ScriptValue WebGL2ComputeRenderingContextBase::getProgramResourceLocation(
    ScriptState* script_state,
    WebGLProgram* program,
    GLenum program_interface,
    const String& name) {
  if (!ValidateWebGLProgramOrShader("getProgramResourceLocation", program))
    return WrapLocation(script_state, -1, program, program_interface);

  GLint location = ContextGL()->GetProgramResourceLocation(
      ObjectOrZero(program), program_interface, name.Utf8().data());
  return WrapLocation(script_state, location, program, program_interface);
}

void WebGL2ComputeRenderingContextBase::bindImageTexture(GLuint unit,
                                                         WebGLTexture* texture,
                                                         GLint level,
                                                         GLboolean layered,
                                                         GLint layer,
                                                         GLenum access,
                                                         GLenum format) {
  ContextGL()->BindImageTexture(unit, ObjectOrZero(texture), level, layered,
                                layer, access, format);
}

void WebGL2ComputeRenderingContextBase::memoryBarrier(GLbitfield barriers) {
  ContextGL()->MemoryBarrierEXT(barriers);
}

void WebGL2ComputeRenderingContextBase::memoryBarrierByRegion(
    GLbitfield barriers) {
  ContextGL()->MemoryBarrierByRegion(barriers);
}

ScriptValue WebGL2ComputeRenderingContextBase::getParameter(
    ScriptState* script_state,
    GLenum pname) {
  if (isContextLost())
    return ScriptValue::CreateNull(script_state);
  switch (pname) {
    case GL_SHADING_LANGUAGE_VERSION: {
      return WebGLAny(
          script_state,
          "WebGL GLSL ES 3.10 (" +
              String(ContextGL()->GetString(GL_SHADING_LANGUAGE_VERSION)) +
              ")");
    }
    case GL_VERSION: {
      return WebGLAny(script_state,
                      "WebGL 2.0 Compute (" +
                          String(ContextGL()->GetString(GL_VERSION)) + ")");
    }
    case GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE:
    case GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS:
    case GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS:
    case GL_MAX_COMBINED_ATOMIC_COUNTERS:
    case GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS:
    case GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS:
    case GL_MAX_COMPUTE_SHARED_MEMORY_SIZE:
    case GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS:
    case GL_MAX_COMPUTE_UNIFORM_COMPONENTS:
    case GL_MAX_COMPUTE_UNIFORM_BLOCKS:
    case GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS:
    case GL_MAX_COMPUTE_IMAGE_UNIFORMS:
    case GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS:
    case GL_MAX_FRAGMENT_ATOMIC_COUNTERS:
    case GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS:
    case GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS:
    case GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS:
    case GL_MAX_VERTEX_ATOMIC_COUNTERS:
    case GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_SHADER_STORAGE_BLOCK_SIZE:
      return GetInt64Parameter(script_state, pname);

    default:
      return WebGL2RenderingContextBase::getParameter(script_state, pname);
  }
}

void WebGL2ComputeRenderingContextBase::Trace(blink::Visitor* visitor) {
  WebGL2RenderingContextBase::Trace(visitor);
}

ScriptValue WebGL2ComputeRenderingContextBase::WrapLocation(
    ScriptState* script_state,
    GLint location,
    WebGLProgram* program,
    GLenum program_interface) {
  switch (program_interface) {
    case GL_PROGRAM_INPUT:
    case GL_PROGRAM_OUTPUT: {
      return WebGLAny(script_state, location);
    }
    case GL_UNIFORM: {
      if (location == -1)
        return ScriptValue::CreateNull(script_state);
      DCHECK_GE(location, 0);
      WebGLUniformLocation* uniform_location =
          WebGLUniformLocation::Create(program, location);
      return ScriptValue(script_state, ToV8(uniform_location, script_state));
    }
    default: {
      return WebGLAny(script_state, location);
    }
  }
}

}  // namespace blink
