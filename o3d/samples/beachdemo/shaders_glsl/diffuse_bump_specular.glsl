// glslv profile log:
// (82) : warning C7011: implicit cast from "float4" to "float3"
// (83) : warning C7011: implicit cast from "float4" to "float3"
// 97 lines, 2 warnings, 0 errors.

// glslf profile log:
// (82) : warning C7011: implicit cast from "float4" to "float3"
// (83) : warning C7011: implicit cast from "float4" to "float3"
// 97 lines, 2 warnings, 0 errors.

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
//semantic specularSampler
//semantic bumpSampler
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
//var sampler2D specularSampler :  :  : -1 : 0
//var sampler2D bumpSampler :  :  : -1 : 0
//var float shininess :  :  : -1 : 0
//var float4 input.position : $vin.POSITION : POSITION : 0 : 1
//var float4 input.normal : $vin.ATTR8 : $_ZZ3SZaTMP29 : 0 : 1
//var float2 input.diffuseUV : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float3 input.tangent : $vin.ATTR9 : $_ZZ3SZaTMP30 : 0 : 1
//var float3 input.binormal : $vin.ATTR10 : $_ZZ3SZaTMP31 : 0 : 1
//var float4 vertexShaderFunction.position : $vout.POSITION : POSITION : -1 : 1
//var float2 vertexShaderFunction.diffuseUV : $vout.TEXCOORD0 : TEXCOORD0 : -1 : 1
//var float3 vertexShaderFunction.tangent : $vout.TEXCOORD1 : TEXCOORD1 : -1 : 1
//var float3 vertexShaderFunction.binormal : $vout.TEXCOORD2 : TEXCOORD2 : -1 : 1
//var float4 vertexShaderFunction.worldPosition : $vout.TEXCOORD3 : TEXCOORD3 : -1 : 1
//var float3 vertexShaderFunction.normal : $vout.TEXCOORD4 : TEXCOORD4 : -1 : 1

attribute vec4 position;
attribute vec4 texcoord0;
vec4 _glPositionTemp;
uniform vec4 dx_clipping;

struct InVertex {
    vec4 position;
    vec4 normal;
    vec2 diffuseUV;
    vec3 tangent;
    vec3 binormal;
};

struct OutVertex {
    vec4 position;
    vec2 diffuseUV;
    vec3 tangent;
    vec3 binormal;
    vec4 worldPosition;
    vec3 normal;
};

OutVertex _ZZ3Sret_0;
vec4 _ZZ3SrZh0020;
vec4 _ZZ3SrZh0022;
vec4 _ZZ3SrZh0024;
vec4 _ZZ3SvZh0024;
vec4 _ZZ3SrZh0026;
vec4 _ZZ3SvZh0026;
vec4 _ZZ3SrZh0028;
vec4 _ZZ3SvZh0028;
attribute vec4 normal;
attribute vec4 tangent;
attribute vec4 binormal;
uniform mat4 world;
uniform mat4 worldViewProjection;
uniform mat4 worldInverseTranspose;

 // main procedure, the original name was vertexShaderFunction
