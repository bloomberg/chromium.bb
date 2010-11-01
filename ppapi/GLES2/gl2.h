// Copyright (c 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __gl2_h_
#define __gl2_h_

#include "../c/dev/ppb_opengles_dev.h"
/*
 * This document is licensed under the SGI Free Software B License Version
 * 2.0. For details, see http://oss.sgi.com/projects/FreeB/ .
 */

/* OpenGL ES core versions */
#define GL_ES_VERSION_2_0                 1

/* ClearBufferMask */
#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_STENCIL_BUFFER_BIT             0x00000400
#define GL_COLOR_BUFFER_BIT               0x00004000

/* Boolean */
#define GL_FALSE                          0
#define GL_TRUE                           1

/* BeginMode */
#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_LINE_LOOP                      0x0002
#define GL_LINE_STRIP                     0x0003
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_TRIANGLE_FAN                   0x0006

/* AlphaFunction (not supported in ES20 */
/*      GL_NEVER */
/*      GL_LESS */
/*      GL_EQUAL */
/*      GL_LEQUAL */
/*      GL_GREATER */
/*      GL_NOTEQUAL */
/*      GL_GEQUAL */
/*      GL_ALWAYS */

/* BlendingFactorDest */
#define GL_ZERO                           0
#define GL_ONE                            1
#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_ALPHA                      0x0304
#define GL_ONE_MINUS_DST_ALPHA            0x0305

/* BlendingFactorSrc */
/*      GL_ZERO */
/*      GL_ONE */
#define GL_DST_COLOR                      0x0306
#define GL_ONE_MINUS_DST_COLOR            0x0307
#define GL_SRC_ALPHA_SATURATE             0x0308
/*      GL_SRC_ALPHA */
/*      GL_ONE_MINUS_SRC_ALPHA */
/*      GL_DST_ALPHA */
/*      GL_ONE_MINUS_DST_ALPHA */

/* BlendEquationSeparate */
#define GL_FUNC_ADD                       0x8006
#define GL_BLEND_EQUATION                 0x8009
#define GL_BLEND_EQUATION_RGB             0x8009    /* same as BLEND_EQUATION */
#define GL_BLEND_EQUATION_ALPHA           0x883D

/* BlendSubtract */
#define GL_FUNC_SUBTRACT                  0x800A
#define GL_FUNC_REVERSE_SUBTRACT          0x800B

/* Separate Blend Functions */
#define GL_BLEND_DST_RGB                  0x80C8
#define GL_BLEND_SRC_RGB                  0x80C9
#define GL_BLEND_DST_ALPHA                0x80CA
#define GL_BLEND_SRC_ALPHA                0x80CB
#define GL_CONSTANT_COLOR                 0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR       0x8002
#define GL_CONSTANT_ALPHA                 0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA       0x8004
#define GL_BLEND_COLOR                    0x8005

/* Buffer Objects */
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_ARRAY_BUFFER_BINDING           0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING   0x8895

#define GL_STREAM_DRAW                    0x88E0
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8

#define GL_BUFFER_SIZE                    0x8764
#define GL_BUFFER_USAGE                   0x8765

#define GL_CURRENT_VERTEX_ATTRIB          0x8626

/* CullFaceMode */
#define GL_FRONT                          0x0404
#define GL_BACK                           0x0405
#define GL_FRONT_AND_BACK                 0x0408

/* DepthFunction */
/*      GL_NEVER */
/*      GL_LESS */
/*      GL_EQUAL */
/*      GL_LEQUAL */
/*      GL_GREATER */
/*      GL_NOTEQUAL */
/*      GL_GEQUAL */
/*      GL_ALWAYS */

/* EnableCap */
#define GL_TEXTURE_2D                     0x0DE1
#define GL_CULL_FACE                      0x0B44
#define GL_BLEND                          0x0BE2
#define GL_DITHER                         0x0BD0
#define GL_STENCIL_TEST                   0x0B90
#define GL_DEPTH_TEST                     0x0B71
#define GL_SCISSOR_TEST                   0x0C11
#define GL_POLYGON_OFFSET_FILL            0x8037
#define GL_SAMPLE_ALPHA_TO_COVERAGE       0x809E
#define GL_SAMPLE_COVERAGE                0x80A0

