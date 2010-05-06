// glslv profile log:
// (64) : warning C7011: implicit cast from "float4" to "float3"
// (64) : warning C7011: implicit cast from "float4" to "float3"
// 80 lines, 2 warnings, 0 errors.

// glslf profile log:
// 80 lines, 0 errors.

// glslv output by Cg compiler
// cgc version 2.0.0010, build date Dec 12 2007
// command line args: -profile glslv
//vendor NVIDIA Corporation
//version 2.0.0.10
//profile glslv
//program vertexShaderFunction
//semantic world : WORLD
//semantic viewProjection : VIEWPROJECTION
//semantic waterColor
//semantic sunVector
//semantic fadeFudge
//semantic diffuseSampler
//var float4x4 world : WORLD : _ZZ2Sworld[0], 4 : -1 : 1
//var float4x4 viewProjection : VIEWPROJECTION : _ZZ2SviewProjection[0], 4 : -1 : 1
//var float4 waterColor :  :  : -1 : 0
//var float3 sunVector :  : _ZZ2SsunVector : -1 : 1
//var float fadeFudge :  : _ZZ2SfadeFudge : -1 : 1
//var sampler diffuseSampler :  :  : -1 : 0
//var float4 input.position : $vin.POSITION : POSITION : 0 : 1
//var float3 input.normal : $vin.ATTR8 : $_ZZ3SZaTMP32 : 0 : 1
//var float2 input.texcoord : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float4 vertexShaderFunction.position : $vout.POSITION : POSITION : -1 : 1
//var float2 vertexShaderFunction.texcoord : $vout.TEXCOORD0 : TEXCOORD0 : -1 : 1
//var float vertexShaderFunction.fade : $vout.TEXCOORD1 : TEXCOORD1 : -1 : 1
//var float4 vertexShaderFunction.color : $vout.TEXCOORD2 : TEXCOORD2 : -1 : 1
//var float4 vertexShaderFunction.worldPosition : $vout.TEXCOORD3 : TEXCOORD3 : -1 : 1

attribute vec4 position;
attribute vec4 texcoord0;
vec4 _glPositionTemp;
uniform vec4 dx_clipping;

struct VertexShaderInput {
    vec4 position;
    vec3 normal;
    vec2 texcoord;
};

struct PixelShaderInput {
    vec4 position;
    vec2 texcoord;
    float fade;
    vec4 color;
    vec4 worldPosition;
};

PixelShaderInput _ZZ3Sret_0;
vec4 _ZZ3SrZh0011;
vec4 _ZZ3SrZh0013;
vec4 _ZZ3SvZh0013;
vec4 _ZZ3SrZh0015;
vec4 _ZZ3SvZh0015;
vec4 _ZZ3SZDtemp16;
float _ZZ3SxZh0023;
float _ZZ3SxZh0025;
float _ZZ3SZDtemp26;
float _ZZ3SbZh0031;
attribute vec4 normal;
uniform mat4 world;
uniform mat4 viewprojection;
uniform vec3 sunVector;
uniform float fadeFudge;

 // main procedure, the original name was vertexShaderFunction
