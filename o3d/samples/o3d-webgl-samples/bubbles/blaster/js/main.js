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
o3djs.base.o3d = o3d;
o3djs.require('o3djs.webgl');
o3djs.require('o3djs.util');
o3djs.require('o3djs.math');
o3djs.require('o3djs.quaternions');
o3djs.require('o3djs.rendergraph');
o3djs.require('o3djs.primitives');
o3djs.require('o3djs.material');
o3djs.require('o3djs.pack');
o3djs.require('o3djs.arcball');
o3djs.require('o3djs.scene');
o3djs.require('o3djs.io');
o3djs.require('o3djs.picking');

/**
 * Some global settings.
 */
var globals = {
  playerRadius: 5,
  fov: 45,
  keyPressDelta: 0.1,
  bubbleCullDistance: 5,
};

/**
 * Some global objects.
 */
var g_root;
var g_o3d;
var g_client;
var g_viewInfo;
var g_game;
var g_pack;
var g_o3dHeight, g_o3dWidth;
var g_paramArray;

/**
 * Run once the body is loaded.
 */
function init() {
  o3djs.webgl.makeClients(init2);
}

function init2(clientElements) {
  var g_o3dElement = clientElements[0]; // A canvas element.
  g_o3d = g_o3dElement.o3d;
  g_client = g_o3dElement.client;

  g_client.normalizeClearColorAlpha = false;
  g_math = o3djs.math;

  g_pack = g_client.createPack();
  var rootPack = g_client.createPack();
  g_viewInfo = o3djs.rendergraph.createBasicView(
      rootPack,
      g_client.root,
      g_client.renderGraphRoot,
      [0, 0, 0, 0]);

  var paramObject = rootPack.createObject('ParamObject');
  g_clockParam = paramObject.createParam('time', 'ParamFloat');
  g_clockParam.value = 0;
  g_lightWorldPosParam =
      paramObject.createParam('lightWorldPos', 'ParamFloat4');
  g_lightWorldPosParam.value = [0, 0, 5];

  g_paramArray = rootPack.createObject('ParamArray');

  g_game = new Game();
  g_game.init(g_pack);
  checkClientSize();

  // Pass the key press to the game's handler.
  o3djs.event.addEventListener(g_o3dElement, 'keypress', function(e) {
    g_game.onKeyPress(e);
  });

  // Attach click handlers to the level links.
  $("#link-help").bind('click', function(e) {
    $("#finalResult").hide();
    showHelpMenu(true);
    return false;
  });

  $("#close-help").bind('click', function(e) {
    showHelpMenu(false);
    return false;
  });

  $("#link-restart").bind('click', function(e) {
    $("#finalResult").hide();
    $("#help").hide();
    g_game = new Game();
    g_game.init();
    var children = g_client.root.children();
    for (var i = 0; i < children.length; ++i) {
      children[i].parent = null; // remove from tree
    }
    return false;
  });

  g_client.setRenderCallback(onRender);
}

/**
 * Shows or hides the help menu.
 * @param doShow
 */
function showHelpMenu(doShow) {
  doShow ? $("#help").slideDown() : $("#help").slideUp();
}


/**
 *  Called every frame.
 */
function onRender(renderEvent) {
  g_clockParam.value += renderEvent.elapsedTime;
  g_game.step(renderEvent.elapsedTime);
  checkClientSize();
}

/**
 * Checks if the window size has changed and updates the matrices if so.
 * @return
 */
function checkClientSize() {
  var newWidth  = parseInt(g_client.width);
  var newHeight = parseInt(g_client.height);
  if (newWidth != g_o3dWidth || newHeight != g_o3dHeight) {
    g_o3dWidth = newWidth;
    g_o3dHeight = newHeight;
    g_game.onClientResize(newWidth, newHeight);
  }
}

/**
 * Removes any callbacks so they don't get called after the page has unloaded.
 */
function uninit() {
  if (g_client) {
    g_client.cleanup();
  }
}
