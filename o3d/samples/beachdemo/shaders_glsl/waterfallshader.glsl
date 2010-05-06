// glslv profile log:
// 80 lines, 0 errors.

// glslf profile log:
// 80 lines, 0 errors.

// glslv output by Cg compiler
// cgc version 2.0.0010, build date Dec 12 2007
// command line args: -profile glslv
//vendor NVIDIA Corporation
//version 2.0.0.10
//profile glslv
//program vertexShaderFunction
//semantic worldViewProjection : WORLDVIEWPROJECTION
//semantic world : WORLD
//semantic viewInverse : VIEWINVERSE
//semantic worldInverseTranspose : WORLDINVERSETRANSPOSE
//semantic lightWorldPos
//semantic vOffset
//semantic diffuseSampler
//var float4x4 worldViewProjection : WORLDVIEWPROJECTION : _ZZ2SworldViewProjection[0], 4 : -1 : 1
//var float4x4 world : WORLD : _ZZ2Sworld[0], 4 : -1 : 1
//var float4x4 viewInverse : VIEWINVERSE : , 4 : -1 : 0
//var float4x4 worldInverseTranspose : WORLDINVERSETRANSPOSE : _ZZ2SworldInverseTranspose[0], 4 : -1 : 1
//var float3 lightWorldPos :  :  : -1 : 0
//var float vOffset :  :  : -1 : 0
//var sampler diffuseSampler :  :  : -1 : 0
//var float4 input.position : $vin.POSITION : POSITION : 0 : 1
//var float4 input.normal : $vin.ATTR8 : $_ZZ3SZaTMP13 : 0 : 1
//var float2 input.texcoord : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float4 vertexShaderFunction.position : $vout.POSITION : POSITION : -1 : 1
//var float3 vertexShaderFunction.normal : $vout.TEXCOORD0 : TEXCOORD0 : -1 : 1
//var float3 vertexShaderFunction.worldPosition : $vout.TEXCOORD1 : TEXCOORD1 : -1 : 1
//var float2 vertexShaderFunction.texcoord : $vout.TEXCOORD2 : TEXCOORD2 : -1 : 1

attribute vec4 position;
attribute vec4 texcoord0;
vec4 _glPositionTemp;
uniform vec4 dx_clipping;

struct VertexShaderInput {
    vec4 position;
    vec4 normal;
    vec2 texcoord;
};

struct PixelShaderInput {
    vec4 position;
    vec3 normal;
    vec3 worldPosition;
    vec2 texcoord;
};

PixelShaderInput _ZZ3Sret_0;
vec4 _ZZ3SrZh0008;
vec4 _ZZ3SrZh0010;
vec4 _ZZ3SrZh0012;
attribute vec4 normal;
uniform mat4 worldViewProjection;
uniform mat4 world;
uniform mat4 worldInverseTranspose;

 // main procedure, the original name was vertexShaderFunction
