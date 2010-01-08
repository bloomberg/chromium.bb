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


// This file contains the definition of the ParamCacheGLES2 class.

#include "core/cross/error.h"
#include "core/cross/param_array.h"
#include "core/cross/renderer.h"
#include "core/cross/semantic_manager.h"
#include "core/cross/gles2/param_cache_gles2.h"
#include "core/cross/gles2/effect_gles2.h"
#include "core/cross/gles2/sampler_gles2.h"
#include "core/cross/gles2/renderer_gles2.h"
#include "core/cross/element.h"
#include "core/cross/draw_element.h"

namespace o3d {

namespace {

String GetUniformSemantic(GLES2Parameter gl_param, const String& name) {
  return name;
}

}  // anonymous namespace

typedef std::vector<ParamObject *> ParamObjectList;

ParamCacheGLES2::ParamCacheGLES2(SemanticManager* semantic_manager,
                                 Renderer* renderer)
    : semantic_manager_(semantic_manager),
      renderer_(renderer),
      last_compile_count_(0) {
}

bool ParamCacheGLES2::ValidateEffect(Effect* effect) {
  DLOG_ASSERT(effect);

  EffectGLES2* effect_gles2 = down_cast<EffectGLES2*>(effect);
  return (effect_gles2->compile_count() == last_compile_count_);
}

void ParamCacheGLES2::UpdateCache(Effect* effect,
                                  DrawElement* draw_element,
                                  Element* element,
                                  Material* material,
                                  ParamObject* override) {
  DLOG_ASSERT(effect);
  EffectGLES2* effect_gl = down_cast<EffectGLES2*>(effect);

  ScanGLEffectParameters(effect_gl->gl_program(),
                         draw_element,
                         element,
                         material,
                         override);

  last_compile_count_ = effect_gl->compile_count();
}

template <typename T>
class TypedEffectParamHandlerGLES2 : public EffectParamHandlerGLES2 {
 public:
  explicit TypedEffectParamHandlerGLES2(T* param)
      : param_(param) {
  }
  virtual void SetEffectParam(RendererGLES2* renderer, GLES2Parameter gl_param);
 private:
  T* param_;
};

class EffectParamHandlerGLMatrixRows : public EffectParamHandlerGLES2 {
 public:
  explicit EffectParamHandlerGLMatrixRows(ParamMatrix4* param)
      : param_(param) {
  }
  virtual void SetEffectParam(RendererGLES2* renderer,
                              GLES2Parameter gl_param) {
    // set the data as floats in row major order.
    Matrix4 mat = param_->value();
    glUniformMatrix4fv(gl_param, 1, GL_FALSE, &mat[0][0]);
  }
 private:
  ParamMatrix4* param_;
};

class EffectParamHandlerGLMatrixColumns : public EffectParamHandlerGLES2 {
 public:
  explicit EffectParamHandlerGLMatrixColumns(ParamMatrix4* param)
      : param_(param) {
  }
  virtual void SetEffectParam(
      RendererGLES2* renderer, GLES2Parameter gl_param) {
    // set the data as floats in column major order.
    Matrix4 mat = transpose(param_->value());
    glUniformMatrix4fv(gl_param, 1, GL_FALSE, &mat[0][0]);
  }
 private:
  ParamMatrix4* param_;
};

template <>
void TypedEffectParamHandlerGLES2<ParamFloat>::SetEffectParam(
    RendererGLES2* renderer,
    GLES2Parameter gl_param) {
  Float f = param_->value();
  glUniform1f(gl_param, f);
};

template <>
void TypedEffectParamHandlerGLES2<ParamFloat2>::SetEffectParam(
    RendererGLES2* renderer,
    GLES2Parameter gl_param) {
  Float2 f = param_->value();
  glUniform2fv(gl_param, 1, f.GetFloatArray());
};

template <>
void TypedEffectParamHandlerGLES2<ParamFloat3>::SetEffectParam(
    RendererGLES2* renderer,
    GLES2Parameter gl_param) {
  Float3 f = param_->value();
  glUniform3fv(gl_param, 1, f.GetFloatArray());
};

template <>
void TypedEffectParamHandlerGLES2<ParamFloat4>::SetEffectParam(
    RendererGLES2* renderer,
    GLES2Parameter gl_param) {
  Float4 f = param_->value();
  glUniform4fv(gl_param, 1, f.GetFloatArray());
};

template <>
void TypedEffectParamHandlerGLES2<ParamInteger>::SetEffectParam(
    RendererGLES2* renderer,
    GLES2Parameter gl_param) {
  int i = param_->value();
  glUniform1i(gl_param, i);
};

template <>
void TypedEffectParamHandlerGLES2<ParamBoolean>::SetEffectParam(
    RendererGLES2* renderer,
    GLES2Parameter gl_param) {
  int i = param_->value();
  glUniform1i(gl_param, i);
};

class EffectParamHandlerForSamplersGLES2 : public EffectParamHandlerGLES2 {
 public:
  explicit EffectParamHandlerForSamplersGLES2(ParamSampler* param)
      : param_(param) {
  }
  virtual void SetEffectParam(
      RendererGLES2* renderer, GLES2Parameter gl_param) {
    SamplerGLES2* sampler_gl = down_cast<SamplerGLES2*>(param_->value());
    if (!sampler_gl) {
      // Use the error sampler.
      sampler_gl = down_cast<SamplerGLES2*>(renderer->error_sampler());
      // If no error texture is set then generate an error.
      if (!renderer->error_texture()) {
        O3D_ERROR(param_->service_locator())
            << "Missing Sampler for ParamSampler " << param_->name();
      }
    }
    GLint handle = sampler_gl->SetTextureAndStates(gl_param);
    glUniform1iv(gl_param, 1, &handle);
  }
  virtual void ResetEffectParam(
      RendererGLES2* renderer, GLES2Parameter gl_param) {
    SamplerGLES2* sampler_gl = down_cast<SamplerGLES2*>(param_->value());
    if (!sampler_gl) {
      sampler_gl = down_cast<SamplerGLES2*>(renderer->error_sampler());
    }
    sampler_gl->ResetTexture(gl_param);
  }
 private:
  ParamSampler* param_;
};

template <typename T>
class EffectParamArrayHandlerGLES2 : public EffectParamHandlerGLES2 {
 public:
  typedef typename T::DataType DataType;
  EffectParamArrayHandlerGLES2(ParamParamArray* param, GLsizei size)
      : param_(param),
        size_(size) {
    values_.reset(new DataType[size]);
  }
  virtual void SetEffectParam(
      RendererGLES2* renderer, GLES2Parameter gl_param) {
    ParamArray* param = param_->value();
    if (param) {
      if (size_ != static_cast<int>(param->size())) {
        O3D_ERROR(param->service_locator())
            << "number of params in ParamArray does not match number of params "
            << "needed by shader array";
      } else {
        for (int i = 0; i < size_; ++i) {
          Param* untyped_element = param->GetUntypedParam(i);
          // TODO(gman): Make this check happen when building the param cache.
          //    To do that would require that ParamParamArray mark it's owner
          //    as changed if a Param in it's ParamArray changes.
          if (untyped_element->IsA(T::GetApparentClass())) {
            SetElement(down_cast<T*>(untyped_element), &values_[i]);
          } else {
            O3D_ERROR(param->service_locator())
                << "Param in ParamArray at index " << i << " is not a "
                << T::GetApparentClassName();
          }
        }
        SetElements(gl_param, size_, values_.get());
      }
    }
  }
  void SetElement(T* param, DataType* value);
  void SetElements(GLES2Parameter gl_param, GLsizei size,
                   const DataType* values);

