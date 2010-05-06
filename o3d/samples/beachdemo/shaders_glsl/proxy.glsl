// glslv profile log:
// (66) : warning C7011: implicit cast from "float4" to "float3"
// (67) : warning C7011: implicit cast from "float4" to "float3"
// 81 lines, 2 warnings, 0 errors.

// glslf profile log:
// (66) : warning C7011: implicit cast from "float4" to "float3"
// (67) : warning C7011: implicit cast from "float4" to "float3"
// 81 lines, 2 warnings, 0 errors.

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
//semantic viewInverse : VIEWINVERSE
//semantic worldInverseTranspose : WORLDINVERSETRANSPOSE
//semantic emissive
//semantic ambient
//semantic diffuse
//semantic specular
//semantic shininess
//semantic offset
//var float4x4 world : WORLD : _ZZ2Sworld[0], 4 : -1 : 1
//var float4x4 worldViewProjection : WORLDVIEWPROJECTION : _ZZ2SworldViewProjection[0], 4 : -1 : 1
//var float3 lightWorldPos :  :  : -1 : 0
//var float4 lightColor :  :  : -1 : 0
//var float4x4 viewInverse : VIEWINVERSE : , 4 : -1 : 0
//var float4x4 worldInverseTranspose : WORLDINVERSETRANSPOSE : _ZZ2SworldInverseTranspose[0], 4 : -1 : 1
//var float4 emissive :  :  : -1 : 0
//var float4 ambient :  :  : -1 : 0
//var float4 diffuse :  :  : -1 : 0
//var float4 specular :  :  : -1 : 0
//var float shininess :  :  : -1 : 0
//var float offset :  :  : -1 : 0
//var float4 input.position : $vin.POSITION : POSITION : 0 : 1
//var float4 input.normal : $vin.ATTR8 : $_ZZ3SZaTMP20 : 0 : 1
//var float4 vertexShaderFunction.position : $vout.POSITION : POSITION : -1 : 1
//var float4 vertexShaderFunction.worldPosition : $vout.TEXCOORD0 : TEXCOORD0 : -1 : 1
//var float3 vertexShaderFunction.normal : $vout.TEXCOORD1 : TEXCOORD1 : -1 : 1

attribute vec4 position;
vec4 _glPositionTemp;
uniform vec4 dx_clipping;

struct InVertex {
    vec4 position;
    vec4 normal;
};

struct OutVertex {
    vec4 position;
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
    _ZZ3Sret_0.worldPosition = _ZZ3SrZh0017;
    _ZZ3Sret_0.normal = _ZZ3SrZh0019.xyz;
    gl_TexCoord[0] = _ZZ3SrZh0017;
    _glPositionTemp = _ZZ3SrZh0015; gl_Position = vec4(_glPositionTemp.x + _glPositionTemp.w * dx_clipping.x, dx_clipping.w * (_glPositionTemp.y + _glPositionTemp.w * dx_clipping.y), _glPositionTemp.z * 2.0 - _glPositionTemp.w, _glPositionTemp.w);
    gl_TexCoord[1].xyz = _ZZ3SrZh0019.xyz;
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
//semantic viewInverse : VIEWINVERSE
//semantic worldInverseTranspose : WORLDINVERSETRANSPOSE
//semantic emissive
//semantic ambient
//semantic diffuse
//semantic specular
//semantic shininess
//semantic offset
//var float4x4 world : WORLD : , 4 : -1 : 0
//var float4x4 worldViewProjection : WORLDVIEWPROJECTION : , 4 : -1 : 0
//var float3 lightWorldPos :  : _ZZ2SlightWorldPos : -1 : 1
//var float4 lightColor :  : _ZZ2SlightColor : -1 : 1
//var float4x4 viewInverse : VIEWINVERSE : _ZZ2SviewInverse[0], 4 : -1 : 1
//var float4x4 worldInverseTranspose : WORLDINVERSETRANSPOSE : , 4 : -1 : 0
//var float4 emissive :  : _ZZ2Semissive : -1 : 1
//var float4 ambient :  : _ZZ2Sambient : -1 : 1
//var float4 diffuse :  : _ZZ2Sdiffuse : -1 : 1
//var float4 specular :  : _ZZ2Sspecular : -1 : 1
//var float shininess :  : _ZZ2Sshininess : -1 : 1
//var float offset :  : _ZZ2Soffset : -1 : 1
//var float4 input.worldPosition : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float3 input.normal : $vin.TEXCOORD1 : TEXCOORD1 : 0 : 1
//var float4 pixelShaderFunction : $vout.COLOR : COLOR : -1 : 1



struct InVertex {
    vec4 normal;
};

struct OutVertex {
    vec4 worldPosition;
    vec3 normal;
};

vec4 _ZZ3Sret_0;
vec3 _ZZ3SZDtemp14;
float _ZZ3SxZh0019;
vec3 _ZZ3SZDtemp20;
vec3 _ZZ3SvZh0021;
float _ZZ3SxZh0025;
vec4 _ZZ3SZDtemp26;
vec4 _ZZ3SvZh0027;
float _ZZ3SxZh0031;
vec3 _ZZ3SZDtemp32;
vec3 _ZZ3SvZh0033;
float _ZZ3SxZh0037;
vec4 _ZZ3StmpZh0043;
float _ZZ3SndotlZh0043;
float _ZZ3SndothZh0043;
vec4 _ZZ3SZDtemp44;
float _ZZ3SspecularZh0045;
float _ZZ3SxZh0049;
uniform vec3 lightWorldPos;
uniform vec4 lightColor;
uniform mat4 viewInverse;
uniform vec4 emissive;
uniform vec4 ambient;
uniform vec4 diffuse;
uniform vec4 specular;
uniform float shininess;
uniform float offset;