/* ErrorCode */
#define GL_NO_ERROR                       0
#define GL_INVALID_ENUM                   0x0500
#define GL_INVALID_VALUE                  0x0501
#define GL_INVALID_OPERATION              0x0502
#define GL_OUT_OF_MEMORY                  0x0505
#define GL_CONTEXT_LOST                   0x300E  // TODO(gman: What value?

/* FrontFaceDirection */
#define GL_CW                             0x0900
#define GL_CCW                            0x0901

/* GetPName */
#define GL_LINE_WIDTH                     0x0B21
#define GL_ALIASED_POINT_SIZE_RANGE       0x846D
#define GL_ALIASED_LINE_WIDTH_RANGE       0x846E
#define GL_CULL_FACE_MODE                 0x0B45
#define GL_FRONT_FACE                     0x0B46
#define GL_DEPTH_RANGE                    0x0B70
#define GL_DEPTH_WRITEMASK                0x0B72
#define GL_DEPTH_CLEAR_VALUE              0x0B73
#define GL_DEPTH_FUNC                     0x0B74
#define GL_STENCIL_CLEAR_VALUE            0x0B91
#define GL_STENCIL_FUNC                   0x0B92
#define GL_STENCIL_FAIL                   0x0B94
#define GL_STENCIL_PASS_DEPTH_FAIL        0x0B95
#define GL_STENCIL_PASS_DEPTH_PASS        0x0B96
#define GL_STENCIL_REF                    0x0B97
#define GL_STENCIL_VALUE_MASK             0x0B93
#define GL_STENCIL_WRITEMASK              0x0B98
#define GL_STENCIL_BACK_FUNC              0x8800
#define GL_STENCIL_BACK_FAIL              0x8801
#define GL_STENCIL_BACK_PASS_DEPTH_FAIL   0x8802
#define GL_STENCIL_BACK_PASS_DEPTH_PASS   0x8803
#define GL_STENCIL_BACK_REF               0x8CA3
#define GL_STENCIL_BACK_VALUE_MASK        0x8CA4
#define GL_STENCIL_BACK_WRITEMASK         0x8CA5
#define GL_VIEWPORT                       0x0BA2
#define GL_SCISSOR_BOX                    0x0C10
/*      GL_SCISSOR_TEST */
#define GL_COLOR_CLEAR_VALUE              0x0C22
#define GL_COLOR_WRITEMASK                0x0C23
#define GL_UNPACK_ALIGNMENT               0x0CF5
#define GL_PACK_ALIGNMENT                 0x0D05
#define GL_MAX_TEXTURE_SIZE               0x0D33
#define GL_MAX_VIEWPORT_DIMS              0x0D3A
#define GL_SUBPIXEL_BITS                  0x0D50
#define GL_RED_BITS                       0x0D52
#define GL_GREEN_BITS                     0x0D53
#define GL_BLUE_BITS                      0x0D54
#define GL_ALPHA_BITS                     0x0D55
#define GL_DEPTH_BITS                     0x0D56
#define GL_STENCIL_BITS                   0x0D57
#define GL_POLYGON_OFFSET_UNITS           0x2A00
/*      GL_POLYGON_OFFSET_FILL */
#define GL_POLYGON_OFFSET_FACTOR          0x8038
#define GL_TEXTURE_BINDING_2D             0x8069
#define GL_SAMPLE_BUFFERS                 0x80A8
#define GL_SAMPLES                        0x80A9
#define GL_SAMPLE_COVERAGE_VALUE          0x80AA
#define GL_SAMPLE_COVERAGE_INVERT         0x80AB

/* GetTextureParameter */
/*      GL_TEXTURE_MAG_FILTER */
/*      GL_TEXTURE_MIN_FILTER */
/*      GL_TEXTURE_WRAP_S */
/*      GL_TEXTURE_WRAP_T */