 private:
  ParamParamArray* param_;
  scoped_array<DataType> values_;
  GLsizei size_;
};

class EffectParamArraySamplerHandlerGLES2 : public EffectParamHandlerGLES2 {
 public:
  explicit EffectParamArraySamplerHandlerGLES2(ParamParamArray* param,
                                               GLsizei size)
      : param_(param),
        size_(size) {
    values_.reset(new GLint[size]);
  }
  virtual void SetEffectParam(RendererGLES2* renderer,
                              GLES2Parameter gl_param) {
    ParamArray* param = param_->value();
    if (param) {
      if (size_ != static_cast<int>(param->size())) {
        O3D_ERROR(param->service_locator())
            << "number of params in ParamArray does not match number of params "
            << "needed by shader array";
      } else {
        for (int i = 0; i < size_; ++i) {
          GLint handle = 0;
          Param* untyped_element = param->GetUntypedParam(i);
          // TODO(gman): Make this check happen when building the param cache.
          //    To do that would require that ParamParamArray mark it's owner
          //    as changed if a Param in it's ParamArray changes.
          if (untyped_element->IsA(ParamSampler::GetApparentClass())) {
            ParamSampler* element = down_cast<ParamSampler*>(untyped_element);
            SamplerGLES2* sampler_gl =
                down_cast<SamplerGLES2*>(element->value());
            if (!sampler_gl) {
              // Use the error sampler.
              sampler_gl = down_cast<SamplerGLES2*>(renderer->error_sampler());
              // If no error texture is set then generate an error.
              if (!renderer->error_texture()) {
                O3D_ERROR(param_->service_locator())
                    << "Missing Sampler for ParamSampler '" << param_->name()
                    << "' index " << i;
              }
            }
            handle = sampler_gl->SetTextureAndStates(gl_param);
          } else {
            O3D_ERROR(param->service_locator())
                << "Param in ParamArray at index " << i
                << " is not a ParamSampler";
          }
          values_[i] = handle;
        }
        glUniform1iv(gl_param, size_, values_.get());
      }
    }
  }
  virtual void ResetEffectParam(RendererGLES2* renderer,
                                GLES2Parameter gl_param) {
    ParamArray* param = param_->value();
    if (param) {
      if (size_ == static_cast<int>(param->size())) {
        for (int i = 0; i < size_; ++i) {
          Param* untyped_element = param->GetUntypedParam(i);
          if (untyped_element->IsA(ParamSampler::GetApparentClass())) {
            ParamSampler* element = down_cast<ParamSampler*>(untyped_element);
            SamplerGLES2* sampler_gl =
                down_cast<SamplerGLES2*>(element->value());
            if (!sampler_gl) {
              sampler_gl = down_cast<SamplerGLES2*>(renderer->error_sampler());
            }
            sampler_gl->ResetTexture(gl_param);
          }
        }
      }
    }
  }

