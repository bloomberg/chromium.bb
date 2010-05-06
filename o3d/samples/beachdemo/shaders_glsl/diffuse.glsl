// glslv profile log:
// (70) : warning C7011: implicit cast from "float4" to "float3"
// (71) : warning C7011: implicit cast from "float4" to "float3"
// 79 lines, 2 warnings, 0 errors.

// glslf profile log:
// (70) : warning C7011: implicit cast from "float4" to "float3"
// (71) : warning C7011: implicit cast from "float4" to "float3"
// 79 lines, 2 warnings, 0 errors.

// glslv output by Cg compiler
// cgc version 2.0.0010, build date Dec 12 2007
// command line args: -profile glslv
//vendor NVIDIA Corporation
//version 2.0.0.10
//profile glslv
//program vertexShaderFunction
//semantic world : WORLD
//semantic worldViewProjection : WORLDVIEWPROJECTION
//semantic lightWorldPos
//semantic lightColor
//semantic clipHeight
//semantic viewInverse : VIEWINVERSE
//semantic worldInverseTranspose : WORLDINVERSETRANSPOSE
//semantic emissive
//semantic ambient
//semantic diffuseSampler
//semantic specular
//semantic shininess
//var float4x4 world : WORLD : _ZZ2Sworld[0], 4 : -1 : 1
//var float4x4 worldViewProjection : WORLDVIEWPROJECTION : _ZZ2SworldViewProjection[0], 4 : -1 : 1
//var float3 lightWorldPos :  :  : -1 : 0
//var float4 lightColor :  :  : -1 : 0
//var float clipHeight :  :  : -1 : 0
//var float4x4 viewInverse : VIEWINVERSE : , 4 : -1 : 0
//var float4x4 worldInverseTranspose : WORLDINVERSETRANSPOSE : _ZZ2SworldInverseTranspose[0], 4 : -1 : 1
//var float4 emissive :  :  : -1 : 0
//var float4 ambient :  :  : -1 : 0
//var sampler2D diffuseSampler :  :  : -1 : 0
//var float4 specular :  :  : -1 : 0
//var float shininess :  :  : -1 : 0
//var float4 input.position : $vin.POSITION : POSITION : 0 : 1
//var float4 input.normal : $vin.ATTR8 : $_ZZ3SZaTMP20 : 0 : 1
//var float2 input.diffuseUV : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float4 vertexShaderFunction.position : $vout.POSITION : POSITION : -1 : 1
//var float2 vertexShaderFunction.diffuseUV : $vout.TEXCOORD0 : TEXCOORD0 : -1 : 1
//var float4 vertexShaderFunction.worldPosition : $vout.TEXCOORD1 : TEXCOORD1 : -1 : 1
//var float3 vertexShaderFunction.normal : $vout.TEXCOORD2 : TEXCOORD2 : -1 : 1

attribute vec4 position;
attribute vec4 texcoord0;
vec4 _glPositionTemp;
uniform vec4 dx_clipping;

struct InVertex {
    vec4 position;
    vec4 normal;
    vec2 diffuseUV;
};

struct OutVertex {
    vec4 position;
    vec2 diffuseUV;
    vec4 worldPosition;
    vec3 normal;
};

OutVertex _ZZ3Sret_0;
vec4 _ZZ3SrZh0015;
vec4 _ZZ3SrZh0017;
vec4 _ZZ3SrZh0019;
vec4 _ZZ3SvZh0019;
attribute vec4 normal;
uniform mat4 world;
uniform mat4 worldViewProjection;
uniform mat4 worldInverseTranspose;

 // main procedure, the original name was vertexShaderFunction
