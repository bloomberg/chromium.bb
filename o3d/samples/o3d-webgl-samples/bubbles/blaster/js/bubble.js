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
 * A bubble object.
 * @param {Bubble.BubbleType} type
 * @param {o3d.Pack} pack
 * @constructor
 */
var Bubble = function(type, pack) {
  this.kind = type;
  this.position = [0, 0, 0];
  this.velocity = [0, 0, 0];
  this.alive = false;
  this.position = [2 * globals.playerRadius, 0, 0];
  this.param = pack.createObject('ParamFloat4');
  this.param.value = [0, 0, 0, 0];

  switch (this.kind) {
    case Bubble.SMALL:
      this.scale = 0.2;
      this.color = [0, 0.4, 0.7, 0.7];
      this.speed = 0.05;
      break;
    case Bubble.MEDIUM:
      this.scale = 0.3;
      this.color = [0, 0.6, 0.2, 0.7];
      this.speed = 0.08;
      break;
    case Bubble.LARGE:
      this.scale = 0.45; // This is the radius.
      this.color = [0.8, 0.2, 0.3, 0.7];
      this.speed = 0.1;
      break;
    default:
      throw 'Invalid type of Bubble';
  }

  this.transform = pack.createObject('Transform');
  if (!Bubble.defaultShape) {
    Bubble.createDefaultShape(pack);
  }
  this.transform.addShape(Bubble.defaultShape);
  this.transform.visible = false;
  this.transform.createParam('diffuse', 'ParamFloat4').value = this.color;
};


/**
 * BubbleType,
 * SMALL
 * MEDIUM
 * LARGE
 */
Bubble.SMALL = 0;
Bubble.MEDIUM = 1;
Bubble.LARGE = 2;

/**
 * The type of a bubble.
 * @type {number}
 */
Bubble.BubbleType = goog.typedef;

/**
 * The type of bubble.
 * @type {Bubble.BubbleType}
 */
Bubble.prototype.kind = -1;

/**
 * The bubble's position.
 * @type {!Array.<number>}
 */
Bubble.prototype.position = [];

/**
 * The bubble's velocity.
 * @type {!Array.<number>}
 */
Bubble.prototype.velocity = [];


/**
 * The bubble's color.
 * @type {!Array.<number>}
 */
Bubble.prototype.color = [1, 1, 1];

/**
 * The bubble's speed. Higher = faster.
 * @type {number}
 */
Bubble.prototype.speed = 1.0;

/**
 * The bubble's relative size.
 * @type {!Array.<number>}
 */
Bubble.prototype.scale = 1.0;

/**
 * If bubble is in bounds.
 * @type {boolean}
 */
Bubble.prototype.alive = false;

/**
 * The bubble's parent transform.
 * @type {o3d.Transform}
 */
Bubble.prototype.transform = null;

/**
 * A param that will pass this bubble's position and radius to the shader.
 * @type {o3d.ParamFloat4}
 */
Bubble.prototype.param = null;

/**
 * A shared primitive used by all the bubbles.
 * @type {o3d.Shape}
 */
Bubble.defaultShape = null;

/**
 * An initializer that creates the default shape.
 * @param {o3d.Pack} pack
 */
Bubble.createDefaultShape = function(pack) {
  var shader = o3djs.material.createBasicMaterial(
      pack,
      g_viewInfo,
      [1, 1, 1, 1],
      true);
  Bubble.defaultShape = o3djs.primitives.createSphere(pack, shader, 1, 20, 20);
};

/**
 * Initializes a bubble at given position. The velocity is inferred to be
 * towards the origin and is scaled based on this bubble's speed.
 *
 * @param {!Array.<number>} position
 */
Bubble.prototype.launch = function(position) {
  this.transform.visible = true;
  this.alive = true;
  this.position = position;
  this.velocity = g_math.mulScalarVector(this.speed,
      g_math.negativeVector(g_math.normalize(position)));
  this.updateTransforms_();
  this.transform.parent = g_client.root;
};


/**
 * Updates the transforms so the bubble draws in the correct position.
 * @private
 */
Bubble.prototype.updateTransforms_ = function() {
  this.transform.identity();
  this.transform.translate(this.position);
  this.transform.scale([this.scale, this.scale, this.scale]);
};

/**
 * Updates the values of the uniform param associated with this bubble.
 */
Bubble.prototype.updateParam = function() {
  this.param.value = [this.position[0], this.position[1], this.position[2],
      this.scale];
};

/**
 * Advances the bubble by one timestep to its new location.
 */
Bubble.prototype.step = function() {
  if (!this.alive) {
    return;
  }

  this.position[0] += this.velocity[0];
  this.position[1] += this.velocity[1];
  this.position[2] += this.velocity[2];
  this.updateParam();
  this.updateTransforms_();

  // Compute the point furthest from the origin along the velocity ray.
  var back = g_math.addVector(g_math.mulScalarVector(-this.scale,
      g_math.normalize(this.velocity)), this.position);
  var worldRay = {near: back, far: [0, 0, 0]};

  // Use picking to determine if the origin has intersected the block.
  var result = g_game.level.block.pickManager.pick(worldRay);
  if (result) {
    var ray = g_math.subVector(result.worldIntersectionPosition, back);
    if (g_math.length(ray) <= this.scale) {
      // Bubble intersected, so stop its movement.
      this.alive = false;
      this.transform.visible = false;
    }
  }
};