#define GL_NUM_COMPRESSED_TEXTURE_FORMATS 0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS     0x86A3

/* HintMode */
#define GL_DONT_CARE                      0x1100
#define GL_FASTEST                        0x1101
#define GL_NICEST                         0x1102

/* HintTarget */
#define GL_GENERATE_MIPMAP_HINT            0x8192

/* DataType */
#define GL_BYTE                           0x1400
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_SHORT                          0x1402
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405
#define GL_FLOAT                          0x1406
#define GL_FIXED                          0x140C

/* PixelFormat */
#define GL_DEPTH_COMPONENT                0x1902
#define GL_ALPHA                          0x1906
#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_LUMINANCE                      0x1909
#define GL_LUMINANCE_ALPHA                0x190A

/* PixelType */
/*      GL_UNSIGNED_BYTE */
#define GL_UNSIGNED_SHORT_4_4_4_4         0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1         0x8034
#define GL_UNSIGNED_SHORT_5_6_5           0x8363

/* Shaders */
#define GL_FRAGMENT_SHADER                  0x8B30
#define GL_VERTEX_SHADER                    0x8B31
#define GL_MAX_VERTEX_ATTRIBS               0x8869
#define GL_MAX_VERTEX_UNIFORM_VECTORS       0x8DFB
#define GL_MAX_VARYING_VECTORS              0x8DFC
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS   0x8B4C
#define GL_MAX_TEXTURE_IMAGE_UNITS          0x8872
#define GL_MAX_FRAGMENT_UNIFORM_VECTORS     0x8DFD
#define GL_SHADER_TYPE                      0x8B4F
#define GL_DELETE_STATUS                    0x8B80
#define GL_LINK_STATUS                      0x8B82
#define GL_VALIDATE_STATUS                  0x8B83
#define GL_ATTACHED_SHADERS                 0x8B85
#define GL_ACTIVE_UNIFORMS                  0x8B86
#define GL_ACTIVE_UNIFORM_MAX_LENGTH        0x8B87
#define GL_ACTIVE_ATTRIBUTES                0x8B89
#define GL_ACTIVE_ATTRIBUTE_MAX_LENGTH      0x8B8A
#define GL_SHADING_LANGUAGE_VERSION         0x8B8C
#define GL_CURRENT_PROGRAM                  0x8B8D

/* StencilFunction */
#define GL_NEVER                          0x0200
#define GL_LESS                           0x0201
#define GL_EQUAL                          0x0202
#define GL_LEQUAL                         0x0203
#define GL_GREATER                        0x0204
#define GL_NOTEQUAL                       0x0205
#define GL_GEQUAL                         0x0206
#define GL_ALWAYS                         0x0207

/* StencilOp */
/*      GL_ZERO */
#define GL_KEEP                           0x1E00
#define GL_REPLACE                        0x1E01
#define GL_INCR                           0x1E02
#define GL_DECR                           0x1E03
#define GL_INVERT                         0x150A
#define GL_INCR_WRAP                      0x8507
#define GL_DECR_WRAP                      0x8508

/* StringName */
#define GL_VENDOR                         0x1F00
#define GL_RENDERER                       0x1F01
#define GL_VERSION                        0x1F02
#define GL_EXTENSIONS                     0x1F03

/* TextureMagFilter */
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601

/* TextureMinFilter */
/*      GL_NEAREST */
/*      GL_LINEAR */
#define GL_NEAREST_MIPMAP_NEAREST         0x2700
#define GL_LINEAR_MIPMAP_NEAREST          0x2701
#define GL_NEAREST_MIPMAP_LINEAR          0x2702
#define GL_LINEAR_MIPMAP_LINEAR           0x2703

/* TextureParameterName */
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803

/* TextureTarget */
/*      GL_TEXTURE_2D */
#define GL_TEXTURE                        0x1702

#define GL_TEXTURE_CUBE_MAP               0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP       0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X    0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X    0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y    0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y    0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z    0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z    0x851A
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE      0x851C

