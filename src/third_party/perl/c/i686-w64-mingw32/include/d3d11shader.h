#undef INTERFACE
/*
 * Copyright 2010 Matteo Bruni for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __D3D11SHADER_H__
#define __D3D11SHADER_H__

#include "d3dcommon.h"

/* These are defined as version-neutral in d3dcommon.h */
typedef D3D_CBUFFER_TYPE D3D11_CBUFFER_TYPE;

typedef D3D_RESOURCE_RETURN_TYPE D3D11_RESOURCE_RETURN_TYPE;

typedef D3D_TESSELLATOR_DOMAIN D3D11_TESSELLATOR_DOMAIN;

typedef D3D_TESSELLATOR_PARTITIONING D3D11_TESSELLATOR_PARTITIONING;

typedef D3D_TESSELLATOR_OUTPUT_PRIMITIVE D3D11_TESSELLATOR_OUTPUT_PRIMITIVE;

typedef struct _D3D11_SHADER_DESC
{
    UINT Version;
    LPCSTR Creator;
    UINT Flags;
    UINT ConstantBuffers;
    UINT BoundResources;
    UINT InputParameters;
    UINT OutputParameters;
    UINT InstructionCount;
    UINT TempRegisterCount;
    UINT TempArrayCount;
    UINT DefCount;
    UINT DclCount;
    UINT TextureNormalInstructions;
    UINT TextureLoadInstructions;
    UINT TextureCompInstructions;
    UINT TextureBiasInstructions;
    UINT TextureGradientInstructions;
    UINT FloatInstructionCount;
    UINT IntInstructionCount;
    UINT UintInstructionCount;
    UINT StaticFlowControlCount;
    UINT DynamicFlowControlCount;
    UINT MacroInstructionCount;
    UINT ArrayInstructionCount;
    UINT CutInstructionCount;
    UINT EmitInstructionCount;
    D3D_PRIMITIVE_TOPOLOGY GSOutputTopology;
    UINT GSMaxOutputVertexCount;
    D3D_PRIMITIVE InputPrimitive;
    UINT PatchConstantParameters;
    UINT cGSInstanceCount;
    UINT cControlPoints;
    D3D_TESSELLATOR_OUTPUT_PRIMITIVE HSOutputPrimitive;
    D3D_TESSELLATOR_PARTITIONING HSPartitioning;
    D3D_TESSELLATOR_DOMAIN TessellatorDomain;
    UINT cBarrierInstructions;
    UINT cInterlockedInstructions;
    UINT cTextureStoreInstructions;
} D3D11_SHADER_DESC;

typedef struct _D3D11_SHADER_VARIABLE_DESC
{
    LPCSTR Name;
    UINT StartOffset;
    UINT Size;
    UINT uFlags;
    LPVOID DefaultValue;
    UINT StartTexture;
    UINT TextureSize;
    UINT StartSampler;
    UINT SamplerSize;
} D3D11_SHADER_VARIABLE_DESC;

typedef struct _D3D11_SHADER_TYPE_DESC
{
    D3D_SHADER_VARIABLE_CLASS Class;
    D3D_SHADER_VARIABLE_TYPE Type;
    UINT Rows;
    UINT Columns;
    UINT Elements;
    UINT Members;
    UINT Offset;
    LPCSTR Name;
} D3D11_SHADER_TYPE_DESC;

typedef struct _D3D11_SHADER_BUFFER_DESC
{
    LPCSTR Name;
    D3D_CBUFFER_TYPE Type;
    UINT Variables;
    UINT Size;
    UINT uFlags;
} D3D11_SHADER_BUFFER_DESC;

typedef struct _D3D11_SHADER_INPUT_BIND_DESC
{
    LPCSTR Name;
    D3D_SHADER_INPUT_TYPE Type;
    UINT BindPoint;
    UINT BindCount;
    UINT uFlags;
    D3D_RESOURCE_RETURN_TYPE ReturnType;
    D3D_SRV_DIMENSION Dimension;
    UINT NumSamples;
} D3D11_SHADER_INPUT_BIND_DESC;

typedef struct _D3D11_SIGNATURE_PARAMETER_DESC
{
    LPCSTR SemanticName;
    UINT SemanticIndex;
    UINT Register;
    D3D_NAME SystemValueType;
    D3D_REGISTER_COMPONENT_TYPE ComponentType;
    BYTE Mask;
    BYTE ReadWriteMask;
    UINT Stream;
} D3D11_SIGNATURE_PARAMETER_DESC;

DEFINE_GUID(IID_ID3D11ShaderReflectionType, 0x6e6ffa6a, 0x9bae, 0x4613, 0xa5, 0x1e, 0x91, 0x65, 0x2d, 0x50, 0x8c, 0x21);

