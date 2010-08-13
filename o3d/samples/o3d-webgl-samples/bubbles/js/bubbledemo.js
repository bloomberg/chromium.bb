/*
 * Copyright 2010, Google Inc.
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

/**
 * A namespace to contain all the BubbleShader related methods and functions.
 */
var bsh = bsh || {};

/**
 * Define pi globally.
 * @type {number}
 */
var kPi = 3.14159265359;

/**
 * Generates a random number between min and max.
 * @param {number} min
 * @param {number} max
 * @return {number}
 */
function randf(min, max) {
  return min + (Math.random() * (max - min));
}

/**
 * Clamps a decimal value to [0, 1] and then converts it to range [0, 255].
 * @param {number} x Input value.
 * @return {number}
 */
bsh.clamp = function(x) {
  x = Math.min(1.0, Math.max(0.0, x));
  return Math.floor(x == 1.0 ? 255 : x * 256.0);
}

/**
 * Applies a cubic Hermite interpolation: x^2 * (3 - 2x)
 * @param {number} x
 * @return {number}
 */
bsh.smoothStep = function(x) {
  return (3.0 - (2.0 * x)) * x * x;
}

/**
 * Does a linear interpolation between a and b at time t.
 * @param {number} t
 * @param {number} a
 * @param {number} b
 * @return {number}
 */
bsh.lerp = function(t, a, b) {
  return a + ((b - a) * t);
}

/**
 * Samples a canvas at the given (u, v) coordinates and returns the red value
 * of that pixel as a float in range [0, 1]. Wraps the texture coordinates (u,v)
 * if they exceed the range [0, 1].
 * @param {Canvas} canvas The canvas to sample from.
 * @param {number} u The u texture coordinate.
 * @param {number} v The v texture coordinate.
 */
bsh.Sample = function(canvas, u, v) {
 var width = canvas.width;
 var height = canvas.height;
 var x = Math.floor((u % 1.0) * width);
 var y = Math.floor((v % 1.0) * height);
 var pixel = canvas.getContext('2d').getImageData(x, y, 1, 1);
 return pixel.data[0] / 255; // Return the red value of the pixel in [0, 1].
}

/**
 * Computes the vertices and indices for a sphere shape. The vertices and
 * indices array will be overwritten with the computed values. Assumes that all
 * bubbles share the same sphere shape.
 *
 * @param {number} rows Number of rows to subdivide the sphere in to.
 * @param {number} cols Number of cols to subdivide the sphere in to.
 * @param {Canvas} canvas A canvas to sample modulation from.
 * @param {!Array.<Object>} vertices Array of vertices (as objects).
 * @param {!Array.<number>} indices The index buffer array for the triangles.
 */
bsh.MakeSphereCoordinates = function(rows, cols, canvas, vertices, indices) {
  var counter = 0;
  for (var y = 0; y < rows + 1; ++y) {
    var phi = y * kPi / rows;
    var y1 = Math.cos(phi);
    var r = Math.sin(phi);
    for (var x = 0; x < cols + 1; ++x) {
      var theta = x * 2.0 * kPi / cols;
      var x1 = Math.cos(theta) * r;
      var z1 = Math.sin(theta) * r;
      var index = x + y * (cols + 1);
      var vertex = {};
      vertices.push(vertex);
      vertex.x = x1;
      vertex.y = y1;
      vertex.z = z1;
      vertex.nx = x1;
      vertex.ny = y1;
      vertex.nz = z1;
      vertex.u = x * 1.0 / cols;
      vertex.v = y * 1.0 / rows;
      vertex.mod = bsh.Sample(canvas, vertex.u, vertex.v);

      if (x != cols && y != rows) {
        // For each vertex, we add indices for 2 triangles representing the
        // quad using this vertex as the upper left corner:
        // [(x, y), (x+1, y), (x+1, y+1)] and
        // [(x, y), (x+1, y+1), (x, y+1)].
        // Note that we don't create triangles for the last row and the last
        // column of vertices.
        indices[counter++] = index;
        indices[counter++] = index + 1;
        indices[counter++] = index + cols + 2;
        indices[counter++] = index;
        indices[counter++] = index + cols + 2;
        indices[counter++] = index + cols + 1;
      }
    }
  }
}

