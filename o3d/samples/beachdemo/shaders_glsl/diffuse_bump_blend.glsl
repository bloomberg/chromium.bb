// glslv profile log:
// (100) : warning C7011: implicit cast from "float4" to "float3"
// (101) : warning C7011: implicit cast from "float4" to "float3"
// (72) : warning C7050: "output.color" might be used before being initialized
// 116 lines, 3 warnings, 0 errors.

// glslf profile log:
// (100) : warning C7011: implicit cast from "float4" to "float3"
// (101) : warning C7011: implicit cast from "float4" to "float3"
// 116 lines, 2 warnings, 0 errors.

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
//semantic diffuse2Sampler
//semantic specular
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
//var sampler2D diffuse2Sampler :  :  : -1 : 0
//var float4 specular :  :  : -1 : 0
//var sampler2D bumpSampler :  :  : -1 : 0
//var float shininess :  :  : -1 : 0
//var float4 input.position : $vin.POSITION : POSITION : 0 : 1
//var float4 input.normal : $vin.ATTR8 : $_ZZ3SZaTMP42 : 0 : 1
//var float2 input.diffuseUV : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float2 input.diffuse2UV : $vin.TEXCOORD1 : TEXCOORD1 : 0 : 1
//var float3 input.tangent : $vin.ATTR9 : $_ZZ3SZaTMP43 : 0 : 1
//var float3 input.tangent2 : $vin.ATTR11 : $_ZZ3SZaTMP44 : 0 : 1
//var float3 input.binormal : $vin.ATTR10 : $_ZZ3SZaTMP45 : 0 : 1
//var float3 input.binormal2 : $vin.ATTR12 : $_ZZ3SZaTMP46 : 0 : 1
//var float4 input.color : $vin.COLOR :  : 0 : 0
//var float4 vertexShaderFunction.position : $vout.POSITION : POSITION : -1 : 1
//var float4 vertexShaderFunction.diffuseUV : $vout.TEXCOORD0 : TEXCOORD0 : -1 : 1
//var float3 vertexShaderFunction.tangent : $vout.TEXCOORD1 : TEXCOORD1 : -1 : 1
//var float3 vertexShaderFunction.tangent2 : $vout.TEXCOORD2 : TEXCOORD2 : -1 : 1
//var float3 vertexShaderFunction.binormal : $vout.TEXCOORD3 : TEXCOORD3 : -1 : 1
//var float3 vertexShaderFunction.binormal2 : $vout.TEXCOORD4 : TEXCOORD4 : -1 : 1
//var float4 vertexShaderFunction.worldPosition : $vout.TEXCOORD5 : TEXCOORD5 : -1 : 1
//var float3 vertexShaderFunction.normal : $vout.TEXCOORD6 : TEXCOORD6 : -1 : 1
//var float4 vertexShaderFunction.color : $vout.COLOR : COLOR : -1 : 1

attribute vec4 position;
attribute vec4 texcoord0;
attribute vec4 texcoord1;
vec4 _glPositionTemp;
uniform vec4 dx_clipping;

struct InVertex {
    vec4 position;
    vec4 normal;
    vec2 diffuseUV;
    vec2 diffuse2UV;
    vec3 tangent;
    vec3 tangent2;
    vec3 binormal;
    vec3 binormal2;
    vec4 color;
};

struct OutVertex {
    vec4 position;
    vec4 diffuseUV;
    vec3 tangent;
    vec3 tangent2;
    vec3 binormal;
    vec3 binormal2;
    vec4 worldPosition;
    vec3 normal;
    vec4 color;
};

OutVertex _ZZ3Sret_0;
vec4 _ZZ3SrZh0029;
vec4 _ZZ3SrZh0031;
vec4 _ZZ3SrZh0033;
vec4 _ZZ3SvZh0033;
vec4 _ZZ3SrZh0035;
vec4 _ZZ3SvZh0035;
vec4 _ZZ3SrZh0037;
vec4 _ZZ3SvZh0037;
vec4 _ZZ3SrZh0039;
vec4 _ZZ3SvZh0039;
vec4 _ZZ3SrZh0041;
vec4 _ZZ3SvZh0041;
attribute vec4 normal;
attribute vec4 tangent;
attribute vec4 tangent1;
attribute vec4 binormal;
attribute vec4 binormal1;
uniform mat4 world;
uniform mat4 worldViewProjection;
uniform mat4 worldInverseTranspose;

 // main procedure, the original name was vertexShaderFunction
