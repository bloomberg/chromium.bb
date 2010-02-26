attribute vec2 pos;

varying vec4 color;

uniform float minFade;

void main() {
  color = vec4(minFade, minFade, minFade, 1.);
  gl_Position = vec4(pos, 0., 1.);
}
