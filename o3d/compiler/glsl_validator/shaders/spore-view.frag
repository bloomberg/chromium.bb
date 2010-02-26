varying vec3 vNormal;
varying vec3 vViewVec;
varying vec2 vTexCoord0;

uniform vec4 uColor;
uniform sampler2D uTexture0;

void main(void) {
    float v = 0.5 * (1.0 + dot(normalize(vViewVec), vNormal));
    
    gl_FragColor = texture2D(uTexture0, vTexCoord0.st);
}