/* TextureUnit */
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE1                       0x84C1
#define GL_TEXTURE2                       0x84C2
#define GL_TEXTURE3                       0x84C3
#define GL_TEXTURE4                       0x84C4
#define GL_TEXTURE5                       0x84C5
#define GL_TEXTURE6                       0x84C6
#define GL_TEXTURE7                       0x84C7
#define GL_TEXTURE8                       0x84C8
#define GL_TEXTURE9                       0x84C9
#define GL_TEXTURE10                      0x84CA
#define GL_TEXTURE11                      0x84CB
#define GL_TEXTURE12                      0x84CC
#define GL_TEXTURE13                      0x84CD
#define GL_TEXTURE14                      0x84CE
#define GL_TEXTURE15                      0x84CF
#define GL_TEXTURE16                      0x84D0
#define GL_TEXTURE17                      0x84D1
#define GL_TEXTURE18                      0x84D2
#define GL_TEXTURE19                      0x84D3
#define GL_TEXTURE20                      0x84D4
#define GL_TEXTURE21                      0x84D5
#define GL_TEXTURE22                      0x84D6
#define GL_TEXTURE23                      0x84D7
#define GL_TEXTURE24                      0x84D8
#define GL_TEXTURE25                      0x84D9
#define GL_TEXTURE26                      0x84DA
#define GL_TEXTURE27                      0x84DB
#define GL_TEXTURE28                      0x84DC
#define GL_TEXTURE29                      0x84DD
#define GL_TEXTURE30                      0x84DE
#define GL_TEXTURE31                      0x84DF
#define GL_ACTIVE_TEXTURE                 0x84E0

/* TextureWrapMode */
#define GL_REPEAT                         0x2901
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_MIRRORED_REPEAT                0x8370

/* Uniform Types */
#define GL_FLOAT_VEC2                     0x8B50
#define GL_FLOAT_VEC3                     0x8B51
#define GL_FLOAT_VEC4                     0x8B52
#define GL_INT_VEC2                       0x8B53
#define GL_INT_VEC3                       0x8B54
#define GL_INT_VEC4                       0x8B55
#define GL_BOOL                           0x8B56
#define GL_BOOL_VEC2                      0x8B57
#define GL_BOOL_VEC3                      0x8B58
#define GL_BOOL_VEC4                      0x8B59
#define GL_FLOAT_MAT2                     0x8B5A
#define GL_FLOAT_MAT3                     0x8B5B
#define GL_FLOAT_MAT4                     0x8B5C
#define GL_SAMPLER_2D                     0x8B5E
#define GL_SAMPLER_CUBE                   0x8B60

/* Vertex Arrays */
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED        0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE           0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE         0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE           0x8625
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED     0x886A
#define GL_VERTEX_ATTRIB_ARRAY_POINTER        0x8645
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING 0x889F

/* Read Format */
#define GL_IMPLEMENTATION_COLOR_READ_TYPE   0x8B9A
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT 0x8B9B

/* Shader Source */
#define GL_COMPILE_STATUS                 0x8B81
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_SHADER_SOURCE_LENGTH           0x8B88
#define GL_SHADER_COMPILER                0x8DFA

/* Shader Binary */
#define GL_SHADER_BINARY_FORMATS          0x8DF8
#define GL_NUM_SHADER_BINARY_FORMATS      0x8DF9

/* Shader Precision-Specified Types */
#define GL_LOW_FLOAT                      0x8DF0
#define GL_MEDIUM_FLOAT                   0x8DF1
#define GL_HIGH_FLOAT                     0x8DF2
#define GL_LOW_INT                        0x8DF3
#define GL_MEDIUM_INT                     0x8DF4
#define GL_HIGH_INT                       0x8DF5

/* Framebuffer Object. */
#define GL_FRAMEBUFFER                    0x8D40
#define GL_RENDERBUFFER                   0x8D41

#define GL_RGBA4                          0x8056
#define GL_RGB5_A1                        0x8057
#define GL_RGB565                         0x8D62
#define GL_DEPTH_COMPONENT16              0x81A5
#define GL_STENCIL_INDEX                  0x1901
#define GL_STENCIL_INDEX8                 0x8D48

