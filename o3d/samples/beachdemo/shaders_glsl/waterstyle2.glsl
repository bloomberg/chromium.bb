// glslv profile log:
// 73 lines, 0 errors.

// glslf profile log:
// 73 lines, 0 errors.

// glslv output by Cg compiler
// cgc version 2.0.0010, build date Dec 12 2007
// command line args: -profile glslv
//vendor NVIDIA Corporation
//version 2.0.0.10
//profile glslv
//program vertexShaderFunction
//semantic worldViewProjection : WORLDVIEWPROJECTION
//var float4x4 worldViewProjection : WORLDVIEWPROJECTION : _ZZ2SworldViewProjection[0], 4 : -1 : 1
//var float4 input.position : $vin.POSITION : POSITION : 0 : 1
//var float4 vertexShaderFunction.position : $vout.POSITION : POSITION : -1 : 1

attribute vec4 position;
vec4 _glPositionTemp;
uniform vec4 dx_clipping;

struct VertexShaderInput {
    vec4 position;
};

struct PixelShaderInput {
    vec4 position;
};

PixelShaderInput _ZZ3Sret_0;
vec4 _ZZ3SrZh0002;
uniform mat4 worldViewProjection;

 // main procedure, the original name was vertexShaderFunction
void main()
{


    _ZZ3SrZh0002 = position.x*worldViewProjection[0];
    _ZZ3SrZh0002 = _ZZ3SrZh0002 + position.y*worldViewProjection[1];
    _ZZ3SrZh0002 = _ZZ3SrZh0002 + position.z*worldViewProjection[2];
    _ZZ3SrZh0002 = _ZZ3SrZh0002 + position.w*worldViewProjection[3];
    _ZZ3Sret_0.position = _ZZ3SrZh0002;
    _glPositionTemp = _ZZ3SrZh0002; gl_Position = vec4(_glPositionTemp.x + _glPositionTemp.w * dx_clipping.x, dx_clipping.w * (_glPositionTemp.y + _glPositionTemp.w * dx_clipping.y), _glPositionTemp.z * 2.0 - _glPositionTemp.w, _glPositionTemp.w);
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
//var float4x4 worldViewProjection : WORLDVIEWPROJECTION : , 4 : -1 : 0
//var float4 pixelShaderFunction : $vout.COLOR : COLOR : -1 : 1



struct VertexShaderInput {
    int dummy;
};

struct PixelShaderInput {
    int dummy;
};

vec4 _ZZ3Sret_0;

 // main procedure, the original name was pixelShaderFunction
void main()
{


    _ZZ3Sret_0 = vec4( 0.00000000E+00, 0.00000000E+00, 5.00000000E-01, 1.00000000E+00);
    gl_FragColor = vec4( 0.00000000E+00, 0.00000000E+00, 5.00000000E-01, 1.00000000E+00);
    return;
} // main end