/**
 * Globals values.
 * @type {Object}
 */
bsh.Globals = {
   kRows: 50,
   kCols: 100,
   kTexWidth: 512,
   kTexHeight: 512,
   kRefractionIndex: 1.33,
   kBubbleCount: 10,
   kRedWavelength: 680,
   kGreenWavelength: 530,
   kBlueWavelength: 440,
   kMaxWavelength: 680,
   kFrequency: 8,
   kNoiseRatio: [0.2, 0.5],
   kBubbleThickness: [0.3, 0.6],
   kBubbleScale: [0.5, 2.0],
   kBubbleSpin: 0.05,
   kNumBlips: 4,
   kBlipSize: [150, 200],
   kBlipDistortion: 0.25,
   kMaxBlips: 15
};

/**
 * An instance of the demo.
 * @param {string} shaderString The bubble's shader string.
 */
var BubbleDemo = function(shaderString) {
  this.indices_ = [];
  this.vertices_ = [];

  var g = bsh.Globals;

  // Make the modulation canvas before making sphere coordinates.
  this.mod_source = new bsh.Modulation(g.kTexWidth, g.kTexHeight, g.kBlipSize,
      g.kNumBlips);

  bsh.MakeSphereCoordinates(g.kRows, g.kCols, this.mod_source.canvas,
      this.vertices_, this.indices_);

  var noise_sampler = this.makeNoiseTexture_();
  var iridescence_sampler = this.makeIridescenceTexture_();
  var cubemap = this.makeCubeMap_();

  var frontMaterial = this.createMaterial_(shaderString, noise_sampler,
      iridescence_sampler, cubemap);

  // This material will cull the front faces instead of the back.
  var backMaterial = this.createMaterial_(shaderString, noise_sampler,
      iridescence_sampler, cubemap);
  backMaterial.state = g_pack.createObject('State');
  backMaterial.state.getStateParam('CullMode').value = g_o3d.State.CULL_CCW;

  this.frontShape = this.makeShape_(frontMaterial);
  this.backShape = this.makeShape_(backMaterial);
  this.bubbles_ = [];
  this.regenerateBubbles();
}

/**
 * Creates a material with the given effect and assigns its uniform parameters.
 * @param {string} shaderString The shader source.
 * @param {o3d.Sampler} noise_sampler
 * @param {o3d.Sampler} iridescence_sampler
 * @param {o3d.TextureCUBE} cubemap
 * @return {o3d.Material}
 * @private
 */
BubbleDemo.prototype.createMaterial_ = function(shaderString, noise_sampler,
    iridescence_sampler, cubemap) {
  var effect = g_pack.createObject('Effect');
  effect.loadFromFXString(shaderString);
  var material = g_pack.createObject('Material');
  material.drawList = g_viewInfo.performanceDrawList;
  material.effect = effect;
  effect.createUniformParameters(material);

  material.getParam("noise_sampler").value = noise_sampler;
  material.getParam("iridescence_sampler").value = iridescence_sampler;
  material.getParam("env_sampler").value = cubemap;
  material.getParam("eye").bind(g_eyeParam);
  material.getParam("timer").bind(g_clock);
  material.getParam("useCubeMap").bind(g_useCubeMap);
  material.getParam("blendTwice").bind(g_blendTwice);
  material.getParam("distortion").bind(g_blipDistortion);
  return material;
}

/**
 * Creates the perlin noise texture.
 * @return {o3d.Sampler}
 * @private
 */
BubbleDemo.prototype.makeNoiseTexture_ = function() {
  var g = bsh.Globals;
  this.noise_source = new bsh.Perlin(g.kFrequency,
      98576, // This is supposed to be a random \cough seed.
      g.kTexWidth,
      g.kTexHeight);
  this.noise_texture = g_pack.createTexture2D(g.kTexWidth,
      g.kTexHeight,
      g_o3d.Texture.ARGB8,
      1,
      false);
  this.noise_source.refresh(this.noise_texture);
  var noise_sampler = g_pack.createObject('Sampler');
  noise_sampler.name = "noise";
  noise_sampler.addressModeU = g_o3d.Sampler.WRAP;
  noise_sampler.addressModeV = g_o3d.Sampler.WRAP;
  noise_sampler.addressModeW = g_o3d.Sampler.WRAP;
  noise_sampler.magFilter = g_o3d.Sampler.LINEAR;
  noise_sampler.minFilter = g_o3d.Sampler.LINEAR;
  noise_sampler.mipFilter = g_o3d.Sampler.NONE;
  noise_sampler.maxAnisotropy = 1;
  noise_sampler.texture = this.noise_texture;
  return noise_sampler;
}