void main()
{

    OutVertex _ZZ4Soutput;

    _ZZ3SrZh0029 = position.x*worldViewProjection[0];
    _ZZ3SrZh0029 = _ZZ3SrZh0029 + position.y*worldViewProjection[1];
    _ZZ3SrZh0029 = _ZZ3SrZh0029 + position.z*worldViewProjection[2];
    _ZZ3SrZh0029 = _ZZ3SrZh0029 + position.w*worldViewProjection[3];
    _ZZ3SrZh0031 = position.x*world[0];
    _ZZ3SrZh0031 = _ZZ3SrZh0031 + position.y*world[1];
    _ZZ3SrZh0031 = _ZZ3SrZh0031 + position.z*world[2];
    _ZZ3SrZh0031 = _ZZ3SrZh0031 + position.w*world[3];
    _ZZ4Soutput.diffuseUV = vec4(texcoord0.x, texcoord0.y, texcoord1.x, texcoord1.y);
    _ZZ3SvZh0033 = vec4(normal.x, normal.y, normal.z, 0.00000000E+00);
    _ZZ3SrZh0033 = _ZZ3SvZh0033.x*worldInverseTranspose[0];
    _ZZ3SrZh0033 = _ZZ3SrZh0033 + _ZZ3SvZh0033.y*worldInverseTranspose[1];
    _ZZ3SrZh0033 = _ZZ3SrZh0033 + _ZZ3SvZh0033.z*worldInverseTranspose[2];
    _ZZ3SrZh0033 = _ZZ3SrZh0033 + _ZZ3SvZh0033.w*worldInverseTranspose[3];
    _ZZ3SrZh0033.xyz;
    _ZZ3SvZh0035 = vec4(binormal.x, binormal.y, binormal.z, 0.00000000E+00);
    _ZZ3SrZh0035 = _ZZ3SvZh0035.x*worldInverseTranspose[0];
    _ZZ3SrZh0035 = _ZZ3SrZh0035 + _ZZ3SvZh0035.y*worldInverseTranspose[1];
    _ZZ3SrZh0035 = _ZZ3SrZh0035 + _ZZ3SvZh0035.z*worldInverseTranspose[2];
    _ZZ3SrZh0035 = _ZZ3SrZh0035 + _ZZ3SvZh0035.w*worldInverseTranspose[3];
    _ZZ3SrZh0035.xyz;
    _ZZ3SvZh0037 = vec4(tangent.x, tangent.y, tangent.z, 0.00000000E+00);
    _ZZ3SrZh0037 = _ZZ3SvZh0037.x*worldInverseTranspose[0];
    _ZZ3SrZh0037 = _ZZ3SrZh0037 + _ZZ3SvZh0037.y*worldInverseTranspose[1];
    _ZZ3SrZh0037 = _ZZ3SrZh0037 + _ZZ3SvZh0037.z*worldInverseTranspose[2];
    _ZZ3SrZh0037 = _ZZ3SrZh0037 + _ZZ3SvZh0037.w*worldInverseTranspose[3];
    _ZZ3SrZh0037.xyz;
    _ZZ3SvZh0039 = vec4(binormal1.x, binormal1.y, binormal1.z, 0.00000000E+00);
    _ZZ3SrZh0039 = _ZZ3SvZh0039.x*worldInverseTranspose[0];
    _ZZ3SrZh0039 = _ZZ3SrZh0039 + _ZZ3SvZh0039.y*worldInverseTranspose[1];
    _ZZ3SrZh0039 = _ZZ3SrZh0039 + _ZZ3SvZh0039.z*worldInverseTranspose[2];
    _ZZ3SrZh0039 = _ZZ3SrZh0039 + _ZZ3SvZh0039.w*worldInverseTranspose[3];
    _ZZ3SrZh0039.xyz;
    _ZZ3SvZh0041 = vec4(tangent1.x, tangent1.y, tangent1.z, 0.00000000E+00);
    _ZZ3SrZh0041 = _ZZ3SvZh0041.x*worldInverseTranspose[0];
    _ZZ3SrZh0041 = _ZZ3SrZh0041 + _ZZ3SvZh0041.y*worldInverseTranspose[1];
    _ZZ3SrZh0041 = _ZZ3SrZh0041 + _ZZ3SvZh0041.z*worldInverseTranspose[2];
    _ZZ3SrZh0041 = _ZZ3SrZh0041 + _ZZ3SvZh0041.w*worldInverseTranspose[3];
    _ZZ3SrZh0041.xyz;
    _ZZ3Sret_0.position = _ZZ3SrZh0029;
    _ZZ3Sret_0.diffuseUV = _ZZ4Soutput.diffuseUV;
    _ZZ3Sret_0.tangent = _ZZ3SrZh0037.xyz;
    _ZZ3Sret_0.tangent2 = _ZZ3SrZh0041.xyz;
    _ZZ3Sret_0.binormal = _ZZ3SrZh0035.xyz;
    _ZZ3Sret_0.binormal2 = _ZZ3SrZh0039.xyz;
    _ZZ3Sret_0.worldPosition = _ZZ3SrZh0031;
    _ZZ3Sret_0.normal = _ZZ3SrZh0033.xyz;
    _ZZ3Sret_0.color = _ZZ4Soutput.color;
    gl_FrontColor = _ZZ4Soutput.color;
    gl_TexCoord[0] = _ZZ4Soutput.diffuseUV;
    gl_TexCoord[6].xyz = _ZZ3SrZh0033.xyz;
    gl_TexCoord[2].xyz = _ZZ3SrZh0041.xyz;
    gl_TexCoord[5] = _ZZ3SrZh0031;
    _glPositionTemp = _ZZ3SrZh0029; gl_Position = vec4(_glPositionTemp.x + _glPositionTemp.w * dx_clipping.x, dx_clipping.w * (_glPositionTemp.y + _glPositionTemp.w * dx_clipping.y), _glPositionTemp.z * 2.0 - _glPositionTemp.w, _glPositionTemp.w);
    gl_TexCoord[4].xyz = _ZZ3SrZh0039.xyz;
    gl_TexCoord[1].xyz = _ZZ3SrZh0037.xyz;
    gl_TexCoord[3].xyz = _ZZ3SrZh0035.xyz;
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
//semantic diffuse2Sampler
//semantic specular
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
//var sampler2D diffuse2Sampler :  : _ZZ2Sdiffuse2Sampler : -1 : 1
//var float4 specular :  : _ZZ2Sspecular : -1 : 1
//var sampler2D bumpSampler :  : _ZZ2SbumpSampler : -1 : 1
//var float shininess :  : _ZZ2Sshininess : -1 : 1
//var float4 input.diffuseUV : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float3 input.tangent : $vin.TEXCOORD1 : TEXCOORD1 : 0 : 1
//var float3 input.tangent2 : $vin.TEXCOORD2 : TEXCOORD2 : 0 : 1
//var float3 input.binormal : $vin.TEXCOORD3 : TEXCOORD3 : 0 : 1
//var float3 input.binormal2 : $vin.TEXCOORD4 : TEXCOORD4 : 0 : 1
//var float4 input.worldPosition : $vin.TEXCOORD5 : TEXCOORD5 : 0 : 1
//var float3 input.normal : $vin.TEXCOORD6 : TEXCOORD6 : 0 : 1
//var float4 input.color : $vin.COLOR : COLOR : 0 : 1
//var float4 pixelShaderFunction : $vout.COLOR : COLOR : -1 : 1



struct InVertex {
    vec4 normal;
    vec2 diffuseUV;
    vec2 diffuse2UV;
    vec3 tangent;
    vec3 tangent2;
    vec3 binormal;
    vec3 binormal2;
    vec4 color;
};

struct OutVertex {
    vec4 diffuseUV;
    vec3 tangent;
    vec3 tangent2;
    vec3 binormal;
    vec3 binormal2;
    vec4 worldPosition;
    vec3 normal;
    vec4 color;
};

vec4 _ZZ3Sret_0;
vec3 _ZZ3SrZh0033;
vec3 _ZZ3SZDtemp34;
float _ZZ3SxZh0039;
vec3 _ZZ3SrZh0043;
float _ZZ3SxZh0049;
vec3 _ZZ3SZDtemp50;
vec3 _ZZ3SvZh0051;
float _ZZ3SxZh0055;
vec4 _ZZ3SZDtemp56;
vec4 _ZZ3SvZh0057;
float _ZZ3SxZh0061;
vec3 _ZZ3SZDtemp62;
vec3 _ZZ3SvZh0063;
float _ZZ3SxZh0067;
vec4 _ZZ3StmpZh0073;
float _ZZ3SndotlZh0073;
float _ZZ3SndothZh0073;
vec4 _ZZ3SZDtemp74;
float _ZZ3SspecularZh0075;
float _ZZ3SxZh0079;
uniform vec3 lightWorldPos;
uniform vec4 lightColor;
uniform float clipHeight;
uniform mat4 viewInverse;
uniform vec4 emissive;
uniform vec4 ambient;
uniform sampler2D diffuseSampler;
uniform sampler2D diffuse2Sampler;
uniform vec4 specular;
uniform sampler2D bumpSampler;
uniform float shininess;

 // main procedure, the original name was pixelShaderFunction
void main()
{

    vec4 _ZZ4Sdiffuse1;
    vec4 _ZZ4Sdiffuse2;
    vec3 _ZZ4StangentNormal;
    vec4 _ZZ4Sdiffuse;
    float _ZZ4Salpha;
    vec3 _ZZ4SZaTMP5;

    _ZZ4Sdiffuse1 = texture2D(diffuseSampler, gl_TexCoord[0].xy);
    _ZZ4Sdiffuse2 = texture2D(diffuse2Sampler, gl_TexCoord[0].zw);
    _ZZ4StangentNormal = texture2D(bumpSampler, gl_TexCoord[0].xy).xyz - vec3( 5.00000000E-01, 5.00000000E-01, 5.00000000E-01);
    _ZZ3SrZh0033 = _ZZ4StangentNormal.x*gl_TexCoord[1].xyz;
    _ZZ3SrZh0033 = _ZZ3SrZh0033 + _ZZ4StangentNormal.y*gl_TexCoord[3].xyz;
    _ZZ3SrZh0033 = _ZZ3SrZh0033 + _ZZ4StangentNormal.z*gl_TexCoord[6].xyz;
    _ZZ3SxZh0039 = dot(_ZZ3SrZh0033, _ZZ3SrZh0033);
    _ZZ3SZDtemp34 = inversesqrt(_ZZ3SxZh0039)*_ZZ3SrZh0033;
    _ZZ4StangentNormal = texture2D(bumpSampler, gl_TexCoord[0].zw).xyz - vec3( 5.00000000E-01, 5.00000000E-01, 5.00000000E-01);
    _ZZ3SrZh0043 = _ZZ4StangentNormal.x*gl_TexCoord[2].xyz;
    _ZZ3SrZh0043 = _ZZ3SrZh0043 + _ZZ4StangentNormal.y*gl_TexCoord[4].xyz;
    _ZZ3SrZh0043 = _ZZ3SrZh0043 + _ZZ4StangentNormal.z*gl_TexCoord[6].xyz;
    _ZZ3SxZh0049 = dot(_ZZ3SrZh0043, _ZZ3SrZh0043);
    inversesqrt(_ZZ3SxZh0049)*_ZZ3SrZh0043;
    _ZZ3SvZh0051 = lightWorldPos - gl_TexCoord[5].xyz;
    _ZZ3SxZh0055 = dot(_ZZ3SvZh0051, _ZZ3SvZh0051);
    _ZZ3SZDtemp50 = inversesqrt(_ZZ3SxZh0055)*_ZZ3SvZh0051;
    _ZZ3SvZh0057 = viewInverse[3] - gl_TexCoord[5];
    _ZZ3SxZh0061 = dot(_ZZ3SvZh0057, _ZZ3SvZh0057);
    _ZZ3SZDtemp56 = inversesqrt(_ZZ3SxZh0061)*_ZZ3SvZh0057;
    _ZZ3SvZh0063 = _ZZ3SZDtemp50 + _ZZ3SZDtemp56.xyz;
    _ZZ3SxZh0067 = dot(_ZZ3SvZh0063, _ZZ3SvZh0063);
    _ZZ3SZDtemp62 = inversesqrt(_ZZ3SxZh0067)*_ZZ3SvZh0063;
    _ZZ3SndotlZh0073 = dot(_ZZ3SZDtemp34, _ZZ3SZDtemp50);
    _ZZ3SndothZh0073 = dot(_ZZ3SZDtemp34, _ZZ3SZDtemp62);
    _ZZ3StmpZh0073 = vec4(_ZZ3SndotlZh0073, _ZZ3SndothZh0073, shininess, shininess);
    _ZZ3SxZh0079 = max(0.00000000E+00, _ZZ3StmpZh0073.y);
    _ZZ3SspecularZh0075 = _ZZ3StmpZh0073.x > 0.00000000E+00 ? pow(_ZZ3SxZh0079, _ZZ3StmpZh0073.z) : 0.00000000E+00;
    _ZZ3SZDtemp74 = vec4(1.00000000E+00, max(0.00000000E+00, _ZZ3StmpZh0073.x), _ZZ3SspecularZh0075, 1.00000000E+00);
    _ZZ4Sdiffuse = _ZZ4Sdiffuse1 + gl_Color.w*(_ZZ4Sdiffuse2 - _ZZ4Sdiffuse1);
    _ZZ4Salpha = gl_TexCoord[5].z > clipHeight ? 0.00000000E+00 : _ZZ4Sdiffuse.w;
    _ZZ4SZaTMP5 = (emissive + lightColor*(ambient*_ZZ4Sdiffuse + _ZZ4Sdiffuse*_ZZ3SZDtemp74.y + specular*_ZZ3SZDtemp74.z)).xyz;
    _ZZ3Sret_0 = vec4(_ZZ4SZaTMP5.x, _ZZ4SZaTMP5.y, _ZZ4SZaTMP5.z, _ZZ4Salpha);
    gl_FragColor = _ZZ3Sret_0;
    return;
} // main end

