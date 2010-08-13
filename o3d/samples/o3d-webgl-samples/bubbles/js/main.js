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
o3djs.require('o3djs.math');
o3djs.require('o3djs.rendergraph');
o3djs.require('o3djs.primitives');
o3djs.require('o3djs.quaternions');
o3djs.require('o3djs.arcball');

// Events
// Run the init() function once the page has finished loading.
//         unload() when the page is unloaded.
window.onload = init;
window.onunload = unload;

// Global variables.
var g_o3d;
var g_math;
var g_quaternions;
var g_client;
var g_o3dElement;
var g_viewInfo;
var g_pack;
var g_o3dWidth = -1;
var g_o3dHeight = -1;
var g_dataroot;
var g_camera = {};
var g_aball;
var g_demo;
var g_clock = null;
var g_dragging = false;
var g_eyeParam;
var g_controls;
var g_done = false;
var g_useCubeMap;
var g_blipDistortion;
var g_blendTwice;

/**
 * Initializes the o3d clients.
 */
function init() {
  o3djs.webgl.makeClients(initStep2);
}


/**
 * Initializes O3D and sets up an instance of BubbleDemo.
 * @param {Array} clientElements Array of o3d object elements.
 */
function initStep2(clientElements) {
  // Initializes global variables and libraries.
  g_o3dElement = clientElements[0];
  g_o3d = g_o3dElement.o3d;
  g_math = o3djs.math;
  g_client = g_o3dElement.client;

  // Create a g_pack to manage our resources/assets
  g_pack = g_client.createPack();
  g_quaternions = o3djs.quaternions;

  // Create the render graph for a view.
  g_viewInfo = o3djs.rendergraph.createBasicView(
      g_pack,
      g_client.root,
      g_client.renderGraphRoot,
      [0.93, 0.93, 0.93, 1]
  );

  // Set the clear color to the default set in the HTML input fields.
  updateClearColor();

  // Sort method for the draw pass.
  g_viewInfo.performanceDrawPass.sortMethod = g_o3d.DrawList.BY_Z_ORDER;

  // Set some blending states.
  var state = g_viewInfo.performanceDrawPassInfo.state;
  state.getStateParam('SourceBlendFunction').value = g_o3d.State.BLENDFUNC_ONE;
  state.getStateParam('DestinationBlendFunction').value =
      g_o3d.State.BLENDFUNC_SOURCE_ALPHA;
  state.getStateParam('BlendEquation').value = g_o3d.State.BLEND_ADD;
  state.getStateParam('SeparateAlphaBlendEnable').value = false;
  state.getStateParam('AlphaBlendEnable').value = true;

  state.getStateParam('ZEnable').value = false;
  state.getStateParam('ZWriteEnable').value = false;

  // By default, cull all the back faces.
  state.getStateParam('CullMode').value = g_o3d.State.CULL_CW;

  // Creates a transform to put our data in.
  g_dataroot = g_pack.createObject('Transform');
  g_dataroot.parent = g_client.root;

  // Create some of the uniforms for the shaders.
  var paramObject = g_pack.createObject('ParamObject');
  g_eyeParam = paramObject.createParam('eye', 'ParamFloat4');
  g_camera.target = [0, 0, 0];
  g_camera.eye = [0, 0, 5];
  updateCamera();
  // Create a timer/clock.
  g_clock = paramObject.createParam('timer', 'ParamFloat');
  g_clock.value = 0;
  // Use the environment map?
  g_useCubeMap = paramObject.createParam('useCubeMap', 'ParamBoolean');
  g_useCubeMap.value = true;
  // Should shader effect be amplified for more vibrant bubbles?
  g_blendTwice = paramObject.createParam('blendTwice', 'ParamBoolean');
  g_blendTwice.value = true;
  // Number, location and overall effect of the modulation map.
  g_blipDistortion = paramObject.createParam('distortion', 'ParamFloat');
  g_blipDistortion.value = bsh.Globals.kBlipDistortion;

  // Create an arcball for scene rotations.
  g_lastRot = g_math.matrix4.identity();
  g_thisRot = g_math.matrix4.identity();
  g_aball = o3djs.arcball.create(100, 100);
  setClientSize();

  // Create the demo - all the geometry and shaders get set up here.
  var shaderString = document.getElementById('shader').value;
  g_demo = new BubbleDemo(shaderString);
  buildControls();

  // Add event listeners.
  o3djs.event.addEventListener(g_o3dElement, 'mousedown', startDragging);
  o3djs.event.addEventListener(g_o3dElement, 'mousemove', drag);
  o3djs.event.addEventListener(g_o3dElement, 'mouseup', stopDragging);
  o3djs.event.addEventListener(g_o3dElement, 'wheel', scrollMe);

  g_client.setRenderCallback(onRender);
}


/**
 * Generates the projection matrix based on the size of the o3d plugin
 * and calculates the view-projection matrix.
 */
function onRender(event) {
  g_clock.value += (event.elapsedTime);
  g_demo.update();
  setClientSize();
  if (!g_done) {
    g_done = true;
    // Hide the loading label.
    $("#loading").slideUp(1000);
  }
}


/**
 * Checks to see if the o3d container size has changed, and if so, adjusts
 * the projection matrix accordingaly.
 */
function setClientSize() {
  var newWidth  = parseInt(g_client.width);
  var newHeight = parseInt(g_client.height);

  if (newWidth != g_o3dWidth || newHeight != g_o3dHeight) {
    g_o3dWidth = newWidth;
    g_o3dHeight = newHeight;

    // Update the projection matrix.
    g_viewInfo.drawContext.projection = g_math.matrix4.perspective(
        g_math.degToRad(45), g_o3dWidth / g_o3dHeight, 0.1, 1000);

    // Sets a new area size for arcball.
    g_aball.setAreaSize(g_o3dWidth, g_o3dHeight);
  }
}


/**
 * Called on mousedown to signify a drag.
 */
function startDragging(e) {
  g_lastRot = g_thisRot;
  g_aball.click([e.x, e.y]);
  g_dragging = true;
}


/**
 * Rotates the scene as the user is dragging the mouse.
 */
function drag(e) {
  if (g_dragging) {
    var rotationQuat = g_aball.drag([e.x, e.y]);
    var rot_mat = g_quaternions.quaternionToRotation(rotationQuat);
    g_thisRot = g_math.matrix4.mul(g_lastRot, rot_mat);

    var m = g_dataroot.localMatrix;
    g_math.matrix4.setUpper3x3(m, g_thisRot);
    g_dataroot.localMatrix = m;
  }
}


/**
 * Called on mouseup.
 */
function stopDragging(e) {
  g_dragging = false;
}


/**
 * Zooms in and out of the scene on scrollwheel event.
 */
function scrollMe(e) {
  var t = (e.deltaY > 0) ? 11 / 12 : 13 /12;
  if (e.deltaY) {
    var t = (e.deltaY > 0) ? 11 / 12 : 13 /12;
    g_camera.eye = g_math.lerpVector(g_camera.target, g_camera.eye, t);
    updateCamera();
  }
}


/**
 * Updates the view matrix. Should be called whenever the camera position or
 * target has changed.
 */
function updateCamera() {
  var up = [0, 1, 0];
  g_viewInfo.drawContext.view = g_math.matrix4.lookAt(g_camera.eye,
                                                      g_camera.target,
                                                      up);
  g_eyeParam.value = g_camera.eye;
}


/**
 * Removes any callbacks so they don't get called after the page has unloaded.
 */
function unload() {
  if (g_client) {
    g_client.cleanup();
  }
}