/**
 * Creates the iridescence texture.
 * @return {o3d.Sampler}
 * @private
 */
BubbleDemo.prototype.makeIridescenceTexture_ = function() {
  var g = bsh.Globals;
  this.iridescence_source = new bsh.Iridescence(g.kTexWidth,
      g.kTexHeight,
      g.kRefractionIndex,
      g.kMaxWavelength);
  this.iridescence_texture = g_pack.createTexture2D(g.kTexWidth,
      g.kTexHeight,
      g_o3d.Texture.ARGB8,
      1,
      false);
  this.iridescence_source.refresh(this.iridescence_texture);
  var iri_sampler = g_pack.createObject('Sampler');
  iri_sampler.name = "iridescence";
  iri_sampler.addressModeU = g_o3d.Sampler.CLAMP;
  iri_sampler.addressModeV = g_o3d.Sampler.CLAMP;
  iri_sampler.addressModeW = g_o3d.Sampler.CLAMP;
  iri_sampler.magFilter = g_o3d.Sampler.LINEAR;
  iri_sampler.minFilter = g_o3d.Sampler.LINEAR;
  iri_sampler.mipFilter = g_o3d.Sampler.NONE;
  iri_sampler.maxAnisotropy = 1;
  iri_sampler.texture = this.iridescence_texture;
  return iri_sampler;
}


/**
 * Creates a cube/environment map.
 * @return {o3d.TextureCUBE}
 * @private
 */
BubbleDemo.prototype.makeCubeMap_ = function() {
  this.env_source = new bsh.Environment($("#tex-environment img"));
  return this.env_source.texture;
}


/**
 * A list of indices used to create geometry.
 * @type {!Array.<number>}
 * @private
 */
BubbleDemo.prototype.indices_ = [];

/**
 * A list of vertices used to create geometry.
 * @type {!Array.<number>}
 * @private
 */
BubbleDemo.prototype.vertices_ = [];

/**
 * The noise texture source.
 * @type {bsh.Perlin}
 */
BubbleDemo.prototype.noise_source = null;

/**
 * The noise texture object.
 * @type {o3d.Texture}
 */
BubbleDemo.prototype.noise_texture = null;

/**
 * The iridescence texture source.
 * @type {bsh.Iridescence}
 */
BubbleDemo.prototype.iridescence_source = null;

/**
 * The iridescence texture object.
 * @type {o3d.Texture}
 */
BubbleDemo.prototype.iridescence_texture = null;

/**
 * The modulation texture source.
 * @type {bsh.Modulation}
 */
BubbleDemo.prototype.mod_source = null;

/**
 * The environment map source.
 * @type {bsh.Environment}
 */
BubbleDemo.prototype.env_source = null;

/**
 * A list of bubbles in the demo.
 * @type {!Array.<Object>}
 */
BubbleDemo.prototype.bubbles_ = [];

/**
 * The shape object shared by all bubble instances.
 * @type {o3d.Shape}
 */
BubbleDemo.prototype.shape = null;

/**
 * An array of streambanks backing the shape primitives used.
 * @type {!Array.<o3d.StreamBank>}
 */
BubbleDemo.prototype.streamBanks = [];


/**
 * Updates the iridescence texture using the latest global parameters.
 */
BubbleDemo.prototype.regenerateIridescence = function() {
  var g = bsh.Globals;
  this.iridescence_source.init(g.kRefractionIndex, g.kMaxWavelength);
  this.iridescence_source.refresh(this.iridescence_texture);
}

/**
 * Updates the noise texture using the latest global parameters.
 */
BubbleDemo.prototype.regenerateNoise = function() {
  var g = bsh.Globals;
  var seed = Math.floor(new Date().getTime() % 100000);
  this.noise_source.init(g.kFrequency, seed);
  this.noise_source.refresh(this.noise_texture);
}