#define GL_RENDERBUFFER_WIDTH             0x8D42
#define GL_RENDERBUFFER_HEIGHT            0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT   0x8D44
#define GL_RENDERBUFFER_RED_SIZE          0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE        0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE         0x8D52
#define GL_RENDERBUFFER_ALPHA_SIZE        0x8D53
#define GL_RENDERBUFFER_DEPTH_SIZE        0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE      0x8D55

#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE           0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME           0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL         0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE 0x8CD3

#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_DEPTH_ATTACHMENT               0x8D00
#define GL_STENCIL_ATTACHMENT             0x8D20

#define GL_NONE                           0

#define GL_FRAMEBUFFER_COMPLETE                      0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT         0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS         0x8CD9
#define GL_FRAMEBUFFER_UNSUPPORTED                   0x8CDD

#define GL_FRAMEBUFFER_BINDING            0x8CA6
#define GL_RENDERBUFFER_BINDING           0x8CA7
#define GL_MAX_RENDERBUFFER_SIZE          0x84E8

#define GL_INVALID_FRAMEBUFFER_OPERATION  0x0506


/*-------------------------------------------------------------------------
 * GL core functions.
 *-----------------------------------------------------------------------*/
#undef GL_APICALL
#define GL_APICALL
#undef GL_APIENTRY
#define GL_APIENTRY

// The client must set this to point to the Pepper OpenGLES interface once it
// is obtained. PPAPI C++ wrappers will do this for you.
#ifdef __cplusplus
extern "C" {
#endif
extern const struct PPB_OpenGLES_Dev* pepper_opengl_interface;
#ifdef __cplusplus
}
#endif