#define INTERFACE ID3D11ShaderReflectionType
DECLARE_INTERFACE(ID3D11ShaderReflectionType)
{
    STDMETHOD(GetDesc)(THIS_ D3D11_SHADER_TYPE_DESC *desc) PURE;
    STDMETHOD_(struct ID3D11ShaderReflectionType *, GetMemberTypeByIndex)(THIS_ UINT index) PURE;
    STDMETHOD_(struct ID3D11ShaderReflectionType *, GetMemberTypeByName)(THIS_ LPCSTR name) PURE;
    STDMETHOD_(LPCSTR, GetMemberTypeName)(THIS_ UINT index) PURE;
    STDMETHOD(IsEqual)(THIS_ struct ID3D11ShaderReflectionType *type) PURE;
    STDMETHOD_(struct ID3D11ShaderReflectionType *, GetSubType)(THIS) PURE;
    STDMETHOD_(struct ID3D11ShaderReflectionType *, GetBaseClass)(THIS) PURE;
    STDMETHOD_(UINT, GetNumInterfaces)(THIS) PURE;
    STDMETHOD_(struct ID3D11ShaderReflectionType *, GetInterfaceByIndex)(THIS_ UINT index) PURE;
    STDMETHOD(IsOfType)(THIS_ struct ID3D11ShaderReflectionType *type) PURE;
    STDMETHOD(ImplementsInterface)(THIS_ ID3D11ShaderReflectionType *base) PURE;
};
#undef INTERFACE

DEFINE_GUID(IID_ID3D11ShaderReflectionVariable, 0x51f23923, 0xf3e5, 0x4bd1, 0x91, 0xcb, 0x60, 0x61, 0x77, 0xd8, 0xdb, 0x4c);

#define INTERFACE ID3D11ShaderReflectionVariable
DECLARE_INTERFACE(ID3D11ShaderReflectionVariable)
{
    STDMETHOD(GetDesc)(THIS_ D3D11_SHADER_VARIABLE_DESC *desc) PURE;
    STDMETHOD_(struct ID3D11ShaderReflectionType *, GetType)(THIS) PURE;
    STDMETHOD_(struct ID3D11ShaderReflectionConstantBuffer *, GetBuffer)(THIS) PURE;
    STDMETHOD_(UINT, GetInterfaceSlot)(THIS_ UINT index) PURE;
};
#undef INTERFACE

DEFINE_GUID(IID_ID3D11ShaderReflectionConstantBuffer, 0xeb62d63d, 0x93dd, 0x4318, 0x8a, 0xe8, 0xc6, 0xf8, 0x3a, 0xd3, 0x71, 0xb8);

#define INTERFACE ID3D11ShaderReflectionConstantBuffer
DECLARE_INTERFACE(ID3D11ShaderReflectionConstantBuffer)
{
    STDMETHOD(GetDesc)(THIS_ D3D11_SHADER_BUFFER_DESC *desc) PURE;
    STDMETHOD_(struct ID3D11ShaderReflectionVariable *, GetVariableByIndex)(THIS_ UINT index) PURE;
    STDMETHOD_(struct ID3D11ShaderReflectionVariable *, GetVariableByName)(THIS_ LPCSTR name) PURE;
};
#undef INTERFACE

DEFINE_GUID(IID_ID3D11ShaderReflection, 0x0a233719, 0x3960, 0x4578, 0x9d, 0x7c, 0x20, 0x3b, 0x8b, 0x1d, 0x9c, 0xc1);

#define INTERFACE ID3D11ShaderReflection
DECLARE_INTERFACE_(ID3D11ShaderReflection, IUnknown)
{
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID *object) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    /* ID3D11ShaderReflection methods */
    STDMETHOD(GetDesc)(THIS_ D3D11_SHADER_DESC *desc) PURE;
    STDMETHOD_(struct ID3D11ShaderReflectionConstantBuffer *, GetConstantBufferByIndex)(THIS_ UINT index) PURE;
    STDMETHOD_(struct ID3D11ShaderReflectionConstantBuffer *, GetConstantBufferByName)(THIS_ LPCSTR name) PURE;
    STDMETHOD(GetResourceBindingDesc)(THIS_ UINT index, D3D11_SHADER_INPUT_BIND_DESC *desc) PURE;
    STDMETHOD(GetInputParameterDesc)(THIS_ UINT index, D3D11_SIGNATURE_PARAMETER_DESC *desc) PURE;
    STDMETHOD(GetOutputParameterDesc)(THIS_ UINT index, D3D11_SIGNATURE_PARAMETER_DESC *desc) PURE;
    STDMETHOD(GetPatchConstantParameterDesc)(THIS_ UINT index, D3D11_SIGNATURE_PARAMETER_DESC *desc) PURE;
    STDMETHOD_(struct ID3D11ShaderReflectionVariable *, GetVariableByName)(THIS_ LPCSTR name) PURE;
    STDMETHOD(GetResourceBindingDescByName)(THIS_ LPCSTR name, D3D11_SHADER_INPUT_BIND_DESC *desc) PURE;
    STDMETHOD_(UINT, GetMovInstructionCount)(THIS) PURE;
    STDMETHOD_(UINT, GetMovcInstructionCount)(THIS) PURE;
    STDMETHOD_(UINT, GetConversionInstructionCount)(THIS) PURE;
    STDMETHOD_(UINT, GetBitwiseInstructionCount)(THIS) PURE;
    STDMETHOD_(D3D_PRIMITIVE, GetGSInputPrimitive)(THIS) PURE;
    STDMETHOD_(WINBOOL, IsSampleFrequencyShader)(THIS) PURE;
    STDMETHOD_(UINT, GetNumInterfaceSlots)(THIS) PURE;
    STDMETHOD(GetMinFeatureLevel)(THIS_ enum D3D_FEATURE_LEVEL *level) PURE;
    STDMETHOD_(UINT, GetThreadGroupSize)(THIS_ UINT *sizex, UINT *sizey, UINT *sizez) PURE;
};
#undef INTERFACE

#endif
