attribute vec3 aVertex;
attribute vec3 aNormal;
attribute vec2 aTexCoord0;

uniform mat4 uPMatrix;
uniform mat4 uMVMatrix;
uniform vec4 uViewPosition;

varying vec3 vNormal;
varying vec3 vViewVec;
varying vec2 vTexCoord0;

void main(void) {
    gl_Position = uPMatrix * uMVMatrix * vec4(aVertex, 1.0);

    vNormal = aNormal;
    vViewVec = uViewPosition.xyz - aVertex.xyz;
    vTexCoord0 = aTexCoord0;
}
