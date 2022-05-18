#version 310 es

layout(location = 0) in vec2 a_particlePos_1;
layout(location = 1) in vec2 a_particleVel_1;
layout(location = 2) in vec2 a_pos_1;
struct Particle {
  vec2 pos;
  vec2 vel;
};

struct SimParams {
  float deltaT;
  float rule1Distance;
  float rule2Distance;
  float rule3Distance;
  float rule1Scale;
  float rule2Scale;
  float rule3Scale;
};

struct Particles {
  Particle particles[5];
};

vec4 vert_main(vec2 a_particlePos, vec2 a_particleVel, vec2 a_pos) {
  float angle = -(atan(a_particleVel.x, a_particleVel.y));
  vec2 pos = vec2(((a_pos.x * cos(angle)) - (a_pos.y * sin(angle))), ((a_pos.x * sin(angle)) + (a_pos.y * cos(angle))));
  return vec4((pos + a_particlePos), 0.0f, 1.0f);
}

void main() {
  gl_PointSize = 1.0;
  vec4 inner_result = vert_main(a_particlePos_1, a_particleVel_1, a_pos_1);
  gl_Position = inner_result;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
#version 310 es
precision mediump float;

layout(location = 0) out vec4 value;
struct Particle {
  vec2 pos;
  vec2 vel;
};

struct SimParams {
  float deltaT;
  float rule1Distance;
  float rule2Distance;
  float rule3Distance;
  float rule1Scale;
  float rule2Scale;
  float rule3Scale;
};

struct Particles {
  Particle particles[5];
};

vec4 frag_main() {
  return vec4(1.0f, 1.0f, 1.0f, 1.0f);
}

void main() {
  vec4 inner_result = frag_main();
  value = inner_result;
  return;
}
#version 310 es

struct Particle {
  vec2 pos;
  vec2 vel;
};

struct SimParams {
  float deltaT;
  float rule1Distance;
  float rule2Distance;
  float rule3Distance;
  float rule1Scale;
  float rule2Scale;
  float rule3Scale;
};

struct Particles {
  Particle particles[5];
};

layout(binding = 0) uniform SimParams_1 {
  float deltaT;
  float rule1Distance;
  float rule2Distance;
  float rule3Distance;
  float rule1Scale;
  float rule2Scale;
  float rule3Scale;
} params;

layout(binding = 1, std430) buffer Particles_1 {
  Particle particles[5];
} particlesA;
layout(binding = 2, std430) buffer Particles_2 {
  Particle particles[5];
} particlesB;
void comp_main(uvec3 tint_symbol) {
  uint index = tint_symbol.x;
  if ((index >= 5u)) {
    return;
  }
  vec2 vPos = particlesA.particles[index].pos;
  vec2 vVel = particlesA.particles[index].vel;
  vec2 cMass = vec2(0.0f, 0.0f);
  vec2 cVel = vec2(0.0f, 0.0f);
  vec2 colVel = vec2(0.0f, 0.0f);
  int cMassCount = 0;
  int cVelCount = 0;
  vec2 pos = vec2(0.0f, 0.0f);
  vec2 vel = vec2(0.0f, 0.0f);
  {
    for(uint i = 0u; (i < 5u); i = (i + 1u)) {
      if ((i == index)) {
        continue;
      }
      pos = particlesA.particles[i].pos.xy;
      vel = particlesA.particles[i].vel.xy;
      if ((distance(pos, vPos) < params.rule1Distance)) {
        cMass = (cMass + pos);
        cMassCount = (cMassCount + 1);
      }
      if ((distance(pos, vPos) < params.rule2Distance)) {
        colVel = (colVel - (pos - vPos));
      }
      if ((distance(pos, vPos) < params.rule3Distance)) {
        cVel = (cVel + vel);
        cVelCount = (cVelCount + 1);
      }
    }
  }
  if ((cMassCount > 0)) {
    cMass = ((cMass / vec2(float(cMassCount), float(cMassCount))) - vPos);
  }
  if ((cVelCount > 0)) {
    cVel = (cVel / vec2(float(cVelCount), float(cVelCount)));
  }
  vVel = (((vVel + (cMass * params.rule1Scale)) + (colVel * params.rule2Scale)) + (cVel * params.rule3Scale));
  vVel = (normalize(vVel) * clamp(length(vVel), 0.0f, 0.100000001f));
  vPos = (vPos + (vVel * params.deltaT));
  if ((vPos.x < -1.0f)) {
    vPos.x = 1.0f;
  }
  if ((vPos.x > 1.0f)) {
    vPos.x = -1.0f;
  }
  if ((vPos.y < -1.0f)) {
    vPos.y = 1.0f;
  }
  if ((vPos.y > 1.0f)) {
    vPos.y = -1.0f;
  }
  particlesB.particles[index].pos = vPos;
  particlesB.particles[index].vel = vVel;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  comp_main(gl_GlobalInvocationID);
  return;
}