void main()
{

    PixelShaderInput _ZZ4Soutput;

    _ZZ3SrZh0011 = position.x*world[0];
    _ZZ3SrZh0011 = _ZZ3SrZh0011 + position.y*world[1];
    _ZZ3SrZh0011 = _ZZ3SrZh0011 + position.z*world[2];
    _ZZ3SrZh0011 = _ZZ3SrZh0011 + position.w*world[3];
    _ZZ3SvZh0013 = vec4(_ZZ3SrZh0011.x, _ZZ3SrZh0011.y, _ZZ3SrZh0011.z, 1.00000000E+00);
    _ZZ3SrZh0013 = _ZZ3SvZh0013.x*viewprojection[0];
    _ZZ3SrZh0013 = _ZZ3SrZh0013 + _ZZ3SvZh0013.y*viewprojection[1];
    _ZZ3SrZh0013 = _ZZ3SrZh0013 + _ZZ3SvZh0013.z*viewprojection[2];
    _ZZ3SrZh0013 = _ZZ3SrZh0013 + _ZZ3SvZh0013.w*viewprojection[3];
    _ZZ3SvZh0015 = vec4(normal.x, normal.y, normal.z, 0.00000000E+00);
    _ZZ3SrZh0015 = _ZZ3SvZh0015.x*world[0];
    _ZZ3SrZh0015 = _ZZ3SrZh0015 + _ZZ3SvZh0015.y*world[1];
    _ZZ3SrZh0015 = _ZZ3SrZh0015 + _ZZ3SvZh0015.z*world[2];
    _ZZ3SrZh0015 = _ZZ3SrZh0015 + _ZZ3SvZh0015.w*world[3];
    _ZZ3SxZh0023 = dot(_ZZ3SrZh0015, _ZZ3SrZh0015);
    _ZZ3SZDtemp16 = inversesqrt(_ZZ3SxZh0023)*_ZZ3SrZh0015;
    _ZZ4Soutput.color = vec4(dot(sunVector, _ZZ3SZDtemp16.xyz), dot(sunVector, _ZZ3SZDtemp16.xyz), dot(sunVector, _ZZ3SZDtemp16.xyz), dot(sunVector, _ZZ3SZDtemp16.xyz));
    _ZZ3SxZh0025 = _ZZ3SrZh0011.z*fadeFudge;
    _ZZ3SbZh0031 = min(1.00000000E+00, _ZZ3SxZh0025);
    _ZZ3SZDtemp26 = max(0.00000000E+00, _ZZ3SbZh0031);
    _ZZ4Soutput.fade = 2.00000003E-01 + 8.00000012E-01*_ZZ3SZDtemp26;
    _ZZ3Sret_0.position = _ZZ3SrZh0013;
    _ZZ3Sret_0.texcoord = texcoord0.xy;
    _ZZ3Sret_0.fade = _ZZ4Soutput.fade;
    _ZZ3Sret_0.color = _ZZ4Soutput.color;
    _ZZ3Sret_0.worldPosition = _ZZ3SrZh0011;
    gl_TexCoord[0].xy = texcoord0.xy;
    gl_TexCoord[2] = _ZZ4Soutput.color;
    _glPositionTemp = _ZZ3SrZh0013; gl_Position = vec4(_glPositionTemp.x + _glPositionTemp.w * dx_clipping.x, dx_clipping.w * (_glPositionTemp.y + _glPositionTemp.w * dx_clipping.y), _glPositionTemp.z * 2.0 - _glPositionTemp.w, _glPositionTemp.w);
    gl_TexCoord[1].x = _ZZ4Soutput.fade;
    gl_TexCoord[3] = _ZZ3SrZh0011;
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
//semantic world : WORLD
//semantic viewProjection : VIEWPROJECTION
//semantic waterColor
//semantic sunVector
//semantic fadeFudge
//semantic diffuseSampler
//var float4x4 world : WORLD : , 4 : -1 : 0
//var float4x4 viewProjection : VIEWPROJECTION : , 4 : -1 : 0
//var float4 waterColor :  :  : -1 : 0
//var float3 sunVector :  :  : -1 : 0
//var float fadeFudge :  :  : -1 : 0
//var sampler diffuseSampler :  : _ZZ2SdiffuseSampler : -1 : 1
//var float2 input.texcoord : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float input.fade : $vin.TEXCOORD1 :  : 0 : 0
//var float4 input.color : $vin.TEXCOORD2 :  : 0 : 0
//var float4 input.worldPosition : $vin.TEXCOORD3 : TEXCOORD3 : 0 : 1
//var float4 pixelShaderFunction : $vout.COLOR : COLOR : -1 : 1



struct VertexShaderInput {
    vec3 normal;
    vec2 texcoord;
};

struct PixelShaderInput {
    vec2 texcoord;
    float fade;
    vec4 color;
    vec4 worldPosition;
};

vec4 _ZZ3Sret_0;
sampler2D _ZZ3SsZh0009;
uniform sampler diffuseSampler;

 // main procedure, the original name was pixelShaderFunction
void main()
{

    vec4 _ZZ4Scolor;
    float _ZZ4Salpha;

    _ZZ3SsZh0009 = sampler2D(diffuseSampler);
    _ZZ4Scolor = texture2D(_ZZ3SsZh0009, gl_TexCoord[0].xy);
    _ZZ4Salpha = gl_TexCoord[3].z < 1.00000000E+02 ? _ZZ4Scolor.w : 0.00000000E+00;
    _ZZ3Sret_0 = vec4(_ZZ4Scolor.x, _ZZ4Scolor.y, _ZZ4Scolor.z, _ZZ4Salpha);
    gl_FragColor = _ZZ3Sret_0;
    return;
} // main end

