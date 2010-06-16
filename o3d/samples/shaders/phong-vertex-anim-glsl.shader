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

// This shader expects to be passed vertices that are part of an x-z plane mesh
// because it assumes the normal for each vertex before being offset by the
// animation is 0, 1, 0.

uniform mat4 viewProjection;
uniform mat4 world;
uniform mat4 worldInverseTranspose;
uniform float time;

attribute vec4 position;

varying vec3 v_normal;
varying vec3 v_worldPosition;

void main() {
  vec3 worldPosition = (world * position).xyz;

  float animValue = time + (worldPosition.x + worldPosition.z) * 0.4;
  float animSin = sin(animValue);
  float animCos = cos(animValue);
  vec4 position = vec4(
      worldPosition.x,
      worldPosition.y + animSin,
      worldPosition.z,
      1);
  gl_Position = viewProjection * position;
  vec3 normalTmp = normalize(vec3(animCos, abs(animSin), animCos));
  v_normal = (worldInverseTranspose * vec4(normalTmp, 0)).xyz;
  v_worldPosition = position.xyz;
}

// #o3d SplitMarker

uniform vec3 lightWorldPos;
uniform vec4 lightIntensity;
uniform vec4 ambientIntensity;
uniform vec4 emissive;
uniform vec4 ambient;
uniform vec4 diffuse;
uniform vec4 specular;
uniform float shininess;
uniform float time;
uniform mat4 viewInverse;

varying vec3 v_normal;
varying vec3 v_worldPosition;

vec4 lit(float l ,float h, float m) {
  return vec4(1.0,
              max(l, 0.0),
              (l > 0.0) ? pow(max(0.0, h), m) : 0.0,
              1.0);
}

void main() {
  vec3 surfaceToLight = normalize(lightWorldPos - v_worldPosition);
  vec3 worldNormal = normalize(v_normal);
  vec3 surfaceToView = normalize(viewInverse[3].xyz - v_worldPosition);
  vec3 halfVector = normalize(surfaceToLight + surfaceToView);
  vec4 litResult = lit(dot(worldNormal, surfaceToLight),
                         dot(worldNormal, halfVector), shininess);
  vec4 outColor = ambientIntensity * ambient;
  outColor += lightIntensity * (diffuse * litResult.y +
      specular * litResult.z);
  outColor += emissive;
  gl_FragColor = vec4(outColor.rgb, diffuse.a);
}

// #o3d MatrixLoadOrder RowMajor
