/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// The 4x4 world view projection matrix.
uniform mat4 worldViewProjection;
uniform mat4 worldInverseTranspose;
uniform mat4 world;

// input parameters for our vertex shader
attribute vec4 position;
attribute vec4 normal;
attribute vec2 texCoord0;

// input parameters for our pixel shader
varying vec2 v_texcoord;
varying vec3 v_normal;
varying vec3 v_worldPosition;

/**
 * Our vertex shader. In the vertex shader, we calculate the lighting.
 * Then we'll combine it with our checker pattern input the pixel shader.
 */
void main() {
  gl_Position = worldViewProjection * position;

  // Transform normal into world space, where we can do lighting
  // calculations even if the world transform contains scaling.
  v_normal = (worldInverseTranspose * normal).xyz;

  // Calculate surface position in world space.
  v_worldPosition = (world * position).xyz;
  v_texcoord = texCoord0;
}

// #o3d SplitMarker

varying vec2 v_texcoord;
varying vec3 v_normal;
varying vec3 v_worldPosition;

// Default and light position
uniform vec4 ambientIntensity;
uniform vec4 ambient;
uniform vec4 diffuse;
uniform vec3 lightWorldPos;
uniform vec3 cameraEye;

// function for getting the checker pattern
vec4 checker(vec2 uv) {
  float checkSize = 4.0;
  float fmodResult = mod(floor(checkSize * uv.x) + floor(checkSize * uv.y),
                          2.0);
  if (fmodResult < 1.0) {
    return vec4(0, 1, 1, 1);  // turquiose
  } else {
    return vec4(1, 0, 1, 1);  // magenta
  }
}

/**
 * Our pixel shader. We take the lighting color we got from the vertex shader
 * and combine it with our checker pattern. We only need to use the x
 * coordinate of our input.col because we gave it uniform color
 */
void main() {
  vec3 surfaceToLight = normalize(lightWorldPos - v_worldPosition);
  vec3 worldNormal = normalize(v_normal);

  // Apply diffuse lighting in world space in case the world transform
  // contains scaling.
  vec4 check = checker(v_texcoord);
  float directionalIntensity = clamp(dot(worldNormal, surfaceToLight), 0.0,
      1.0);
  vec4 outColor = (ambientIntensity * ambient +
      directionalIntensity * diffuse) * check;
  gl_FragColor = vec4(outColor.rgb, diffuse.a);
}

// #o3d MatrixLoadOrder RowMajor
