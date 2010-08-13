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
 * Represents the rotating target that is the focus of each level.
 * @param {string} src The path to the json scene.
 * @constructor
 */
var Block = function(src) {
  this.src = src;
  this.load_();
};

/**
 * The URL of the scene.json file the target uses.
 * @type {string}
 */
Block.prototype.src = '';

/**
 * The top level transform that contains this block. Beneath this transform
 * sits a custom transform for intermediate scaling, then the geometry.
 * @type {o3d.Transform}
 */
Block.prototype.transform = null;

/**
 * A pack just for components belonging to this block.
 * @type {o3d.Pack}
 */
Block.prototype.pack = null;

/**
 * A pick manager that helps computes intersections between the block and
 * bubbles.
 *
 * @type {o3d.PickManager}
 */
Block.prototype.pickManager = null;

/**
 * Advances the block one time step.
 * @param {number} elapsedTime
 */
Block.prototype.step = function(elapsedTime) {
  // No op. Could potentially advance a clock to rotate the model.
};

/**
 * Returns the overridden effect that the model should use.
 * @return {o3d.Effect}
 */
Block.prototype.getOverrideEffect = function() {
  var shader = document.getElementById('blockShader').value;
  shader = shader.replace(/SIZE/g,
      g_game.level.bubbleManager_.bubbleQueue_.length);
  var effect = this.pack.createObject('Effect');
  effect.loadFromFXString(shader);
  return effect;
};

/**
 * Loads the scene.json file and adds it to the scene. Also binds the eye and
 * bubble position parameters.
 */
Block.prototype.load_ = function() {
  var that = this;
  this.pack = g_client.createPack();
  this.transform = this.pack.createObject('Transform');

  var parentTransform = this.transform;
  function callback(pack, parent, exception) {
    if (exception) {
      alert("Could not load: \n" + exception);
    } else {
      o3djs.pack.preparePack(that.pack, g_viewInfo);
      parent.parent = g_client.root;
      // Create the pick manager.
      that.pickManager = o3djs.picking.createPickManager(parent);
      that.pickManager.update();

      // TODO: Create a generic sampler with IO-loaded image texture that will
      // be used for all models.
      var samplers = pack.getObjectsByClassName('o3d.Sampler');
      var materials = pack.getObjectsByClassName('o3d.Material');
      for (var m = 0; m < materials.length; ++m) {
        var material = materials[m];
        var effect = that.getOverrideEffect();
        effect.createUniformParameters(material);
        material.effect = effect;
        if (material.getParam('myTexture')) {
          material.getParam('myTexture').value = samplers[0];
        }
        if (material.getParam('bubblePosition')) {
          material.getParam('bubblePosition').value = g_paramArray;
        }

        // TODO: Not all materials will use this parameter, or it may be
        // called something different.
        if (material.getParam('lightWorldPos')) {
          material.getParam('lightWorldPos').bind(g_lightWorldPosParam);
        }
      }

    }
  }

  // Inner transform for scaling and adjustment.
  var child = this.pack.createObject('Transform');
  child.parent = this.transform;

  try {
    o3djs.scene.loadScene(g_client, this.pack, child, this.src, callback);
  } catch (e) {
    alert("loading failed : " + e);
  }
};