#define glActiveTexture pepper_opengl_interface->ActiveTexture
#define glAttachShader pepper_opengl_interface->AttachShader
#define glBindAttribLocation pepper_opengl_interface->BindAttribLocation
#define glBindBuffer pepper_opengl_interface->BindBuffer
#define glBindFramebuffer pepper_opengl_interface->BindFramebuffer
#define glBindRenderbuffer pepper_opengl_interface->BindRenderbuffer
#define glBindTexture pepper_opengl_interface->BindTexture
#define glBlendColor pepper_opengl_interface->BlendColor
#define glBlendEquation pepper_opengl_interface->BlendEquation
#define glBlendEquationSeparate pepper_opengl_interface->BlendEquationSeparate
#define glBlendFunc pepper_opengl_interface->BlendFunc
#define glBlendFuncSeparate pepper_opengl_interface->BlendFuncSeparate
#define glBufferData pepper_opengl_interface->BufferData
#define glBufferSubData pepper_opengl_interface->BufferSubData
#define glCheckFramebufferStatus pepper_opengl_interface->CheckFramebufferStatus
#define glClear pepper_opengl_interface->Clear
#define glClearColor pepper_opengl_interface->ClearColor
#define glClearDepthf pepper_opengl_interface->ClearDepthf
#define glClearStencil pepper_opengl_interface->ClearStencil
#define glColorMask pepper_opengl_interface->ColorMask
#define glCompileShader pepper_opengl_interface->CompileShader
#define glCompressedTexImage2D pepper_opengl_interface->CompressedTexImage2D
#define glCompressedTexSubImage2D pepper_opengl_interface->CompressedTexSubImage2D
#define glCopyTexImage2D pepper_opengl_interface->CopyTexImage2D
#define glCopyTexSubImage2D pepper_opengl_interface->CopyTexSubImage2D
#define glCreateProgram pepper_opengl_interface->CreateProgram
#define glCreateShader pepper_opengl_interface->CreateShader
#define glCullFace pepper_opengl_interface->CullFace
#define glDeleteBuffers pepper_opengl_interface->DeleteBuffers
#define glDeleteFramebuffers pepper_opengl_interface->DeleteFramebuffers
#define glDeleteProgram pepper_opengl_interface->DeleteProgram
#define glDeleteRenderbuffers pepper_opengl_interface->DeleteRenderbuffers
#define glDeleteShader pepper_opengl_interface->DeleteShader
#define glDeleteTextures pepper_opengl_interface->DeleteTextures
#define glDepthFunc pepper_opengl_interface->DepthFunc
#define glDepthMask pepper_opengl_interface->DepthMask
#define glDepthRangef pepper_opengl_interface->DepthRangef
#define glDetachShader pepper_opengl_interface->DetachShader
#define glDisable pepper_opengl_interface->Disable
#define glDisableVertexAttribArray pepper_opengl_interface->DisableVertexAttribArray
#define glDrawArrays pepper_opengl_interface->DrawArrays
#define glDrawElements pepper_opengl_interface->DrawElements
#define glEnable pepper_opengl_interface->Enable
#define glEnableVertexAttribArray pepper_opengl_interface->EnableVertexAttribArray
#define glFinish pepper_opengl_interface->Finish
#define glFlush pepper_opengl_interface->Flush
#define glFramebufferRenderbuffer pepper_opengl_interface->FramebufferRenderbuffer
#define glFramebufferTexture2D pepper_opengl_interface->FramebufferTexture2D
#define glFrontFace pepper_opengl_interface->FrontFace
#define glGenBuffers pepper_opengl_interface->GenBuffers
#define glGenerateMipmap pepper_opengl_interface->GenerateMipmap
#define glGenFramebuffers pepper_opengl_interface->GenFramebuffers
#define glGenRenderbuffers pepper_opengl_interface->GenRenderbuffers
#define glGenTextures pepper_opengl_interface->GenTextures
#define glGetActiveAttrib pepper_opengl_interface->GetActiveAttrib
#define glGetActiveUniform pepper_opengl_interface->GetActiveUniform
#define glGetAttachedShaders pepper_opengl_interface->GetAttachedShaders
#define glGetAttribLocation pepper_opengl_interface->GetAttribLocation
#define glGetBooleanv pepper_opengl_interface->GetBooleanv
#define glGetBufferParameteriv pepper_opengl_interface->GetBufferParameteriv
#define glGetError pepper_opengl_interface->GetError
#define glGetFloatv pepper_opengl_interface->GetFloatv
#define glGetFramebufferAttachmentParameteriv pepper_opengl_interface->GetFramebufferAttachmentParameteriv
#define glGetIntegerv pepper_opengl_interface->GetIntegerv
#define glGetProgramiv pepper_opengl_interface->GetProgramiv
#define glGetProgramInfoLog pepper_opengl_interface->GetProgramInfoLog
#define glGetRenderbufferParameteriv pepper_opengl_interface->GetRenderbufferParameteriv
#define glGetShaderiv pepper_opengl_interface->GetShaderiv
#define glGetShaderInfoLog pepper_opengl_interface->GetShaderInfoLog
#define glGetShaderPrecisionFormat pepper_opengl_interface->GetShaderPrecisionFormat
#define glGetShaderSource pepper_opengl_interface->GetShaderSource
#define glGetString pepper_opengl_interface->GetString
#define glGetTexParameterfv pepper_opengl_interface->GetTexParameterfv
#define glGetTexParameteriv pepper_opengl_interface->GetTexParameteriv
#define glGetUniformfv pepper_opengl_interface->GetUniformfv
#define glGetUniformiv pepper_opengl_interface->GetUniformiv
#define glGetUniformLocation pepper_opengl_interface->GetUniformLocation
#define glGetVertexAttribfv pepper_opengl_interface->GetVertexAttribfv
#define glGetVertexAttribiv pepper_opengl_interface->GetVertexAttribiv
#define glGetVertexAttribPointerv pepper_opengl_interface->GetVertexAttribPointerv
#define glHint pepper_opengl_interface->Hint
#define glIsBuffer pepper_opengl_interface->IsBuffer
#define glIsEnabled pepper_opengl_interface->IsEnabled
#define glIsFramebuffer pepper_opengl_interface->IsFramebuffer
#define glIsProgram pepper_opengl_interface->IsProgram
#define glIsRenderbuffer pepper_opengl_interface->IsRenderbuffer
#define glIsShader pepper_opengl_interface->IsShader
#define glIsTexture pepper_opengl_interface->IsTexture
#define glLineWidth pepper_opengl_interface->LineWidth
#define glLinkProgram pepper_opengl_interface->LinkProgram
#define glPixelStorei pepper_opengl_interface->PixelStorei
#define glPolygonOffset pepper_opengl_interface->PolygonOffset
#define glReadPixels pepper_opengl_interface->ReadPixels
#define glReleaseShaderCompiler pepper_opengl_interface->ReleaseShaderCompiler
#define glRenderbufferStorage pepper_opengl_interface->RenderbufferStorage
#define glSampleCoverage pepper_opengl_interface->SampleCoverage
#define glScissor pepper_opengl_interface->Scissor
#define glShaderBinary pepper_opengl_interface->ShaderBinary
#define glShaderSource pepper_opengl_interface->ShaderSource
#define glStencilFunc pepper_opengl_interface->StencilFunc
#define glStencilFuncSeparate pepper_opengl_interface->StencilFuncSeparate
#define glStencilMask pepper_opengl_interface->StencilMask
#define glStencilMaskSeparate pepper_opengl_interface->StencilMaskSeparate
#define glStencilOp pepper_opengl_interface->StencilOp
#define glStencilOpSeparate pepper_opengl_interface->StencilOpSeparate
#define glTexImage2D pepper_opengl_interface->TexImage2D
#define glTexParameterf pepper_opengl_interface->TexParameterf
#define glTexParameterfv pepper_opengl_interface->TexParameterfv
#define glTexParameteri pepper_opengl_interface->TexParameteri
#define glTexParameteriv pepper_opengl_interface->TexParameteriv
#define glTexSubImage2D pepper_opengl_interface->TexSubImage2D
#define glUniform1f pepper_opengl_interface->Uniform1f
#define glUniform1fv pepper_opengl_interface->Uniform1fv
#define glUniform1i pepper_opengl_interface->Uniform1i
#define glUniform1iv pepper_opengl_interface->Uniform1iv
#define glUniform2f pepper_opengl_interface->Uniform2f
#define glUniform2fv pepper_opengl_interface->Uniform2fv
#define glUniform2i pepper_opengl_interface->Uniform2i
#define glUniform2iv pepper_opengl_interface->Uniform2iv
#define glUniform3f pepper_opengl_interface->Uniform3f
#define glUniform3fv pepper_opengl_interface->Uniform3fv
#define glUniform3i pepper_opengl_interface->Uniform3i
#define glUniform3iv pepper_opengl_interface->Uniform3iv
#define glUniform4f pepper_opengl_interface->Uniform4f
#define glUniform4fv pepper_opengl_interface->Uniform4fv
#define glUniform4i pepper_opengl_interface->Uniform4i
#define glUniform4iv pepper_opengl_interface->Uniform4iv
#define glUniformMatrix2fv pepper_opengl_interface->UniformMatrix2fv
#define glUniformMatrix3fv pepper_opengl_interface->UniformMatrix3fv
#define glUniformMatrix4fv pepper_opengl_interface->UniformMatrix4fv
#define glUseProgram pepper_opengl_interface->UseProgram
#define glValidateProgram pepper_opengl_interface->ValidateProgram
#define glVertexAttrib1f pepper_opengl_interface->VertexAttrib1f
#define glVertexAttrib1fv pepper_opengl_interface->VertexAttrib1fv
#define glVertexAttrib2f pepper_opengl_interface->VertexAttrib2f
#define glVertexAttrib2fv pepper_opengl_interface->VertexAttrib2fv
#define glVertexAttrib3f pepper_opengl_interface->VertexAttrib3f
#define glVertexAttrib3fv pepper_opengl_interface->VertexAttrib3fv
#define glVertexAttrib4f pepper_opengl_interface->VertexAttrib4f
#define glVertexAttrib4fv pepper_opengl_interface->VertexAttrib4fv
#define glVertexAttribPointer pepper_opengl_interface->VertexAttribPointer
#define glViewport pepper_opengl_interface->Viewport

#endif /* __gl2_h_ */

