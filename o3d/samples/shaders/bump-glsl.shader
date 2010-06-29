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
attribute vec4 tangent;
attribute vec4 binormal;
attribute vec2 texCoord0;

// input parameters for our pixel shader
// also the output parameters for our vertex shader
varying vec2 v_texCoord;
varying vec3 v_normal;
varying vec3 v_binormal;
varying vec3 v_tangent;
varying vec3 v_worldPosition;

void main() {

  // Transform position into clip space.
  gl_Position = worldViewProjection * position;

  // Transform the tangent frame into world space.
  v_tangent = (worldInverseTranspose * tangent).xyz;
  v_binormal = (worldInverseTranspose * binormal).xyz;
  v_normal = (worldInverseTranspose * normal).xyz;

  // Pass through the texture coordinates.
  v_texCoord = texCoord0;

  // Calculate surface position in world space. Used for lighting.
  v_worldPosition = (world * position).xyz;
}

// #o3d SplitMarker

// whether to use texture
uniform float useTexture;

uniform mat4 viewInverse;

uniform vec3 lightWorldPos;
uniform vec4 lightIntensity;
uniform vec4 ambientIntensity;
uniform vec4 emissive;
uniform vec4 ambient;
uniform vec4 diffuse;
uniform vec4 specular;
uniform float shininess;

uniform sampler2D AmbientSampler;
uniform sampler2D DiffuseSampler;
uniform sampler2D BumpSampler;

varying vec2 v_texCoord;
varying vec3 v_normal;
varying vec3 v_binormal;
varying vec3 v_tangent;
varying vec3 v_worldPosition;

vec4 lit(float l ,float h, float m) {
  return vec4(1.0,
              max(l, 0.0),
              (l > 0.0) ? pow(max(0.0, h), m) : 0.0,
              1.0);
}

void main() {
  // Construct a transform from tangent space into world space.
  mat3 tangentToWorld = mat3(v_tangent,
                             v_binormal,
                             v_normal);

  // Read the tangent space normal from the normal map and remove the bias so
  // they are in the range [-0.5,0.5]. There is no need to scale by 2 because
  // the vector will soon be normalized.
  vec3 tangentNormal = texture2D(BumpSampler, v_texCoord.xy).xyz -
      vec3(0.5, 0.5, 0.5);

  // Transform the normal into world space.
  vec3 worldNormal =(tangentToWorld * tangentNormal);
  worldNormal = normalize(worldNormal);

  // Read the diffuse and ambient colors.
  vec4 textureAmbient = vec4(1, 1, 1, 1);
  vec4 textureDiffuse = vec4(1, 1, 1, 1);
  if (useTexture == 1.0) {
    textureAmbient = texture2D(AmbientSampler, v_texCoord.xy);
    textureDiffuse = texture2D(DiffuseSampler, v_texCoord.xy);
  }

  // Apply lighting in world space in case the world transform contains scaling.
  vec3 surfaceToLight = normalize(lightWorldPos.xyz -
      v_worldPosition.xyz);
  vec3 surfaceToView = normalize(viewInverse[3].xyz - v_worldPosition);
  vec3 halfVector = normalize(surfaceToLight + surfaceToView);
  vec4 litResult = lit(dot(worldNormal, surfaceToLight),
                         dot(worldNormal, halfVector), shininess);
  vec4 outColor = ambientIntensity * ambient * textureAmbient;
  outColor += lightIntensity * (diffuse * textureDiffuse * litResult.y +
      specular * litResult.z);
  outColor += emissive;
  gl_FragColor = vec4(outColor.rgb, diffuse.a * textureDiffuse.a);
}

// #o3d MatrixLoadOrder RowMajor