/**
 * Updates the modulation vertex stream using the latest global parameters.
 */
BubbleDemo.prototype.regenerateModulation = function() {
  var g = bsh.Globals;
  this.mod_source.init(g.kBlipSize, g.kNumBlips);

  // Update the stream to sample from the new texture.
  var newModulationCoords = [];
  for (var i = 0; i < this.vertices_.length; ++i) {
    var vertex = this.vertices_[i];
    var value = bsh.Sample(this.mod_source.canvas, vertex.u, vertex.v);
    vertex.mod = value;
    newModulationCoords.push(value);
  }

  for (var i = 0; i < this.streamBanks.length; i++) {
    var modulationStream =
        this.streamBanks[i].getVertexStream(g_o3d.Stream.TEXCOORD, 1);
    modulationStream.field.setAt(0, newModulationCoords);
  }
}

/**
* Toggles whether bubbles are rendered with a second thin-film layer, making
* it easier to see their surface.
* @param {boolean} doRender True if bubbles should be rendered with a duplicate
*   blending effect.
*/
BubbleDemo.prototype.renderThickerBubbles = function(doRender) {
  g_blendTwice.value = doRender;
}

/**
 * Updates the set of bubbles to use the latest global parameters.
 */
BubbleDemo.prototype.regenerateBubbles = function() {
  var g = bsh.Globals;
  var i = 0;

  // Update existing bubbles and create new ones if necessary.
  for (i = 0; i < g.kBubbleCount; ++i) {
    var bubble = this.bubbles_[i] || {};
    bubble.position = [randf(-6, 6), randf(-6, 6), randf(-6, 6)];
    bubble.rotation_speed = [randf(-g.kBubbleSpin, g.kBubbleSpin),
                             randf(-g.kBubbleSpin, g.kBubbleSpin),
                             randf(-g.kBubbleSpin, g.kBubbleSpin)];
    bubble.scale = randf(g.kBubbleScale[0], g.kBubbleScale[1]);

    var max_thickness = randf(g.kBubbleThickness[0], g.kBubbleThickness[1]);
    var min_thickness = randf(g.kBubbleThickness[0], max_thickness);

    // thickness = base * e^(-y*falloff) for y in [-scale..scale].
    bubble.thickness_falloff =
        Math.log(max_thickness / min_thickness) / (2 * bubble.scale);
    bubble.base_thickness =
        max_thickness * Math.exp(-bubble.scale * bubble.thickness_falloff);
    bubble.noise_ratio = randf(g.kNoiseRatio[0], g.kNoiseRatio[1]);

    if (!bubble.transform) {
      // The parent transform for this one bubble.
      bubble.transform = g_pack.createObject("Transform");
      bubble.transform.name = "Bubble" + i;

      // One draw for back face, another for front.
      bubble.back = g_pack.createObject("Transform");
      bubble.back.createParam('thickness_params', 'ParamFloat4');
      bubble.back.parent = bubble.transform;
      bubble.back.addShape(this.backShape);

      bubble.front = g_pack.createObject("Transform");
      bubble.front.createParam('thickness_params', 'ParamFloat4');
      bubble.front.parent = bubble.transform;
      bubble.front.addShape(this.frontShape);

      // This adds a little modulation so that the bubbles grow/shrink at
      // different rates and times.
      var bNumber = kPi * i * 0.5;
      bubble.back.createParam('bubbleNumber', 'ParamFloat').value = bNumber;
      bubble.front.createParam('bubbleNumber', 'ParamFloat').value = bNumber;
    }

    bubble.transform.visible = true;
    this.bubbles_[i] = bubble;
    this.updateParams_(bubble);
  }

  // Hide the rest of the bubbles.
  for (i; i < this.bubbles_.length; ++i) {
    this.bubbles_[i].transform.visible = false;
  }
}

/**
 * Updates the uniform parameters for a bubble.
 * @param {Object} bubble
 * @private
 */
BubbleDemo.prototype.updateParams_ = function(bubble) {
  bubble.back.getParam('thickness_params').value =
    [bubble.thickness_falloff, bubble.base_thickness, bubble.noise_ratio, 0.5];
  bubble.front.getParam('thickness_params').value =
    [bubble.thickness_falloff, bubble.base_thickness, bubble.noise_ratio, 1.0];
}

