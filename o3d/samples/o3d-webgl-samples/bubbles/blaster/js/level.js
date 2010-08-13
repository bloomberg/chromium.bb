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
 * Represents a currently active level that is being played.
 * @param config
 */
var Level = function(config) {
  this.load_(config);
  this.pack_ = g_client.createPack();
};

/**
 * This level's bubble manager.
 * @type {BubbleManager}
 * @private
 */
Level.prototype.bubbleManager_ = null;

/**
 * This level's block.
 * @type {Block}
 */
Level.prototype.block = null;

/**
 * A pack for assets belonging to this level.
 * @type {o3d.Pack}
 * @private
 */
Level.prototype.pack_ = null;

/**
 * The current percent of surface area covered, as a percent value in the range
 * 0 to 100.
 * @type {number}
 * @private
 */
Level.prototype.progress_ = 0;

/**
 * The name of the level.
 * @type {string}
 */
Level.prototype.levelName = '';

/**
 * The level number.
 * @type {number}
 */
Level.prototype.levelNumber = 0;

/**
 * The sequence of bubbles alloted for this level.
 * @type {!Array.<Bubble.BubbleType>}
 */
Level.prototype.bubbles = [];

/**
 * The target coverage goal for this level, in the range [0, 100].
 * @type {number}
 */
Level.prototype.goal = -1;

/**
 * The path to the resource used to load the block model.
 * @type {string}
 */
Level.prototype.sceneSrc = '';

/**
 * Launches a bubble into the game, if possible. Ends the game if no bubbles
 * are remaining in the queue.
 */
Level.prototype.launchBubble = function() {
  var bubble = this.bubbleManager_.pop();
  if (bubble) {
    bubble.launch(g_game.camera.eye);
    // TODO: We arbitrarily advance the progress counter. This should later
    // compute the covered surface area and adjust appropriately.
    this.progress_ += Math.floor(Math.random() * 10);
    this.progress_ = Math.min(this.progress_, 99);
    $("#goal").css("width", this.progress_ + "%");
  } else {
    g_game.endGame($("#goal").css("width"));
  }
};

/**
 * Advances the level by a time duration of elapsedTime.
 * @param {number} elapsedTime The seconds passed since the last call.
 */
Level.prototype.step = function(elapsedTime) {
  this.bubbleManager_.step(elapsedTime);
  this.block.step(elapsedTime);
};

/**
 * Starts the level. Should be called after the configurations have loaded.
 */
Level.prototype.start = function() {
  $('#level-description').text(this.levelNumber + ' ' + this.levelName);
  this.bubbleManager_ = new BubbleManager(this.bubbles, this.pack_);
  this.block = new Block(this.sceneSrc);
};

/**
 * Loads a JSON-encoded configuration file and populates this level with its
 * values.
 * @param {string} config The path to the JSON file.
 * @private
 */
Level.prototype.load_ = function(config) {
  // TODO: This will eventually load config, a json path, and populate the
  // fields below.
  this.levelNumber = 12;
  this.levelName = 'Crate';
  this.bubbles = [Bubble.SMALL, Bubble.LARGE, Bubble.SMALL, Bubble.MEDIUM,
                  Bubble.SMALL, Bubble.LARGE, Bubble.SMALL, Bubble.MEDIUM,
                  Bubble.SMALL, Bubble.LARGE, Bubble.SMALL, Bubble.MEDIUM,
                  Bubble.SMALL, Bubble.LARGE, Bubble.SMALL, Bubble.MEDIUM,
                  Bubble.SMALL, Bubble.LARGE, Bubble.SMALL, Bubble.MEDIUM,
                  Bubble.SMALL, Bubble.LARGE, Bubble.SMALL, Bubble.MEDIUM];
  this.goal = 90;
  this.sceneSrc = "../../../simpleviewer/assets/cube/scene.json";
};
