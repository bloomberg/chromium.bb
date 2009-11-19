
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for gles2 commmand ids

#include "tests/common/win/testing_common.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"

namespace command_buffer {
namespace gles2 {

// *** These IDs MUST NOT CHANGE!!! ***
// Changing them will break all client programs.
TEST(GLES2CommandIdTest, CommandIdsMatch) {
  COMPILE_ASSERT(ActiveTexture::kCmdId == 1024,
                 GLES2_ActiveTexture_kCmdId_mismatch);
  COMPILE_ASSERT(AttachShader::kCmdId == 1025,
                 GLES2_AttachShader_kCmdId_mismatch);
  COMPILE_ASSERT(BindAttribLocation::kCmdId == 1026,
                 GLES2_BindAttribLocation_kCmdId_mismatch);
  COMPILE_ASSERT(BindAttribLocationImmediate::kCmdId == 1027,
                 GLES2_BindAttribLocationImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(BindBuffer::kCmdId == 1028,
                 GLES2_BindBuffer_kCmdId_mismatch);
  COMPILE_ASSERT(BindFramebuffer::kCmdId == 1029,
                 GLES2_BindFramebuffer_kCmdId_mismatch);
  COMPILE_ASSERT(BindRenderbuffer::kCmdId == 1030,
                 GLES2_BindRenderbuffer_kCmdId_mismatch);
  COMPILE_ASSERT(BindTexture::kCmdId == 1031,
                 GLES2_BindTexture_kCmdId_mismatch);
  COMPILE_ASSERT(BlendColor::kCmdId == 1032,
                 GLES2_BlendColor_kCmdId_mismatch);
  COMPILE_ASSERT(BlendEquation::kCmdId == 1033,
                 GLES2_BlendEquation_kCmdId_mismatch);
  COMPILE_ASSERT(BlendEquationSeparate::kCmdId == 1034,
                 GLES2_BlendEquationSeparate_kCmdId_mismatch);
  COMPILE_ASSERT(BlendFunc::kCmdId == 1035,
                 GLES2_BlendFunc_kCmdId_mismatch);
  COMPILE_ASSERT(BlendFuncSeparate::kCmdId == 1036,
                 GLES2_BlendFuncSeparate_kCmdId_mismatch);
  COMPILE_ASSERT(BufferData::kCmdId == 1037,
                 GLES2_BufferData_kCmdId_mismatch);
  COMPILE_ASSERT(BufferDataImmediate::kCmdId == 1038,
                 GLES2_BufferDataImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(BufferSubData::kCmdId == 1039,
                 GLES2_BufferSubData_kCmdId_mismatch);
  COMPILE_ASSERT(BufferSubDataImmediate::kCmdId == 1040,
                 GLES2_BufferSubDataImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(CheckFramebufferStatus::kCmdId == 1041,
                 GLES2_CheckFramebufferStatus_kCmdId_mismatch);
  COMPILE_ASSERT(Clear::kCmdId == 1042,
                 GLES2_Clear_kCmdId_mismatch);
  COMPILE_ASSERT(ClearColor::kCmdId == 1043,
                 GLES2_ClearColor_kCmdId_mismatch);
  COMPILE_ASSERT(ClearDepthf::kCmdId == 1044,
                 GLES2_ClearDepthf_kCmdId_mismatch);
  COMPILE_ASSERT(ClearStencil::kCmdId == 1045,
                 GLES2_ClearStencil_kCmdId_mismatch);
  COMPILE_ASSERT(ColorMask::kCmdId == 1046,
                 GLES2_ColorMask_kCmdId_mismatch);
  COMPILE_ASSERT(CompileShader::kCmdId == 1047,
                 GLES2_CompileShader_kCmdId_mismatch);
  COMPILE_ASSERT(CompressedTexImage2D::kCmdId == 1048,
                 GLES2_CompressedTexImage2D_kCmdId_mismatch);
  COMPILE_ASSERT(CompressedTexImage2DImmediate::kCmdId == 1049,
                 GLES2_CompressedTexImage2DImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(CompressedTexSubImage2D::kCmdId == 1050,
                 GLES2_CompressedTexSubImage2D_kCmdId_mismatch);
  COMPILE_ASSERT(CompressedTexSubImage2DImmediate::kCmdId == 1051,
                 GLES2_CompressedTexSubImage2DImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(CopyTexImage2D::kCmdId == 1052,
                 GLES2_CopyTexImage2D_kCmdId_mismatch);
  COMPILE_ASSERT(CopyTexSubImage2D::kCmdId == 1053,
                 GLES2_CopyTexSubImage2D_kCmdId_mismatch);
  COMPILE_ASSERT(CreateProgram::kCmdId == 1054,
                 GLES2_CreateProgram_kCmdId_mismatch);
  COMPILE_ASSERT(CreateShader::kCmdId == 1055,
                 GLES2_CreateShader_kCmdId_mismatch);
  COMPILE_ASSERT(CullFace::kCmdId == 1056,
                 GLES2_CullFace_kCmdId_mismatch);
  COMPILE_ASSERT(DeleteBuffers::kCmdId == 1057,
                 GLES2_DeleteBuffers_kCmdId_mismatch);
  COMPILE_ASSERT(DeleteBuffersImmediate::kCmdId == 1058,
                 GLES2_DeleteBuffersImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(DeleteFramebuffers::kCmdId == 1059,
                 GLES2_DeleteFramebuffers_kCmdId_mismatch);
  COMPILE_ASSERT(DeleteFramebuffersImmediate::kCmdId == 1060,
                 GLES2_DeleteFramebuffersImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(DeleteProgram::kCmdId == 1061,
                 GLES2_DeleteProgram_kCmdId_mismatch);
  COMPILE_ASSERT(DeleteRenderbuffers::kCmdId == 1062,
                 GLES2_DeleteRenderbuffers_kCmdId_mismatch);
  COMPILE_ASSERT(DeleteRenderbuffersImmediate::kCmdId == 1063,
                 GLES2_DeleteRenderbuffersImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(DeleteShader::kCmdId == 1064,
                 GLES2_DeleteShader_kCmdId_mismatch);
  COMPILE_ASSERT(DeleteTextures::kCmdId == 1065,
                 GLES2_DeleteTextures_kCmdId_mismatch);
  COMPILE_ASSERT(DeleteTexturesImmediate::kCmdId == 1066,
                 GLES2_DeleteTexturesImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(DepthFunc::kCmdId == 1067,
                 GLES2_DepthFunc_kCmdId_mismatch);
  COMPILE_ASSERT(DepthMask::kCmdId == 1068,
                 GLES2_DepthMask_kCmdId_mismatch);
  COMPILE_ASSERT(DepthRangef::kCmdId == 1069,
                 GLES2_DepthRangef_kCmdId_mismatch);
  COMPILE_ASSERT(DetachShader::kCmdId == 1070,
                 GLES2_DetachShader_kCmdId_mismatch);
  COMPILE_ASSERT(Disable::kCmdId == 1071,
                 GLES2_Disable_kCmdId_mismatch);
  COMPILE_ASSERT(DisableVertexAttribArray::kCmdId == 1072,
                 GLES2_DisableVertexAttribArray_kCmdId_mismatch);
  COMPILE_ASSERT(DrawArrays::kCmdId == 1073,
                 GLES2_DrawArrays_kCmdId_mismatch);
  COMPILE_ASSERT(DrawElements::kCmdId == 1074,
                 GLES2_DrawElements_kCmdId_mismatch);
  COMPILE_ASSERT(Enable::kCmdId == 1075,
                 GLES2_Enable_kCmdId_mismatch);
  COMPILE_ASSERT(EnableVertexAttribArray::kCmdId == 1076,
                 GLES2_EnableVertexAttribArray_kCmdId_mismatch);
  COMPILE_ASSERT(Finish::kCmdId == 1077,
                 GLES2_Finish_kCmdId_mismatch);
  COMPILE_ASSERT(Flush::kCmdId == 1078,
                 GLES2_Flush_kCmdId_mismatch);
  COMPILE_ASSERT(FramebufferRenderbuffer::kCmdId == 1079,
                 GLES2_FramebufferRenderbuffer_kCmdId_mismatch);
  COMPILE_ASSERT(FramebufferTexture2D::kCmdId == 1080,
                 GLES2_FramebufferTexture2D_kCmdId_mismatch);
  COMPILE_ASSERT(FrontFace::kCmdId == 1081,
                 GLES2_FrontFace_kCmdId_mismatch);
  COMPILE_ASSERT(GenBuffers::kCmdId == 1082,
                 GLES2_GenBuffers_kCmdId_mismatch);
  COMPILE_ASSERT(GenBuffersImmediate::kCmdId == 1083,
                 GLES2_GenBuffersImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(GenerateMipmap::kCmdId == 1084,
                 GLES2_GenerateMipmap_kCmdId_mismatch);
  COMPILE_ASSERT(GenFramebuffers::kCmdId == 1085,
                 GLES2_GenFramebuffers_kCmdId_mismatch);
  COMPILE_ASSERT(GenFramebuffersImmediate::kCmdId == 1086,
                 GLES2_GenFramebuffersImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(GenRenderbuffers::kCmdId == 1087,
                 GLES2_GenRenderbuffers_kCmdId_mismatch);
  COMPILE_ASSERT(GenRenderbuffersImmediate::kCmdId == 1088,
                 GLES2_GenRenderbuffersImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(GenTextures::kCmdId == 1089,
                 GLES2_GenTextures_kCmdId_mismatch);
  COMPILE_ASSERT(GenTexturesImmediate::kCmdId == 1090,
                 GLES2_GenTexturesImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(GetActiveAttrib::kCmdId == 1091,
                 GLES2_GetActiveAttrib_kCmdId_mismatch);
  COMPILE_ASSERT(GetActiveUniform::kCmdId == 1092,
                 GLES2_GetActiveUniform_kCmdId_mismatch);
  COMPILE_ASSERT(GetAttachedShaders::kCmdId == 1093,
                 GLES2_GetAttachedShaders_kCmdId_mismatch);
  COMPILE_ASSERT(GetAttribLocation::kCmdId == 1094,
                 GLES2_GetAttribLocation_kCmdId_mismatch);
  COMPILE_ASSERT(GetAttribLocationImmediate::kCmdId == 1095,
                 GLES2_GetAttribLocationImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(GetBooleanv::kCmdId == 1096,
                 GLES2_GetBooleanv_kCmdId_mismatch);
  COMPILE_ASSERT(GetBufferParameteriv::kCmdId == 1097,
                 GLES2_GetBufferParameteriv_kCmdId_mismatch);
  COMPILE_ASSERT(GetError::kCmdId == 1098,
                 GLES2_GetError_kCmdId_mismatch);
  COMPILE_ASSERT(GetFloatv::kCmdId == 1099,
                 GLES2_GetFloatv_kCmdId_mismatch);
  COMPILE_ASSERT(GetFramebufferAttachmentParameteriv::kCmdId == 1100,
                 GLES2_GetFramebufferAttachmentParameteriv_kCmdId_mismatch);
  COMPILE_ASSERT(GetIntegerv::kCmdId == 1101,
                 GLES2_GetIntegerv_kCmdId_mismatch);
  COMPILE_ASSERT(GetProgramiv::kCmdId == 1102,
                 GLES2_GetProgramiv_kCmdId_mismatch);
  COMPILE_ASSERT(GetProgramInfoLog::kCmdId == 1103,
                 GLES2_GetProgramInfoLog_kCmdId_mismatch);
  COMPILE_ASSERT(GetRenderbufferParameteriv::kCmdId == 1104,
                 GLES2_GetRenderbufferParameteriv_kCmdId_mismatch);
  COMPILE_ASSERT(GetShaderiv::kCmdId == 1105,
                 GLES2_GetShaderiv_kCmdId_mismatch);
  COMPILE_ASSERT(GetShaderInfoLog::kCmdId == 1106,
                 GLES2_GetShaderInfoLog_kCmdId_mismatch);
  COMPILE_ASSERT(GetShaderPrecisionFormat::kCmdId == 1107,
                 GLES2_GetShaderPrecisionFormat_kCmdId_mismatch);
  COMPILE_ASSERT(GetShaderSource::kCmdId == 1108,
                 GLES2_GetShaderSource_kCmdId_mismatch);
  COMPILE_ASSERT(GetString::kCmdId == 1109,
                 GLES2_GetString_kCmdId_mismatch);
  COMPILE_ASSERT(GetTexParameterfv::kCmdId == 1110,
                 GLES2_GetTexParameterfv_kCmdId_mismatch);
  COMPILE_ASSERT(GetTexParameteriv::kCmdId == 1111,
                 GLES2_GetTexParameteriv_kCmdId_mismatch);
  COMPILE_ASSERT(GetUniformfv::kCmdId == 1112,
                 GLES2_GetUniformfv_kCmdId_mismatch);
  COMPILE_ASSERT(GetUniformiv::kCmdId == 1113,
                 GLES2_GetUniformiv_kCmdId_mismatch);
  COMPILE_ASSERT(GetUniformLocation::kCmdId == 1114,
                 GLES2_GetUniformLocation_kCmdId_mismatch);
  COMPILE_ASSERT(GetUniformLocationImmediate::kCmdId == 1115,
                 GLES2_GetUniformLocationImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(GetVertexAttribfv::kCmdId == 1116,
                 GLES2_GetVertexAttribfv_kCmdId_mismatch);
  COMPILE_ASSERT(GetVertexAttribiv::kCmdId == 1117,
                 GLES2_GetVertexAttribiv_kCmdId_mismatch);
  COMPILE_ASSERT(GetVertexAttribPointerv::kCmdId == 1118,
                 GLES2_GetVertexAttribPointerv_kCmdId_mismatch);
  COMPILE_ASSERT(Hint::kCmdId == 1119,
                 GLES2_Hint_kCmdId_mismatch);
  COMPILE_ASSERT(IsBuffer::kCmdId == 1120,
                 GLES2_IsBuffer_kCmdId_mismatch);
  COMPILE_ASSERT(IsEnabled::kCmdId == 1121,
                 GLES2_IsEnabled_kCmdId_mismatch);
  COMPILE_ASSERT(IsFramebuffer::kCmdId == 1122,
                 GLES2_IsFramebuffer_kCmdId_mismatch);
  COMPILE_ASSERT(IsProgram::kCmdId == 1123,
                 GLES2_IsProgram_kCmdId_mismatch);
  COMPILE_ASSERT(IsRenderbuffer::kCmdId == 1124,
                 GLES2_IsRenderbuffer_kCmdId_mismatch);
  COMPILE_ASSERT(IsShader::kCmdId == 1125,
                 GLES2_IsShader_kCmdId_mismatch);
  COMPILE_ASSERT(IsTexture::kCmdId == 1126,
                 GLES2_IsTexture_kCmdId_mismatch);
  COMPILE_ASSERT(LineWidth::kCmdId == 1127,
                 GLES2_LineWidth_kCmdId_mismatch);
  COMPILE_ASSERT(LinkProgram::kCmdId == 1128,
                 GLES2_LinkProgram_kCmdId_mismatch);
  COMPILE_ASSERT(PixelStorei::kCmdId == 1129,
                 GLES2_PixelStorei_kCmdId_mismatch);
  COMPILE_ASSERT(PolygonOffset::kCmdId == 1130,
                 GLES2_PolygonOffset_kCmdId_mismatch);
  COMPILE_ASSERT(ReadPixels::kCmdId == 1131,
                 GLES2_ReadPixels_kCmdId_mismatch);
  COMPILE_ASSERT(RenderbufferStorage::kCmdId == 1132,
                 GLES2_RenderbufferStorage_kCmdId_mismatch);
  COMPILE_ASSERT(SampleCoverage::kCmdId == 1133,
                 GLES2_SampleCoverage_kCmdId_mismatch);
  COMPILE_ASSERT(Scissor::kCmdId == 1134,
                 GLES2_Scissor_kCmdId_mismatch);
  COMPILE_ASSERT(ShaderSource::kCmdId == 1135,
                 GLES2_ShaderSource_kCmdId_mismatch);
  COMPILE_ASSERT(ShaderSourceImmediate::kCmdId == 1136,
                 GLES2_ShaderSourceImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(StencilFunc::kCmdId == 1137,
                 GLES2_StencilFunc_kCmdId_mismatch);
  COMPILE_ASSERT(StencilFuncSeparate::kCmdId == 1138,
                 GLES2_StencilFuncSeparate_kCmdId_mismatch);
  COMPILE_ASSERT(StencilMask::kCmdId == 1139,
                 GLES2_StencilMask_kCmdId_mismatch);
  COMPILE_ASSERT(StencilMaskSeparate::kCmdId == 1140,
                 GLES2_StencilMaskSeparate_kCmdId_mismatch);
  COMPILE_ASSERT(StencilOp::kCmdId == 1141,
                 GLES2_StencilOp_kCmdId_mismatch);
  COMPILE_ASSERT(StencilOpSeparate::kCmdId == 1142,
                 GLES2_StencilOpSeparate_kCmdId_mismatch);
  COMPILE_ASSERT(TexImage2D::kCmdId == 1143,
                 GLES2_TexImage2D_kCmdId_mismatch);
  COMPILE_ASSERT(TexImage2DImmediate::kCmdId == 1144,
                 GLES2_TexImage2DImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(TexParameterf::kCmdId == 1145,
                 GLES2_TexParameterf_kCmdId_mismatch);
  COMPILE_ASSERT(TexParameterfv::kCmdId == 1146,
                 GLES2_TexParameterfv_kCmdId_mismatch);
  COMPILE_ASSERT(TexParameterfvImmediate::kCmdId == 1147,
                 GLES2_TexParameterfvImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(TexParameteri::kCmdId == 1148,
                 GLES2_TexParameteri_kCmdId_mismatch);
  COMPILE_ASSERT(TexParameteriv::kCmdId == 1149,
                 GLES2_TexParameteriv_kCmdId_mismatch);
  COMPILE_ASSERT(TexParameterivImmediate::kCmdId == 1150,
                 GLES2_TexParameterivImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(TexSubImage2D::kCmdId == 1151,
                 GLES2_TexSubImage2D_kCmdId_mismatch);
  COMPILE_ASSERT(TexSubImage2DImmediate::kCmdId == 1152,
                 GLES2_TexSubImage2DImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform1f::kCmdId == 1153,
                 GLES2_Uniform1f_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform1fv::kCmdId == 1154,
                 GLES2_Uniform1fv_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform1fvImmediate::kCmdId == 1155,
                 GLES2_Uniform1fvImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform1i::kCmdId == 1156,
                 GLES2_Uniform1i_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform1iv::kCmdId == 1157,
                 GLES2_Uniform1iv_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform1ivImmediate::kCmdId == 1158,
                 GLES2_Uniform1ivImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform2f::kCmdId == 1159,
                 GLES2_Uniform2f_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform2fv::kCmdId == 1160,
                 GLES2_Uniform2fv_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform2fvImmediate::kCmdId == 1161,
                 GLES2_Uniform2fvImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform2i::kCmdId == 1162,
                 GLES2_Uniform2i_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform2iv::kCmdId == 1163,
                 GLES2_Uniform2iv_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform2ivImmediate::kCmdId == 1164,
                 GLES2_Uniform2ivImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform3f::kCmdId == 1165,
                 GLES2_Uniform3f_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform3fv::kCmdId == 1166,
                 GLES2_Uniform3fv_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform3fvImmediate::kCmdId == 1167,
                 GLES2_Uniform3fvImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform3i::kCmdId == 1168,
                 GLES2_Uniform3i_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform3iv::kCmdId == 1169,
                 GLES2_Uniform3iv_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform3ivImmediate::kCmdId == 1170,
                 GLES2_Uniform3ivImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform4f::kCmdId == 1171,
                 GLES2_Uniform4f_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform4fv::kCmdId == 1172,
                 GLES2_Uniform4fv_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform4fvImmediate::kCmdId == 1173,
                 GLES2_Uniform4fvImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform4i::kCmdId == 1174,
                 GLES2_Uniform4i_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform4iv::kCmdId == 1175,
                 GLES2_Uniform4iv_kCmdId_mismatch);
  COMPILE_ASSERT(Uniform4ivImmediate::kCmdId == 1176,
                 GLES2_Uniform4ivImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(UniformMatrix2fv::kCmdId == 1177,
                 GLES2_UniformMatrix2fv_kCmdId_mismatch);
  COMPILE_ASSERT(UniformMatrix2fvImmediate::kCmdId == 1178,
                 GLES2_UniformMatrix2fvImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(UniformMatrix3fv::kCmdId == 1179,
                 GLES2_UniformMatrix3fv_kCmdId_mismatch);
  COMPILE_ASSERT(UniformMatrix3fvImmediate::kCmdId == 1180,
                 GLES2_UniformMatrix3fvImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(UniformMatrix4fv::kCmdId == 1181,
                 GLES2_UniformMatrix4fv_kCmdId_mismatch);
  COMPILE_ASSERT(UniformMatrix4fvImmediate::kCmdId == 1182,
                 GLES2_UniformMatrix4fvImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(UseProgram::kCmdId == 1183,
                 GLES2_UseProgram_kCmdId_mismatch);
  COMPILE_ASSERT(ValidateProgram::kCmdId == 1184,
                 GLES2_ValidateProgram_kCmdId_mismatch);
  COMPILE_ASSERT(VertexAttrib1f::kCmdId == 1185,
                 GLES2_VertexAttrib1f_kCmdId_mismatch);
  COMPILE_ASSERT(VertexAttrib1fv::kCmdId == 1186,
                 GLES2_VertexAttrib1fv_kCmdId_mismatch);
  COMPILE_ASSERT(VertexAttrib1fvImmediate::kCmdId == 1187,
                 GLES2_VertexAttrib1fvImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(VertexAttrib2f::kCmdId == 1188,
                 GLES2_VertexAttrib2f_kCmdId_mismatch);
  COMPILE_ASSERT(VertexAttrib2fv::kCmdId == 1189,
                 GLES2_VertexAttrib2fv_kCmdId_mismatch);
  COMPILE_ASSERT(VertexAttrib2fvImmediate::kCmdId == 1190,
                 GLES2_VertexAttrib2fvImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(VertexAttrib3f::kCmdId == 1191,
                 GLES2_VertexAttrib3f_kCmdId_mismatch);
  COMPILE_ASSERT(VertexAttrib3fv::kCmdId == 1192,
                 GLES2_VertexAttrib3fv_kCmdId_mismatch);
  COMPILE_ASSERT(VertexAttrib3fvImmediate::kCmdId == 1193,
                 GLES2_VertexAttrib3fvImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(VertexAttrib4f::kCmdId == 1194,
                 GLES2_VertexAttrib4f_kCmdId_mismatch);
  COMPILE_ASSERT(VertexAttrib4fv::kCmdId == 1195,
                 GLES2_VertexAttrib4fv_kCmdId_mismatch);
  COMPILE_ASSERT(VertexAttrib4fvImmediate::kCmdId == 1196,
                 GLES2_VertexAttrib4fvImmediate_kCmdId_mismatch);
  COMPILE_ASSERT(VertexAttribPointer::kCmdId == 1197,
                 GLES2_VertexAttribPointer_kCmdId_mismatch);
  COMPILE_ASSERT(Viewport::kCmdId == 1198,
                 GLES2_Viewport_kCmdId_mismatch);
  COMPILE_ASSERT(SwapBuffers::kCmdId == 1199,
                 GLES2_SwapBuffers_kCmdId_mismatch);
}
}  // namespace gles2
}  // namespace command_buffer

