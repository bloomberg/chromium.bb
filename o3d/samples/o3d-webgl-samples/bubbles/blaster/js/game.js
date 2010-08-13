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
 * Represents a game entity. There should be a single game object.
 * @constructor
 */
var Game = function() {
};

/**
 * Initializes the game object and populates its fields.
 * @param {o3d.Pack} pack The pack that this game should create objects in.
 */
Game.prototype.init = function(pack) {
  this.camera = new Camera(globals.fov, globals.playerRadius,
      g_viewInfo.drawContext);
  this.level = new Level(''); // TODO: This will eventually be a .json path.
  this.level.start();
  g_lightWorldPosParam.value = this.camera.eye;
  this.camera.updateView();
};

/**
 * The game's camera.
 * @type {Camera}
 */
Game.prototype.camera = null;

/**
 * The current level.
 * @type {Level}
 */
Game.prototype.level = null;

/**
 * An action handler to address key presses in the game.
 * @param {Event} event
 */
Game.prototype.onKeyPress = function(event) {
  event = event || window.event;
  if (event.metaKey)
    return;

  var keyChar = String.fromCharCode(o3djs.event.getEventKeyChar(event));
  keyChar = keyChar.toLowerCase();

  var dir;
  switch(keyChar) {
    case 'a':
      dir = Camera.LEFT;
      break;
    case 'd':
      dir = Camera.RIGHT;
      break;
    case 'w':
      dir = Camera.UP;
      break;
    case 's':
      dir = Camera.DOWN;
      break;
    case ' ':
      this.level.launchBubble();
      // fall through
    default:
      return;
  }
  this.camera.rotate(dir, globals.keyPressDelta);
  g_lightWorldPosParam.value = this.camera.eye;
};

/**
 * Ends the game and displays the score screen.
 * @param {string} amount Coverage achieved, as a string.
 */
Game.prototype.endGame = function(amount) {
  $("#final").text(amount);
  $("#finalResult").fadeIn()
};

/**
 * Steps the game forward one timestep (elapsedTime).
 * @param {number} elapsedTime The seconds passed since the last call.
 */
Game.prototype.step = function(elapsedTime) {
  this.level.step(elapsedTime);
};


/**
 * An action handler to address when the o3d container is resized.
 * @param {number} newWidth
 * @param {number} newHeight
 */
Game.prototype.onClientResize = function(newWidth, newHeight) {
  this.camera.updateProjection(newWidth, newHeight);
};
