attribute vec3 pos;
attribute vec4 colorIn;
uniform mat4 mvp;
varying vec4 color;
void main() {
  color = colorIn;
  gl_Position = mvp * vec4(pos.xyz, 1.);
}