void main()
{


    _ZZ3SrZh0020 = position.x*worldViewProjection[0];
    _ZZ3SrZh0020 = _ZZ3SrZh0020 + position.y*worldViewProjection[1];
    _ZZ3SrZh0020 = _ZZ3SrZh0020 + position.z*worldViewProjection[2];
    _ZZ3SrZh0020 = _ZZ3SrZh0020 + position.w*worldViewProjection[3];
    _ZZ3SrZh0022 = position.x*world[0];
    _ZZ3SrZh0022 = _ZZ3SrZh0022 + position.y*world[1];
    _ZZ3SrZh0022 = _ZZ3SrZh0022 + position.z*world[2];
    _ZZ3SrZh0022 = _ZZ3SrZh0022 + position.w*world[3];
    _ZZ3SvZh0024 = vec4(normal.x, normal.y, normal.z, 0.00000000E+00);
    _ZZ3SrZh0024 = _ZZ3SvZh0024.x*worldInverseTranspose[0];
    _ZZ3SrZh0024 = _ZZ3SrZh0024 + _ZZ3SvZh0024.y*worldInverseTranspose[1];
    _ZZ3SrZh0024 = _ZZ3SrZh0024 + _ZZ3SvZh0024.z*worldInverseTranspose[2];
    _ZZ3SrZh0024 = _ZZ3SrZh0024 + _ZZ3SvZh0024.w*worldInverseTranspose[3];
    _ZZ3SrZh0024.xyz;
    _ZZ3SvZh0026 = vec4(binormal.x, binormal.y, binormal.z, 0.00000000E+00);
    _ZZ3SrZh0026 = _ZZ3SvZh0026.x*worldInverseTranspose[0];
    _ZZ3SrZh0026 = _ZZ3SrZh0026 + _ZZ3SvZh0026.y*worldInverseTranspose[1];
    _ZZ3SrZh0026 = _ZZ3SrZh0026 + _ZZ3SvZh0026.z*worldInverseTranspose[2];
    _ZZ3SrZh0026 = _ZZ3SrZh0026 + _ZZ3SvZh0026.w*worldInverseTranspose[3];
    _ZZ3SrZh0026.xyz;
    _ZZ3SvZh0028 = vec4(tangent.x, tangent.y, tangent.z, 0.00000000E+00);
    _ZZ3SrZh0028 = _ZZ3SvZh0028.x*worldInverseTranspose[0];
    _ZZ3SrZh0028 = _ZZ3SrZh0028 + _ZZ3SvZh0028.y*worldInverseTranspose[1];
    _ZZ3SrZh0028 = _ZZ3SrZh0028 + _ZZ3SvZh0028.z*worldInverseTranspose[2];
    _ZZ3SrZh0028 = _ZZ3SrZh0028 + _ZZ3SvZh0028.w*worldInverseTranspose[3];
    _ZZ3SrZh0028.xyz;
    _ZZ3Sret_0.position = _ZZ3SrZh0020;
    _ZZ3Sret_0.diffuseUV = texcoord0.xy;
    _ZZ3Sret_0.tangent = _ZZ3SrZh0028.xyz;
    _ZZ3Sret_0.binormal = _ZZ3SrZh0026.xyz;
    _ZZ3Sret_0.worldPosition = _ZZ3SrZh0022;
    _ZZ3Sret_0.normal = _ZZ3SrZh0024.xyz;
    gl_TexCoord[0].xy = texcoord0.xy;
    gl_TexCoord[2].xyz = _ZZ3SrZh0026.xyz;
    _glPositionTemp = _ZZ3SrZh0020; gl_Position = vec4(_glPositionTemp.x + _glPositionTemp.w * dx_clipping.x, dx_clipping.w * (_glPositionTemp.y + _glPositionTemp.w * dx_clipping.y), _glPositionTemp.z * 2.0 - _glPositionTemp.w, _glPositionTemp.w);
    gl_TexCoord[4].xyz = _ZZ3SrZh0024.xyz;
    gl_TexCoord[1].xyz = _ZZ3SrZh0028.xyz;
    gl_TexCoord[3] = _ZZ3SrZh0022;
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
//semantic specularSampler
//semantic bumpSampler
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
//var sampler2D specularSampler :  : _ZZ2SspecularSampler : -1 : 1
//var sampler2D bumpSampler :  : _ZZ2SbumpSampler : -1 : 1
//var float shininess :  : _ZZ2Sshininess : -1 : 1
//var float2 input.diffuseUV : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float3 input.tangent : $vin.TEXCOORD1 : TEXCOORD1 : 0 : 1
//var float3 input.binormal : $vin.TEXCOORD2 : TEXCOORD2 : 0 : 1
//var float4 input.worldPosition : $vin.TEXCOORD3 : TEXCOORD3 : 0 : 1
//var float3 input.normal : $vin.TEXCOORD4 : TEXCOORD4 : 0 : 1
//var float4 pixelShaderFunction : $vout.COLOR : COLOR : -1 : 1



struct InVertex {
    vec4 normal;
    vec2 diffuseUV;
    vec3 tangent;
    vec3 binormal;
};

struct OutVertex {
    vec2 diffuseUV;
    vec3 tangent;
    vec3 binormal;
    vec4 worldPosition;
    vec3 normal;
};

vec4 _ZZ3Sret_0;
vec3 _ZZ3SrZh0028;
vec3 _ZZ3SZDtemp29;
float _ZZ3SxZh0034;
vec3 _ZZ3SZDtemp35;
vec3 _ZZ3SvZh0036;
float _ZZ3SxZh0040;
vec4 _ZZ3SZDtemp41;
vec4 _ZZ3SvZh0042;
float _ZZ3SxZh0046;
vec3 _ZZ3SZDtemp47;
vec3 _ZZ3SvZh0048;
float _ZZ3SxZh0052;
vec4 _ZZ3StmpZh0058;
float _ZZ3SndotlZh0058;
float _ZZ3SndothZh0058;
vec4 _ZZ3SZDtemp59;
float _ZZ3SspecularZh0060;
float _ZZ3SxZh0064;
uniform vec3 lightWorldPos;
uniform vec4 lightColor;
uniform float clipHeight;
uniform mat4 viewInverse;
uniform vec4 emissive;
uniform vec4 ambient;
uniform sampler2D diffuseSampler;
uniform sampler2D specularSampler;
uniform sampler2D bumpSampler;
uniform float shininess;

 // main procedure, the original name was pixelShaderFunction
void main()
{

    vec4 _ZZ4Sdiffuse;
    vec4 _ZZ4Sspecular;
    vec3 _ZZ4StangentNormal;
    float _ZZ4Salpha;
    vec3 _ZZ4SZaTMP4;

    _ZZ4Sdiffuse = texture2D(diffuseSampler, gl_TexCoord[0].xy);
    _ZZ4Sspecular = texture2D(specularSampler, gl_TexCoord[0].xy);
    _ZZ4StangentNormal = texture2D(bumpSampler, gl_TexCoord[0].xy).xyz - vec3( 5.00000000E-01, 5.00000000E-01, 5.00000000E-01);
    _ZZ3SrZh0028 = _ZZ4StangentNormal.x*gl_TexCoord[1].xyz;
    _ZZ3SrZh0028 = _ZZ3SrZh0028 + _ZZ4StangentNormal.y*gl_TexCoord[2].xyz;
    _ZZ3SrZh0028 = _ZZ3SrZh0028 + _ZZ4StangentNormal.z*gl_TexCoord[4].xyz;
    _ZZ3SxZh0034 = dot(_ZZ3SrZh0028, _ZZ3SrZh0028);
    _ZZ3SZDtemp29 = inversesqrt(_ZZ3SxZh0034)*_ZZ3SrZh0028;
    _ZZ3SvZh0036 = lightWorldPos - gl_TexCoord[3].xyz;
    _ZZ3SxZh0040 = dot(_ZZ3SvZh0036, _ZZ3SvZh0036);
    _ZZ3SZDtemp35 = inversesqrt(_ZZ3SxZh0040)*_ZZ3SvZh0036;
    _ZZ3SvZh0042 = viewInverse[3] - gl_TexCoord[3];
    _ZZ3SxZh0046 = dot(_ZZ3SvZh0042, _ZZ3SvZh0042);
    _ZZ3SZDtemp41 = inversesqrt(_ZZ3SxZh0046)*_ZZ3SvZh0042;
    _ZZ3SvZh0048 = _ZZ3SZDtemp35 + _ZZ3SZDtemp41.xyz;
    _ZZ3SxZh0052 = dot(_ZZ3SvZh0048, _ZZ3SvZh0048);
    _ZZ3SZDtemp47 = inversesqrt(_ZZ3SxZh0052)*_ZZ3SvZh0048;
    _ZZ3SndotlZh0058 = dot(_ZZ3SZDtemp29, _ZZ3SZDtemp35);
    _ZZ3SndothZh0058 = dot(_ZZ3SZDtemp29, _ZZ3SZDtemp47);
    _ZZ3StmpZh0058 = vec4(_ZZ3SndotlZh0058, _ZZ3SndothZh0058, shininess, shininess);
    _ZZ3SxZh0064 = max(0.00000000E+00, _ZZ3StmpZh0058.y);
    _ZZ3SspecularZh0060 = _ZZ3StmpZh0058.x > 0.00000000E+00 ? pow(_ZZ3SxZh0064, _ZZ3StmpZh0058.z) : 0.00000000E+00;
    _ZZ3SZDtemp59 = vec4(1.00000000E+00, max(0.00000000E+00, _ZZ3StmpZh0058.x), _ZZ3SspecularZh0060, 1.00000000E+00);
    _ZZ4Salpha = gl_TexCoord[3].z > clipHeight ? 0.00000000E+00 : _ZZ4Sdiffuse.w;
    _ZZ4SZaTMP4 = (emissive + lightColor*(ambient*_ZZ4Sdiffuse + _ZZ4Sdiffuse*_ZZ3SZDtemp59.y + _ZZ4Sspecular*_ZZ3SZDtemp59.z)).xyz;
    _ZZ3Sret_0 = vec4(_ZZ4SZaTMP4.x, _ZZ4SZaTMP4.y, _ZZ4SZaTMP4.z, _ZZ4Salpha);
    gl_FragColor = _ZZ3Sret_0;
    return;
} // main end

