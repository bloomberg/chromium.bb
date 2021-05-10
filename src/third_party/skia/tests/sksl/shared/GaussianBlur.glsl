
out vec4 sk_FragColor;
layout (binding = 0) uniform sampler2D uTextureSampler_0_Stage1;
layout (binding = 0) uniform uniformBuffer {
    layout (offset = 0) vec4 sk_RTAdjust;
    layout (offset = 16) vec2 uIncrement_Stage1_c0;
    layout (offset = 32) vec4[7] uKernel_Stage1_c0;
    layout (offset = 144) mat3 umatrix_Stage1_c0_c0;
    layout (offset = 192) vec4 uborder_Stage1_c0_c0_c0;
    layout (offset = 208) vec4 usubset_Stage1_c0_c0_c0;
    layout (offset = 224) vec4 unorm_Stage1_c0_c0_c0;
};
layout (location = 0) in vec2 vLocalCoord_Stage0;
vec4 MatrixEffect_Stage1_c0_c0(vec4 _input, vec2 _coords) {
    vec2 _1_coords = (umatrix_Stage1_c0_c0 * vec3(_coords, 1.0)).xy;
    vec2 _2_inCoord = _1_coords;
    _2_inCoord *= unorm_Stage1_c0_c0_c0.xy;
    vec2 _3_subsetCoord;
    _3_subsetCoord.x = _2_inCoord.x;
    _3_subsetCoord.y = _2_inCoord.y;
    vec2 _4_clampedCoord;
    _4_clampedCoord = _3_subsetCoord;
    vec4 _5_textureColor = texture(uTextureSampler_0_Stage1, _4_clampedCoord * unorm_Stage1_c0_c0_c0.zw);
    float _6_snappedX = floor(_2_inCoord.x + 0.0010000000474974513) + 0.5;
    if (_6_snappedX < usubset_Stage1_c0_c0_c0.x || _6_snappedX > usubset_Stage1_c0_c0_c0.z) {
        _5_textureColor = uborder_Stage1_c0_c0_c0;
    }
    return _5_textureColor;

}
void main() {
    vec4 output_Stage1;
    vec4 _8_output;
    _8_output = vec4(0.0, 0.0, 0.0, 0.0);
    vec2 _9_coord = vLocalCoord_Stage0 - 12.0 * uIncrement_Stage1_c0;
    vec2 _10_coordSampled = vec2(0.0, 0.0);
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[0].x;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[0].y;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[0].z;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[0].w;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[1].x;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[1].y;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[1].z;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[1].w;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[2].x;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[2].y;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[2].z;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[2].w;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[3].x;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[3].y;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[3].z;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[3].w;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[4].x;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[4].y;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[4].z;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[4].w;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[5].x;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[5].y;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[5].z;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[5].w;
    _9_coord += uIncrement_Stage1_c0;
    _10_coordSampled = _9_coord;
    _8_output += MatrixEffect_Stage1_c0_c0(vec4(1.0), _10_coordSampled) * uKernel_Stage1_c0[6].x;
    _9_coord += uIncrement_Stage1_c0;
    output_Stage1 = _8_output;

    {
        sk_FragColor = output_Stage1;
    }
}