void main()
{


    _ZZ3SrZh0008 = position.x*worldViewProjection[0];
    _ZZ3SrZh0008 = _ZZ3SrZh0008 + position.y*worldViewProjection[1];
    _ZZ3SrZh0008 = _ZZ3SrZh0008 + position.z*worldViewProjection[2];
    _ZZ3SrZh0008 = _ZZ3SrZh0008 + position.w*worldViewProjection[3];
    _ZZ3SrZh0010 = position.x*world[0];
    _ZZ3SrZh0010 = _ZZ3SrZh0010 + position.y*world[1];
    _ZZ3SrZh0010 = _ZZ3SrZh0010 + position.z*world[2];
    _ZZ3SrZh0010 = _ZZ3SrZh0010 + position.w*world[3];
    _ZZ3SrZh0012 = normal.x*worldInverseTranspose[0];
    _ZZ3SrZh0012 = _ZZ3SrZh0012 + normal.y*worldInverseTranspose[1];
    _ZZ3SrZh0012 = _ZZ3SrZh0012 + normal.z*worldInverseTranspose[2];
    _ZZ3SrZh0012 = _ZZ3SrZh0012 + normal.w*worldInverseTranspose[3];
    _ZZ3SrZh0012.xyz;
    _ZZ3SrZh0010.xyz;
    _ZZ3Sret_0.position = _ZZ3SrZh0008;
    _ZZ3Sret_0.normal = _ZZ3SrZh0012.xyz;
    _ZZ3Sret_0.worldPosition = _ZZ3SrZh0010.xyz;
    _ZZ3Sret_0.texcoord = texcoord0.xy;
    gl_TexCoord[0].xyz = _ZZ3SrZh0012.xyz;
    gl_TexCoord[2].xy = texcoord0.xy;
    _glPositionTemp = _ZZ3SrZh0008; gl_Position = vec4(_glPositionTemp.x + _glPositionTemp.w * dx_clipping.x, dx_clipping.w * (_glPositionTemp.y + _glPositionTemp.w * dx_clipping.y), _glPositionTemp.z * 2.0 - _glPositionTemp.w, _glPositionTemp.w);
    gl_TexCoord[1].xyz = _ZZ3SrZh0010.xyz;
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
//semantic world : WORLD
//semantic viewInverse : VIEWINVERSE
//semantic worldInverseTranspose : WORLDINVERSETRANSPOSE
//semantic lightWorldPos
//semantic vOffset
//semantic diffuseSampler
//var float4x4 worldViewProjection : WORLDVIEWPROJECTION : , 4 : -1 : 0
//var float4x4 world : WORLD : , 4 : -1 : 0
//var float4x4 viewInverse : VIEWINVERSE : _ZZ2SviewInverse[0], 4 : -1 : 1
//var float4x4 worldInverseTranspose : WORLDINVERSETRANSPOSE : , 4 : -1 : 0
//var float3 lightWorldPos :  : _ZZ2SlightWorldPos : -1 : 1
//var float vOffset :  : _ZZ2SvOffset : -1 : 1
//var sampler diffuseSampler :  : _ZZ2SdiffuseSampler : -1 : 1
//var float3 input.normal : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float3 input.worldPosition : $vin.TEXCOORD1 : TEXCOORD1 : 0 : 1
//var float2 input.texcoord : $vin.TEXCOORD2 : TEXCOORD2 : 0 : 1
//var float4 pixelShaderFunction : $vout.COLOR : COLOR : -1 : 1



struct VertexShaderInput {
    vec4 normal;
    vec2 texcoord;
};

struct PixelShaderInput {
    vec3 normal;
    vec3 worldPosition;
    vec2 texcoord;
};

vec4 _ZZ3Sret_0;
vec3 _ZZ3SZDtemp7;
vec3 _ZZ3SvZh0008;
float _ZZ3SxZh0012;
vec3 _ZZ3SZDtemp13;
float _ZZ3SxZh0018;
vec3 _ZZ3SZDtemp19;
vec3 _ZZ3SvZh0020;
float _ZZ3SxZh0024;
vec3 _ZZ3SZDtemp25;
vec3 _ZZ3SvZh0026;
float _ZZ3SxZh0030;
vec4 _ZZ3StmpZh0036;
float _ZZ3SndotlZh0036;
float _ZZ3SndothZh0036;
float _ZZ3SspecularZh0038;
float _ZZ3SxZh0042;
sampler2D _ZZ3SsZh0046;
vec2 _ZZ3ScZh0046;
vec3 _ZZ3SZaTMP47;
uniform mat4 viewInverse;
uniform vec3 lightWorldPos;
uniform float vOffset;
uniform sampler diffuseSampler;

 // main procedure, the original name was pixelShaderFunction
void main()
{

    vec4 _ZZ4Sdiffuse;

    _ZZ3SvZh0008 = lightWorldPos - gl_TexCoord[1].xyz;
    _ZZ3SxZh0012 = dot(_ZZ3SvZh0008, _ZZ3SvZh0008);
    _ZZ3SZDtemp7 = inversesqrt(_ZZ3SxZh0012)*_ZZ3SvZh0008;
    _ZZ3SxZh0018 = dot(gl_TexCoord[0].xyz, gl_TexCoord[0].xyz);
    _ZZ3SZDtemp13 = inversesqrt(_ZZ3SxZh0018)*gl_TexCoord[0].xyz;
    _ZZ3SZaTMP47.x = viewInverse[3].x;
    _ZZ3SZaTMP47.y = viewInverse[3].y;
    _ZZ3SZaTMP47.z = viewInverse[3].z;
    _ZZ3SvZh0020 = _ZZ3SZaTMP47 - gl_TexCoord[1].xyz;
    _ZZ3SxZh0024 = dot(_ZZ3SvZh0020, _ZZ3SvZh0020);
    _ZZ3SZDtemp19 = inversesqrt(_ZZ3SxZh0024)*_ZZ3SvZh0020;
    _ZZ3SvZh0026 = _ZZ3SZDtemp7 + _ZZ3SZDtemp19;
    _ZZ3SxZh0030 = dot(_ZZ3SvZh0026, _ZZ3SvZh0026);
    _ZZ3SZDtemp25 = inversesqrt(_ZZ3SxZh0030)*_ZZ3SvZh0026;
    _ZZ3SndotlZh0036 = dot(_ZZ3SZDtemp13, _ZZ3SZDtemp7);
    _ZZ3SndothZh0036 = dot(_ZZ3SZDtemp13, _ZZ3SZDtemp25);
    _ZZ3StmpZh0036 = vec4(_ZZ3SndotlZh0036, _ZZ3SndothZh0036, 0.00000000E+00, 0.00000000E+00);
    _ZZ3SxZh0042 = max(0.00000000E+00, _ZZ3StmpZh0036.y);
    _ZZ3SspecularZh0038 = _ZZ3StmpZh0036.x > 0.00000000E+00 ? pow(_ZZ3SxZh0042, _ZZ3StmpZh0036.z) : 0.00000000E+00;
    vec4(1.00000000E+00, max(0.00000000E+00, _ZZ3StmpZh0036.x), _ZZ3SspecularZh0038, 1.00000000E+00);
    _ZZ3SsZh0046 = sampler2D(diffuseSampler);
    _ZZ3ScZh0046 = vec2(gl_TexCoord[2].x, gl_TexCoord[2].y + vOffset);
    _ZZ4Sdiffuse = texture2D(_ZZ3SsZh0046, _ZZ3ScZh0046);
    _ZZ3Sret_0 = _ZZ4Sdiffuse;
    gl_FragColor = _ZZ4Sdiffuse;
    return;
} // main end