void main()
{


    _ZZ3SrZh0015 = position.x*worldViewProjection[0];
    _ZZ3SrZh0015 = _ZZ3SrZh0015 + position.y*worldViewProjection[1];
    _ZZ3SrZh0015 = _ZZ3SrZh0015 + position.z*worldViewProjection[2];
    _ZZ3SrZh0015 = _ZZ3SrZh0015 + position.w*worldViewProjection[3];
    _ZZ3SrZh0017 = position.x*world[0];
    _ZZ3SrZh0017 = _ZZ3SrZh0017 + position.y*world[1];
    _ZZ3SrZh0017 = _ZZ3SrZh0017 + position.z*world[2];
    _ZZ3SrZh0017 = _ZZ3SrZh0017 + position.w*world[3];
    _ZZ3SvZh0019 = vec4(normal.x, normal.y, normal.z, 0.00000000E+00);
    _ZZ3SrZh0019 = _ZZ3SvZh0019.x*worldInverseTranspose[0];
    _ZZ3SrZh0019 = _ZZ3SrZh0019 + _ZZ3SvZh0019.y*worldInverseTranspose[1];
    _ZZ3SrZh0019 = _ZZ3SrZh0019 + _ZZ3SvZh0019.z*worldInverseTranspose[2];
    _ZZ3SrZh0019 = _ZZ3SrZh0019 + _ZZ3SvZh0019.w*worldInverseTranspose[3];
    _ZZ3SrZh0019.xyz;
    _ZZ3Sret_0.position = _ZZ3SrZh0015;
    _ZZ3Sret_0.diffuseUV = texcoord0.xy;
    _ZZ3Sret_0.worldPosition = _ZZ3SrZh0017;
    _ZZ3Sret_0.normal = _ZZ3SrZh0019.xyz;
    gl_TexCoord[0].xy = texcoord0.xy;
    gl_TexCoord[2].xyz = _ZZ3SrZh0019.xyz;
    _glPositionTemp = _ZZ3SrZh0015; gl_Position = vec4(_glPositionTemp.x + _glPositionTemp.w * dx_clipping.x, dx_clipping.w * (_glPositionTemp.y + _glPositionTemp.w * dx_clipping.y), _glPositionTemp.z * 2.0 - _glPositionTemp.w, _glPositionTemp.w);
    gl_TexCoord[1] = _ZZ3SrZh0017;
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
//semantic worldViewProjection : WORLDVIEWPROJECTION
//semantic lightWorldPos
//semantic lightColor
//semantic clipHeight
//semantic viewInverse : VIEWINVERSE
//semantic worldInverseTranspose : WORLDINVERSETRANSPOSE
//semantic emissive
//semantic ambient
//semantic diffuseSampler
//semantic specular
//semantic shininess
//var float4x4 world : WORLD : , 4 : -1 : 0
//var float4x4 worldViewProjection : WORLDVIEWPROJECTION : , 4 : -1 : 0
//var float3 lightWorldPos :  : _ZZ2SlightWorldPos : -1 : 1
//var float4 lightColor :  : _ZZ2SlightColor : -1 : 1
//var float clipHeight :  : _ZZ2SclipHeight : -1 : 1
//var float4x4 viewInverse : VIEWINVERSE : _ZZ2SviewInverse[0], 4 : -1 : 1
//var float4x4 worldInverseTranspose : WORLDINVERSETRANSPOSE : , 4 : -1 : 0
//var float4 emissive :  : _ZZ2Semissive : -1 : 1
//var float4 ambient :  : _ZZ2Sambient : -1 : 1
//var sampler2D diffuseSampler :  : _ZZ2SdiffuseSampler : -1 : 1
//var float4 specular :  : _ZZ2Sspecular : -1 : 1
//var float shininess :  : _ZZ2Sshininess : -1 : 1
//var float2 input.diffuseUV : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float4 input.worldPosition : $vin.TEXCOORD1 : TEXCOORD1 : 0 : 1
//var float3 input.normal : $vin.TEXCOORD2 : TEXCOORD2 : 0 : 1
//var float4 pixelShaderFunction : $vout.COLOR : COLOR : -1 : 1



struct InVertex {
    vec4 normal;
    vec2 diffuseUV;
};

struct OutVertex {
    vec2 diffuseUV;
    vec4 worldPosition;
    vec3 normal;
};

vec4 _ZZ3Sret_0;
vec3 _ZZ3SZDtemp16;
float _ZZ3SxZh0021;
vec3 _ZZ3SZDtemp22;
vec3 _ZZ3SvZh0023;
float _ZZ3SxZh0027;
vec4 _ZZ3SZDtemp28;
vec4 _ZZ3SvZh0029;
float _ZZ3SxZh0033;
vec3 _ZZ3SZDtemp34;
vec3 _ZZ3SvZh0035;
float _ZZ3SxZh0039;
vec4 _ZZ3StmpZh0045;
float _ZZ3SndotlZh0045;
float _ZZ3SndothZh0045;
vec4 _ZZ3SZDtemp46;
float _ZZ3SspecularZh0047;
float _ZZ3SxZh0051;
uniform vec3 lightWorldPos;
uniform vec4 lightColor;
uniform float clipHeight;
uniform mat4 viewInverse;
uniform vec4 emissive;
uniform vec4 ambient;
uniform sampler2D diffuseSampler;
uniform vec4 specular;
uniform float shininess;

 // main procedure, the original name was pixelShaderFunction
void main()
{

    vec4 _ZZ4Sdiffuse;
    float _ZZ4Salpha;
    vec3 _ZZ4SZaTMP1;

    _ZZ4Sdiffuse = texture2D(diffuseSampler, gl_TexCoord[0].xy);
    _ZZ3SxZh0021 = dot(gl_TexCoord[2].xyz, gl_TexCoord[2].xyz);
    _ZZ3SZDtemp16 = inversesqrt(_ZZ3SxZh0021)*gl_TexCoord[2].xyz;
    _ZZ3SvZh0023 = lightWorldPos - gl_TexCoord[1].xyz;
    _ZZ3SxZh0027 = dot(_ZZ3SvZh0023, _ZZ3SvZh0023);
    _ZZ3SZDtemp22 = inversesqrt(_ZZ3SxZh0027)*_ZZ3SvZh0023;
    _ZZ3SvZh0029 = viewInverse[3] - gl_TexCoord[1];
    _ZZ3SxZh0033 = dot(_ZZ3SvZh0029, _ZZ3SvZh0029);
    _ZZ3SZDtemp28 = inversesqrt(_ZZ3SxZh0033)*_ZZ3SvZh0029;
    _ZZ3SvZh0035 = _ZZ3SZDtemp22 + _ZZ3SZDtemp28.xyz;
    _ZZ3SxZh0039 = dot(_ZZ3SvZh0035, _ZZ3SvZh0035);
    _ZZ3SZDtemp34 = inversesqrt(_ZZ3SxZh0039)*_ZZ3SvZh0035;
    _ZZ3SndotlZh0045 = dot(_ZZ3SZDtemp16, _ZZ3SZDtemp22);
    _ZZ3SndothZh0045 = dot(_ZZ3SZDtemp16, _ZZ3SZDtemp34);
    _ZZ3StmpZh0045 = vec4(_ZZ3SndotlZh0045, _ZZ3SndothZh0045, shininess, shininess);
    _ZZ3SxZh0051 = max(0.00000000E+00, _ZZ3StmpZh0045.y);
    _ZZ3SspecularZh0047 = _ZZ3StmpZh0045.x > 0.00000000E+00 ? pow(_ZZ3SxZh0051, _ZZ3StmpZh0045.z) : 0.00000000E+00;
    _ZZ3SZDtemp46 = vec4(1.00000000E+00, max(0.00000000E+00, _ZZ3StmpZh0045.x), _ZZ3SspecularZh0047, 1.00000000E+00);
    _ZZ4Salpha = gl_TexCoord[1].z > clipHeight ? 0.00000000E+00 : _ZZ4Sdiffuse.w;
    _ZZ4SZaTMP1 = (emissive + lightColor*(ambient*_ZZ4Sdiffuse + _ZZ4Sdiffuse*_ZZ3SZDtemp46.y + specular*_ZZ3SZDtemp46.z)).xyz;
    _ZZ3Sret_0 = vec4(_ZZ4SZaTMP1.x, _ZZ4SZaTMP1.y, _ZZ4SZaTMP1.z, _ZZ4Salpha);
    gl_FragColor = _ZZ3Sret_0;
    return;
} // main end

