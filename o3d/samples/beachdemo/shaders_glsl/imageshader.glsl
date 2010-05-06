// glslv profile log:
// 61 lines, 0 errors.

// glslf profile log:
// 61 lines, 0 errors.

// glslv output by Cg compiler
// cgc version 2.0.0010, build date Dec 12 2007
// command line args: -profile glslv
//vendor NVIDIA Corporation
//version 2.0.0.10
//profile glslv
//program vertexShaderFunction
//semantic worldViewProjection : WORLDVIEWPROJECTION
//semantic colorMult
//semantic diffuseSampler
//var float4x4 worldViewProjection : WORLDVIEWPROJECTION : _ZZ2SworldViewProjection[0], 4 : -1 : 1
//var float4 colorMult :  :  : -1 : 0
//var sampler diffuseSampler :  :  : -1 : 0
//var float4 input.position : $vin.POSITION : POSITION : 0 : 1
//var float2 input.texcoord : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float4 vertexShaderFunction.position : $vout.POSITION : POSITION : -1 : 1
//var float2 vertexShaderFunction.texcoord : $vout.TEXCOORD2 : TEXCOORD2 : -1 : 1

attribute vec4 position;
attribute vec4 texcoord0;
vec4 _glPositionTemp;
uniform vec4 dx_clipping;

struct VertexShaderInput {
    vec4 position;
    vec2 texcoord;
};

struct PixelShaderInput {
    vec4 position;
    vec2 texcoord;
};

PixelShaderInput _ZZ3Sret_0;
vec4 _ZZ3SrZh0004;
uniform mat4 worldViewProjection;

 // main procedure, the original name was vertexShaderFunction
void main()
{


    _ZZ3SrZh0004 = position.x*worldViewProjection[0];
    _ZZ3SrZh0004 = _ZZ3SrZh0004 + position.y*worldViewProjection[1];
    _ZZ3SrZh0004 = _ZZ3SrZh0004 + position.z*worldViewProjection[2];
    _ZZ3SrZh0004 = _ZZ3SrZh0004 + position.w*worldViewProjection[3];
    _ZZ3Sret_0.position = _ZZ3SrZh0004;
    _ZZ3Sret_0.texcoord = texcoord0.xy;
    gl_TexCoord[2].xy = texcoord0.xy;
    _glPositionTemp = _ZZ3SrZh0004; gl_Position = vec4(_glPositionTemp.x + _glPositionTemp.w * dx_clipping.x, dx_clipping.w * (_glPositionTemp.y + _glPositionTemp.w * dx_clipping.y), _glPositionTemp.z * 2.0 - _glPositionTemp.w, _glPositionTemp.w);
    return;
} // main end


// #o3d SplitMarker
// #o3d MatrixLoadOrder RowMajor

// glslf output by Cg compiler
// cgc version 2.0.0010, build date Dec 12 2007
// command line args: -profile glslf
//vendor NVIDIA Corporation
//version 2.0.0.10
//profile glslf
//program pixelShaderFunction
//semantic worldViewProjection : WORLDVIEWPROJECTION
//semantic colorMult
//semantic diffuseSampler
//var float4x4 worldViewProjection : WORLDVIEWPROJECTION : , 4 : -1 : 0
//var float4 colorMult :  : _ZZ2ScolorMult : -1 : 1
//var sampler diffuseSampler :  : _ZZ2SdiffuseSampler : -1 : 1
//var float2 input.texcoord : $vin.TEXCOORD2 : TEXCOORD2 : 0 : 1
//var float4 pixelShaderFunction : $vout.COLOR : COLOR : -1 : 1



struct VertexShaderInput {
    vec2 texcoord;
};

struct PixelShaderInput {
    vec2 texcoord;
};

vec4 _ZZ3Sret_0;
sampler2D _ZZ3SsZh0004;
uniform vec4 colorMult;
uniform sampler diffuseSampler;

 // main procedure, the original name was pixelShaderFunction
void main()
{


    _ZZ3SsZh0004 = sampler2D(diffuseSampler);
    _ZZ3Sret_0 = texture2D(_ZZ3SsZh0004, gl_TexCoord[2].xy)*colorMult;
    gl_FragColor = _ZZ3Sret_0;
    return;
} // main end

