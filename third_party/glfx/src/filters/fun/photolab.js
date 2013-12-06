/**
 * @filter       Photo Lab
 * @description  Applies a film-developing effect.
 * @param minColor Minimum color per pixel.
 */
function photolab(minColor) {
    gl.photolab = gl.photolab || new Shader(null, '\
        uniform sampler2D texture;\
        uniform vec4 minColor;\
        varying vec2 texCoord;\
        void main() {\
            vec4 color = texture2D(texture, texCoord);\
            gl_FragColor = max(minColor, color);\
        }\
    ');

    simpleShader.call(this, gl.photolab, {
        minColor: [minColor[0], minColor[1], minColor[2], 1]
    });

    return this;
}
