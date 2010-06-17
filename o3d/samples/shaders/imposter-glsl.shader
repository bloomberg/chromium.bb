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

// This Imposter shader rotates around Y to face the camera.
// It doesn't have any lighting.

uniform mat4 world;
uniform mat4 viewInverse;
uniform mat4 viewProjection;

// input parameters for our vertex shader
attribute vec4 position;
attribute vec2 texCoord0;

// input parameters for our pixel shader
// also the output parameters for our vertex shader
varying vec2 v_texcoord;

/**
 * Vertex Shader - vertex shader for phong illumination
 */
void main() {
  vec3 toCamera = normalize(viewInverse[3].xyz - world[3].xyz);
  vec3 yAxis = vec3(0, 1, 0);
  vec3 xAxis = cross(yAxis, toCamera);
  vec3 zAxis = cross(xAxis, yAxis);

  mat4 newWorld = mat4(
      vec4(xAxis, 0),
      vec4(yAxis, 0),
      vec4(xAxis, 0),
      world[3]);
  gl_Position = (viewProjection * newWorld) * position;
  v_texcoord = texCoord0;
}

// #o3d SplitMarker

uniform sampler2D texSampler0;
varying vec2 v_texcoord;

/**
 * Pixel Shader
 */
void main() {
  gl_FragColor = texture2D(texSampler0, v_texcoord);
}

// #o3d MatrixLoadOrder RowMajor
