uniform mat4 viewProjection;
uniform mat4 world;
uniform mat4 viewInverse;
uniform vec3 worldVelocity;
uniform vec3 worldAcceleration;
uniform float timeRange;
uniform float time;
uniform float timeOffset;
uniform float frameDuration;
uniform float numFrames;

// Incoming vertex attributes
attribute vec4 uvLifeTimeFrameStart; // uv, lifeTime, frameStart
attribute vec4 positionStartTime;    // position.xyz, startTime
attribute vec4 velocityStartSize;    // velocity.xyz, startSize
attribute vec4 accelerationEndSize;  // acceleration.xyz, endSize
attribute vec4 spinStartSpinSpeed;   // spinStart.x, spinSpeed.y
attribute vec4 colorMult;            // multiplies color and ramp textures

// Outgoing variables to fragment shader
varying vec2 outputTexcoord;
varying float outputPercentLife;
varying vec4 outputColorMult;

void main() {
  vec2 uv = uvLifeTimeFrameStart.xy;
  float lifeTime = uvLifeTimeFrameStart.z;
  float frameStart = uvLifeTimeFrameStart.w;
  vec3 position = positionStartTime.xyz;
  float startTime = positionStartTime.w;
  vec3 velocity = (world * vec4(velocityStartSize.xyz,
                                0.)).xyz + worldVelocity;
  float startSize = velocityStartSize.w;
  vec3 acceleration = (world * vec4(accelerationEndSize.xyz,
                                    0)).xyz + worldAcceleration;
  float endSize = accelerationEndSize.w;
  float spinStart = spinStartSpinSpeed.x;
  float spinSpeed = spinStartSpinSpeed.y;

  float localTime = mod((time - timeOffset - startTime), timeRange);
  float percentLife = localTime / lifeTime;

  float frame = mod(floor(localTime / frameDuration + frameStart),
                    numFrames);
  float uOffset = frame / numFrames;
  float u = uOffset + (uv.x + 0.5) * (1. / numFrames);

  outputTexcoord = vec2(u, uv.y + 0.5);
  outputColorMult = colorMult;

  vec3 basisX = viewInverse[0].xyz;
  vec3 basisZ = viewInverse[1].xyz;

  float size = mix(startSize, endSize, percentLife);
  size = (percentLife < 0. || percentLife > 1.) ? 0. : size;
  float s = sin(spinStart + spinSpeed * localTime);
  float c = cos(spinStart + spinSpeed * localTime);

  vec2 rotatedPoint = vec2(uv.x * c + uv.y * s, 
                           -uv.x * s + uv.y * c);
  vec3 localPosition = vec3(basisX * rotatedPoint.x +
                            basisZ * rotatedPoint.y) * size +
                       velocity * localTime +
                       acceleration * localTime * localTime + 
                       position;

  outputPercentLife = percentLife;
  gl_Position = viewProjection * vec4(localPosition + world[3].xyz, 1.);
}