/**
 * Creates a bubble shape that uses the material. Also adds to the streambank
 * variable the bank that was used for the primitive that was created.
 * @param {o3d.Material} material
 * @return {o3d.Shape}
 * @private
 */
BubbleDemo.prototype.makeShape_ = function(material) {
  var shape = g_pack.createObject('Shape');
  var shapePrimitive = g_pack.createObject('Primitive');
  var streamBank = g_pack.createObject('StreamBank');

  shapePrimitive.owner = shape;
  shapePrimitive.material = material;
  shapePrimitive.streamBank = streamBank;
  this.streamBanks.push(streamBank);

  var g = bsh.Globals;
  shapePrimitive.primitiveType = g_o3d.Primitive.TRIANGLELIST;
  shapePrimitive.numberPrimitives = this.indices_.length / 3;
  shapePrimitive.numberVertices = this.vertices_.length;
  shapePrimitive.createDrawElement(g_pack, null);

  var positionArray = [];
  var normalArray = [];
  var uvArray = [];
  var modulationArray = [];

  for (var i = 0; i < this.vertices_.length; ++i) {
    var vertex = this.vertices_[i];
    positionArray.push(vertex.x, vertex.y, vertex.z);
    normalArray.push(vertex.nx, vertex.ny, vertex.nz);
    uvArray.push(vertex.u, vertex.v);
    modulationArray.push(vertex.mod);
  }

  var indicesArray = this.indices_;

  // Create buffers containing the vertex data.
  var positionsBuffer = g_pack.createObject('VertexBuffer');
  var positionsField = positionsBuffer.createField('FloatField', 3);
  positionsBuffer.set(positionArray);

  var texCoordsBuffer = g_pack.createObject('VertexBuffer');
  var texCoordsField = texCoordsBuffer.createField('FloatField', 2);
  texCoordsBuffer.set(uvArray);

  var modulationBuffer = g_pack.createObject('VertexBuffer');
  var modulationField = modulationBuffer.createField('FloatField', 1);
  modulationBuffer.set(modulationArray);

  var normalBuffer = g_pack.createObject('VertexBuffer');
  var normalField = normalBuffer.createField('FloatField', 3);
  normalBuffer.set(normalArray);

  var indexBuffer = g_pack.createObject('IndexBuffer');
  indexBuffer.set(indicesArray);

  // Add buffers to the StreamBank.
  streamBank.setVertexStream(
      g_o3d.Stream.POSITION,
      0,
      positionsField,
      0);
  streamBank.setVertexStream(
      g_o3d.Stream.TEXCOORD,
      0,
      texCoordsField,
      0);
  streamBank.setVertexStream(
      g_o3d.Stream.TEXCOORD,
      1,
      modulationField,
      0);
  streamBank.setVertexStream(
      g_o3d.Stream.NORMAL,
      0,
      normalField,
      0);
  shapePrimitive.indexBuffer = indexBuffer;
  return shape;
}

/**
 * Updates all the bubbles managed by this demo object.
 */
BubbleDemo.prototype.update = function() {
  for (var i = 0; i < this.bubbles_.length; ++i) {
    this.updateBubble_(this.bubbles_[i]);
  }
}

/**
 * Updates the position and scale of a single bubble.
 * @param {Object} bubble
 * @private
 */
BubbleDemo.prototype.updateBubble_ = function(bubble) {
  if (!bubble.transform.parent) {
    bubble.transform.parent = g_dataroot;
  }

  // TODO: potentially do a priority-order pass and manually sort the bubbles
  // to ensure back face is drawn prior to front?

  bubble.transform.identity();
  var rotation = bubble.rotation_speed;
  bubble.transform.rotateX(rotation[0] * 2 * kPi * g_clock.value);
  bubble.transform.rotateY(rotation[1] * 2 * kPi * g_clock.value);
  bubble.transform.rotateZ(rotation[2] * 2 * kPi * g_clock.value);
  bubble.transform.scale(bubble.scale, bubble.scale, bubble.scale);
  bubble.transform.translate(bubble.position);
}