 // main procedure, the original name was pixelShaderFunction
void main()
{

    float _ZZ4Salpha;
    vec3 _ZZ4SZaTMP1;

    _ZZ3SxZh0019 = dot(gl_TexCoord[1].xyz, gl_TexCoord[1].xyz);
    _ZZ3SZDtemp14 = inversesqrt(_ZZ3SxZh0019)*gl_TexCoord[1].xyz;
    _ZZ3SvZh0021 = lightWorldPos - gl_TexCoord[0].xyz;
    _ZZ3SxZh0025 = dot(_ZZ3SvZh0021, _ZZ3SvZh0021);
    _ZZ3SZDtemp20 = inversesqrt(_ZZ3SxZh0025)*_ZZ3SvZh0021;
    _ZZ3SvZh0027 = viewInverse[3] - gl_TexCoord[0];
    _ZZ3SxZh0031 = dot(_ZZ3SvZh0027, _ZZ3SvZh0027);
    _ZZ3SZDtemp26 = inversesqrt(_ZZ3SxZh0031)*_ZZ3SvZh0027;
    _ZZ3SvZh0033 = _ZZ3SZDtemp20 + _ZZ3SZDtemp26.xyz;
    _ZZ3SxZh0037 = dot(_ZZ3SvZh0033, _ZZ3SvZh0033);
    _ZZ3SZDtemp32 = inversesqrt(_ZZ3SxZh0037)*_ZZ3SvZh0033;
    _ZZ3SndotlZh0043 = dot(_ZZ3SZDtemp14, _ZZ3SZDtemp20);
    _ZZ3SndothZh0043 = dot(_ZZ3SZDtemp14, _ZZ3SZDtemp32);
    _ZZ3StmpZh0043 = vec4(_ZZ3SndotlZh0043, _ZZ3SndothZh0043, shininess, shininess);
    _ZZ3SxZh0049 = max(0.00000000E+00, _ZZ3StmpZh0043.y);
    _ZZ3SspecularZh0045 = _ZZ3StmpZh0043.x > 0.00000000E+00 ? pow(_ZZ3SxZh0049, _ZZ3StmpZh0043.z) : 0.00000000E+00;
    _ZZ3SZDtemp44 = vec4(1.00000000E+00, max(0.00000000E+00, _ZZ3StmpZh0043.x), _ZZ3SspecularZh0045, 1.00000000E+00);
    _ZZ4Salpha = gl_TexCoord[0].z > offset ? 0.00000000E+00 : diffuse.w;
    _ZZ4SZaTMP1 = (emissive + lightColor*(ambient*diffuse + diffuse*_ZZ3SZDtemp44.y + specular*_ZZ3SZDtemp44.z)).xyz;
    _ZZ3Sret_0 = vec4(_ZZ4SZaTMP1.x, _ZZ4SZaTMP1.y, _ZZ4SZaTMP1.z, _ZZ4Salpha);
    gl_FragColor = _ZZ3Sret_0;
    return;
} // main end