 private:
  ParamParamArray* param_;
  scoped_array<GLint> values_;
  GLsizei size_;
};

template<>
void EffectParamArrayHandlerGLES2<ParamFloat>::SetElement(
    ParamFloat* param,
    float* value) {
  *value = param->value();
}

template<>
void EffectParamArrayHandlerGLES2<ParamFloat>::SetElements(
    GLES2Parameter gl_param,
    GLsizei count,
    const float* values) {
  glUniform1fv(gl_param, count, values);
}

template<>
void EffectParamArrayHandlerGLES2<ParamFloat2>::SetElement(
    ParamFloat2* param,
    Float2* value) {
  *value = param->value();
}

template<>
void EffectParamArrayHandlerGLES2<ParamFloat2>::SetElements(
    GLES2Parameter gl_param,
    GLsizei count,
    const Float2* values) {
  glUniform2fv(gl_param, count, values[0].GetFloatArray());
}

template<>
void EffectParamArrayHandlerGLES2<ParamFloat3>::SetElement(
    ParamFloat3* param,
    Float3* value) {
  *value = param->value();
}

template<>
void EffectParamArrayHandlerGLES2<ParamFloat3>::SetElements(
    GLES2Parameter gl_param,
    GLsizei count,
    const Float3* values) {
  glUniform3fv(gl_param, count, values[0].GetFloatArray());
}

template<>
void EffectParamArrayHandlerGLES2<ParamFloat4>::SetElement(
    ParamFloat4* param,
    Float4* value) {
  *value = param->value();
}

template<>
void EffectParamArrayHandlerGLES2<ParamFloat4>::SetElements(
    GLES2Parameter gl_param,
    GLsizei count,
    const Float4* values) {
  glUniform4fv(gl_param, count, values[0].GetFloatArray());
}

class ColumnMajorParamMatrix4 : public ParamMatrix4 {
};

class RowMajorParamMatrix4 : public ParamMatrix4 {
};

template<>
void EffectParamArrayHandlerGLES2<RowMajorParamMatrix4>::SetElement(
    RowMajorParamMatrix4* param,
    Matrix4* value) {
  // set the data as floats in row major order.
  *value = param->value();
}

template<>
void EffectParamArrayHandlerGLES2<RowMajorParamMatrix4>::SetElements(
    GLES2Parameter gl_param,
    GLsizei count,
    const Matrix4* values) {
  glUniformMatrix4fv(gl_param, count, GL_FALSE,
                     reinterpret_cast<const GLfloat*>(values_.get()));
}

template<>
void EffectParamArrayHandlerGLES2<ColumnMajorParamMatrix4>::SetElement(
    ColumnMajorParamMatrix4* param,
    Matrix4* value) {
  // set the data as floats in row major order.
  *value = transpose(param->value());
}

template<>
void EffectParamArrayHandlerGLES2<ColumnMajorParamMatrix4>::SetElements(
    GLES2Parameter gl_param,
    GLsizei count,
    const Matrix4* values) {
  // NOTE: The transpose happens in the function above because GLES2 requires
  //    the transpose argument to this function to be GL_FALSE.
  glUniformMatrix4fv(gl_param, count, GL_FALSE,
                     reinterpret_cast<const GLfloat*>(values_.get()));
}

template<>
void EffectParamArrayHandlerGLES2<ParamBoolean>::SetElement(
    ParamBoolean* param,
    bool* value) {
  *value = param->value();
}

template<>
void EffectParamArrayHandlerGLES2<ParamBoolean>::SetElements(
    GLES2Parameter gl_param,
    GLsizei count,
    const bool* values) {
  scoped_array<GLint> local(new GLint[count]);
  for (GLsizei ii = 0; ii < count; ++ii) {
    local[ii] = values[ii];
  }
  glUniform1iv(gl_param, count, local.get());
}

template<>
void EffectParamArrayHandlerGLES2<ParamInteger>::SetElement(
    ParamInteger* param,
    int* value) {
  *value = param->value();
}

template<>
void EffectParamArrayHandlerGLES2<ParamInteger>::SetElements(
    GLES2Parameter gl_param,
    GLsizei count,
    const int* values) {
  glUniform1iv(gl_param, count, values);
}

static EffectParamHandlerGLES2::Ref GetHandlerFromParamAndGLType(
    EffectGLES2* effect_gl,
    Param *param,
    GLenum gl_type,
    GLsizei size) {
  EffectParamHandlerGLES2::Ref handler;
  if (param->IsA(ParamParamArray::GetApparentClass())) {
    ParamParamArray* param_param_array = down_cast<ParamParamArray*>(param);
    switch (gl_type) {
      case GL_FLOAT:
        handler = EffectParamHandlerGLES2::Ref(
            new EffectParamArrayHandlerGLES2<ParamFloat>(param_param_array,
                                                         size));
        break;
      case GL_FLOAT_VEC2:
        handler = EffectParamHandlerGLES2::Ref(
            new EffectParamArrayHandlerGLES2<ParamFloat2>(param_param_array,
                                                          size));
        break;
      case GL_FLOAT_VEC3:
        handler = EffectParamHandlerGLES2::Ref(
            new EffectParamArrayHandlerGLES2<ParamFloat3>(param_param_array,
                                                          size));
        break;
      case GL_FLOAT_VEC4:
        handler = EffectParamHandlerGLES2::Ref(
            new EffectParamArrayHandlerGLES2<ParamFloat4>(param_param_array,
                                                          size));
        break;
      case GL_FLOAT_MAT4:
        if (effect_gl->matrix_load_order() == Effect::COLUMN_MAJOR) {
          handler = EffectParamHandlerGLES2::Ref(
              new EffectParamArrayHandlerGLES2<ColumnMajorParamMatrix4>(
                  param_param_array, size));
        } else {
          handler = EffectParamHandlerGLES2::Ref(
              new EffectParamArrayHandlerGLES2<RowMajorParamMatrix4>(
                  param_param_array, size));
        }
        break;
      case GL_INT:
        handler = EffectParamHandlerGLES2::Ref(
            new EffectParamArrayHandlerGLES2<ParamInteger>(param_param_array,
                                                           size));
        break;
      case GL_BOOL:
        handler = EffectParamHandlerGLES2::Ref(
            new EffectParamArrayHandlerGLES2<ParamBoolean>(param_param_array,
                                                           size));
        break;
      case GL_SAMPLER_2D:
      case GL_SAMPLER_CUBE:
        handler = EffectParamHandlerGLES2::Ref(
            new EffectParamArraySamplerHandlerGLES2(param_param_array, size));
        break;
      default:
        break;
    }
  } else if (param->IsA(ParamMatrix4::GetApparentClass())) {
    if (gl_type == GL_FLOAT_MAT4) {
      if (effect_gl->matrix_load_order() == Effect::COLUMN_MAJOR) {
        // set the data as floats in column major order.
        handler = EffectParamHandlerGLES2::Ref(
            new EffectParamHandlerGLMatrixColumns(
                down_cast<ParamMatrix4*>(param)));
      } else {
        // set the data as floats in row major order.
        handler = EffectParamHandlerGLES2::Ref(
            new EffectParamHandlerGLMatrixRows(
                down_cast<ParamMatrix4*>(param)));
      }
    }
  } else if (param->IsA(ParamFloat::GetApparentClass())) {
    if (gl_type == GL_FLOAT) {
      handler = EffectParamHandlerGLES2::Ref(
          new TypedEffectParamHandlerGLES2<ParamFloat>(
              down_cast<ParamFloat*>(param)));
    }
  } else if (param->IsA(ParamFloat2::GetApparentClass())) {
    if (gl_type == GL_FLOAT_VEC2) {
      handler = EffectParamHandlerGLES2::Ref(
          new TypedEffectParamHandlerGLES2<ParamFloat2>(
              down_cast<ParamFloat2*>(param)));
    }
  } else if (param->IsA(ParamFloat3::GetApparentClass())) {
    if (gl_type == GL_FLOAT_VEC3) {
      handler = EffectParamHandlerGLES2::Ref(
          new TypedEffectParamHandlerGLES2<ParamFloat3>(
              down_cast<ParamFloat3*>(param)));
    }
  } else if (param->IsA(ParamFloat4::GetApparentClass())) {
    if (gl_type == GL_FLOAT_VEC4) {
      handler = EffectParamHandlerGLES2::Ref(
          new TypedEffectParamHandlerGLES2<ParamFloat4>(
              down_cast<ParamFloat4*>(param)));
    }
  }  else if (param->IsA(ParamInteger::GetApparentClass())) {
    if (gl_type == GL_INT) {
      handler = EffectParamHandlerGLES2::Ref(
          new TypedEffectParamHandlerGLES2<ParamInteger>(
              down_cast<ParamInteger*>(param)));
    }
  } else if (param->IsA(ParamBoolean::GetApparentClass())) {
    if (gl_type == GL_BOOL) {
      handler = EffectParamHandlerGLES2::Ref(
          new TypedEffectParamHandlerGLES2<ParamBoolean>(
              down_cast<ParamBoolean*>(param)));
    }
  } else if (param->IsA(ParamSampler::GetApparentClass())) {
    if (gl_type == GL_SAMPLER_2D ||
        gl_type == GL_SAMPLER_CUBE) {
      handler = EffectParamHandlerGLES2::Ref(
          new EffectParamHandlerForSamplersGLES2(
              down_cast<ParamSampler*>(param)));
    }
  }
  return handler;
}

// Local helper function for scanning varying Cg parameters of a
// program or effect and recording their entries into the varying map.
static void ScanVaryingParameters(GLuint gl_program,
                                  ParamCacheGLES2* param_cache_gl) {
  GLint num_attribs = 0;
  GLint max_len = 0;
  glGetProgramiv(gl_program, GL_ACTIVE_ATTRIBUTES, &num_attribs);
  glGetProgramiv(gl_program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_len);
  // TODO(gman): Should we check for error?
  scoped_array<char> name_buffer(new char[max_len + 1]);
  for (GLint ii = 0; ii < num_attribs; ++ii) {
    GLsizei length;
    GLsizei size;
    GLenum type;
    glGetActiveAttrib(
        gl_program, ii, max_len + 1, &length, &size, &type, name_buffer.get());
    // TODO(gman): Should we check for error?
    GLint location = glGetAttribLocation(gl_program, name_buffer.get());
    param_cache_gl->varying_map().insert(std::make_pair(ii, location));
  }
}

// Local helper function for scanning uniform Cg parameters of a
// program or effect and recording their entries into the parameter maps.
static void ScanUniformParameters(SemanticManager* semantic_manager,
                                  Renderer* renderer,
                                  GLuint gl_program,
                                  ParamCacheGLES2* param_cache_gl,
                                  const ParamObjectList& param_objects,
                                  EffectGLES2* effect_gl) {
  GLint num_uniforms = 0;
  GLint max_len = 0;
  glGetProgramiv(gl_program, GL_ACTIVE_UNIFORMS, &num_uniforms);
  glGetProgramiv(gl_program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_len);
  // TODO(gman): Should we check for error?
  scoped_array<char> name_buffer(new char[max_len + 1]);
  for (GLint ii = 0; ii < num_uniforms; ++ii) {
    GLsizei length;
    GLsizei size;
    GLenum gl_type;
    glGetActiveUniform(
        gl_program, ii,
        max_len + 1, &length, &size, &gl_type, name_buffer.get());
    // TODO(gman): Should we check for error?
    GLint location = glGetUniformLocation(gl_program, name_buffer.get());
    String name(name_buffer.get(), length);

    // Find a Param of the same name, and record the link.
    if (param_cache_gl->uniform_map().find(ii) ==
        param_cache_gl->uniform_map().end()) {
      const ObjectBase::Class *sem_class = NULL;
      // Try looking by SAS class name.
      String semantic = GetUniformSemantic(ii, name);
      if (!semantic.empty()) {
        sem_class = semantic_manager->LookupSemantic(semantic);
      }
      EffectParamHandlerGLES2::Ref handler;
      // Look through all the param objects to find a matching param.
      unsigned last = param_objects.size() - 1;
      for (unsigned int i = 0; i < param_objects.size(); ++i) {
        ParamObject *param_object = param_objects[i];
        Param *param = param_object->GetUntypedParam(name);
        if (!param && sem_class) {
          param = param_object->GetUntypedParam(sem_class->name());
        }
        if (!param) {
          // If this is the last param object and we didn't find a matching
          // param then if it's a sampler use the error sampler
          if (i == last) {
            if (gl_type == GL_SAMPLER_2D || gl_type == GL_SAMPLER_CUBE) {
              param = renderer->error_param_sampler();
            }
          }
          if (!param) {
            continue;
          }
        }
        handler = GetHandlerFromParamAndGLType(effect_gl, param, gl_type, size);
        if (!handler.IsNull()) {
          param_cache_gl->uniform_map().insert(
              std::make_pair(location, handler));
          DLOG(INFO) << "ElementGLES2 Matched gl_paramETER \""
                     << name << "\" to Param \""
                     << param->name() << "\" from \""
                     << param_object->name() << "\"";
          break;
        } else {
          // We found a param, but it didn't match the type. keep looking.
          DLOG(ERROR) << "ElementGLES2 Param \""
                      << param->name() << "\" type \""
                      << param->GetClassName() << "\" from \""
                      << param_object->name()
                      << "\" does not match gl_paramETER \""
                      << name << "\"";
        }
      }
      if (handler.IsNull()) {
        DLOG(ERROR) << "No matching Param for gl_paramETER \""
                    << name << "\"";
      }
    }
  }
}

static void DoScanGLEffectParameters(SemanticManager* semantic_manager,
                                     Renderer* renderer,
                                     ParamCacheGLES2* param_cache_gl,
                                     GLuint gl_program,
                                     EffectGLES2* effect_gl,
                                     const ParamObjectList& param_objects) {
  ScanVaryingParameters(gl_program, param_cache_gl);
  ScanUniformParameters(semantic_manager,
                        renderer,
                        gl_program,
                        param_cache_gl,
                        param_objects,
                        effect_gl);
}

// Search the leaf parameters of a CGeffect and it's shaders for parameters
// using cgGetFirstEffectParameter() / cgGetFirstLeafParameter() /
// cgGetNextLeafParameter(). Add the GLES2Parameters found to the parameter
// maps on the DrawElement.
void ParamCacheGLES2::ScanGLEffectParameters(GLuint gl_program,
                                             ParamObject* draw_element,
                                             ParamObject* element,
                                             Material* material,
                                             ParamObject* override) {
  DLOG(INFO) << "DrawElementGLES2 ScanGLEffectParameters";
  DLOG_ASSERT(material);
  DLOG_ASSERT(draw_element);
  DLOG_ASSERT(element);
  EffectGLES2* effect_gl = static_cast<EffectGLES2*>(material->effect());
  DLOG_ASSERT(effect_gl);
  if (gl_program == 0)  {
    DLOG(ERROR) << "Can't scan an empty Program for Parameters.";
    return;
  }
  uniform_map_.clear();
  varying_map_.clear();
  ParamObjectList param_object_list;
  param_object_list.push_back(override);
  param_object_list.push_back(draw_element);
  param_object_list.push_back(element);
  param_object_list.push_back(material);
  param_object_list.push_back(effect_gl);
  param_object_list.push_back(semantic_manager_->sas_param_object());
  DoScanGLEffectParameters(semantic_manager_,
                           renderer_,
                           this,
                           gl_program,
                           effect_gl,
                           param_object_list);
}

}  // namespace o3d

