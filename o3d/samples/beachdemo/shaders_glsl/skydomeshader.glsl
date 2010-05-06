// glslv profile log:
// 62 lines, 0 errors.

// glslf profile log:
// 62 lines, 0 errors.

// glslv output by Cg compiler
// cgc version 2.0.0010, build date Dec 12 2007
// command line args: -profile glslv
//vendor NVIDIA Corporation
//version 2.0.0.10
//profile glslv
//program vertexShaderFunction
//semantic worldViewProjectionInverse : VIEWPROJECTIONINVERSE
//semantic viewInverse : VIEWINVERSE
//semantic environmentSampler
//var float4x4 worldViewProjectionInverse : VIEWPROJECTIONINVERSE : _ZZ2SworldViewProjectionInverse[0], 4 : -1 : 1
//var float4x4 viewInverse : VIEWINVERSE : , 4 : -1 : 0
//var samplerCUBE environmentSampler :  :  : -1 : 0
//var float4 input.position : $vin.POSITION : POSITION : 0 : 1
//var float4 vertexShaderFunction.position : $vout.POSITION : POSITION : -1 : 1
//var float3 vertexShaderFunction.worldPosition : $vout.TEXCOORD0 : TEXCOORD0 : -1 : 1

attribute vec4 position;
vec4 _glPositionTemp;
uniform vec4 dx_clipping;

struct VertexShaderInput {
    vec4 position;
};

struct PixelShaderInput {
    vec4 position;
    vec3 worldPosition;
};

PixelShaderInput _ZZ3Sret_0;
vec4 _ZZ3SrZh0004;
uniform mat4 viewProjectionInverse;

 // main procedure, the original name was vertexShaderFunction
void main()
{

    PixelShaderInput _ZZ4Soutput;

    _ZZ3SrZh0004 = position.x*viewProjectionInverse[0];
    _ZZ3SrZh0004 = _ZZ3SrZh0004 + position.y*viewProjectionInverse[1];
    _ZZ3SrZh0004 = _ZZ3SrZh0004 + position.z*viewProjectionInverse[2];
    _ZZ3SrZh0004 = _ZZ3SrZh0004 + position.w*viewProjectionInverse[3];
    _ZZ4Soutput.worldPosition = _ZZ3SrZh0004.xyz/_ZZ3SrZh0004.w;
    _ZZ3Sret_0.position = position;
    _ZZ3Sret_0.worldPosition = _ZZ4Soutput.worldPosition;
    gl_TexCoord[0].xyz = _ZZ4Soutput.worldPosition;
    _glPositionTemp = position; gl_Position = vec4(_glPositionTemp.x + _glPositionTemp.w * dx_clipping.x, dx_clipping.w * (_glPositionTemp.y + _glPositionTemp.w * dx_clipping.y), _glPositionTemp.z * 2.0 - _glPositionTemp.w, _glPositionTemp.w);
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
//semantic worldViewProjectionInverse : VIEWPROJECTIONINVERSE
//semantic viewInverse : VIEWINVERSE
//semantic environmentSampler
//var float4x4 worldViewProjectionInverse : VIEWPROJECTIONINVERSE : , 4 : -1 : 0
//var float4x4 viewInverse : VIEWINVERSE : _ZZ2SviewInverse[0], 4 : -1 : 1
//var samplerCUBE environmentSampler :  : _ZZ2SenvironmentSampler : -1 : 1
//var float3 input.worldPosition : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float4 pixelShaderFunction : $vout.COLOR : COLOR : -1 : 1



struct VertexShaderInput {
    int dummy;
};

struct PixelShaderInput {
    vec3 worldPosition;
};

vec4 _ZZ3Sret_0;
vec3 _ZZ3SZDtemp3;
vec3 _ZZ3SvZh0004;
float _ZZ3SxZh0008;
float _ZZ3SaZh0010;
vec3 _ZZ3ScZh0012;
vec3 _ZZ3SZaTMP13;
uniform mat4 viewInverse;
uniform samplerCube environmentSampler;

 // main procedure, the original name was pixelShaderFunction
void main()
{


    _ZZ3SZaTMP13.x = viewInverse[3].x;
    _ZZ3SZaTMP13.y = viewInverse[3].y;
    _ZZ3SZaTMP13.z = viewInverse[3].z;
    _ZZ3SvZh0004 = _ZZ3SZaTMP13 - gl_TexCoord[0].xyz;
    _ZZ3SxZh0008 = dot(_ZZ3SvZh0004, _ZZ3SvZh0004);
    _ZZ3SZDtemp3 = inversesqrt(_ZZ3SxZh0008)*_ZZ3SvZh0004;
    _ZZ3SaZh0010 = -_ZZ3SZDtemp3.z;
    _ZZ3ScZh0012 = vec3(_ZZ3SZDtemp3.x, abs(_ZZ3SaZh0010) + 9.99999978E-03, -_ZZ3SZDtemp3.y);
    _ZZ3Sret_0 = textureCube(environmentSampler, _ZZ3ScZh0012);
    gl_FragColor = _ZZ3Sret_0;
    return;
} // main end

