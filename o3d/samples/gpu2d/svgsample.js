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
 * @fileoverview Framework for samples that load and display SVG files.
 *
 * This is purely *example* code, showing how to use the SVG loader.
 */

o3djs.require('o3djs.cameracontroller');
o3djs.require('o3djs.event');
o3djs.require('o3djs.gpu2d');
o3djs.require('o3djs.math');
o3djs.require('o3djs.picking');
o3djs.require('o3djs.rendergraph');
o3djs.require('o3djs.util');
// Also requires the following files to be loaded by the containing page:
//   - svgloader.js
//   - ../third_party/xmljs/tinyxmlsax.js

// Globals
var g_filename;
var g_o3d;
var g_math;
var g_client;
var g_pack;
var g_viewInfo;
var g_o3dElement;
var g_finished = false;  // for selenium testing.
var g_clientWidth;
var g_clientHeight;
var g_cameraController;

if (!("console" in window)) {
  var logElement = document.getElementById('log');
  window.console =
      { log: function(s) {
               if (logElement) {
                   logElement.innerHTML = logElement.innerHTML + "<span>" +
                       s.toString() + "</span><br>";
               }
             }
      };
}

/**
 * Initializes the sample with the given URL.
 * @param {string} filename The URL of the SVG file to load.
 */
function init(filename) {
  g_filename = filename;
  o3djs.util.makeClients(initStep2);
}

/**
 * Remove any callbacks so they don't get called after the page has unloaded.
 */
function unload() {
  if (g_client) {
    g_client.cleanup();
  }
}

/**
 * Completes initialization of the sample.
 * @param {!Array.<!Element>} clientElements Array of o3d object elements.
 */
function initStep2(clientElements) {
  // Initializes global variables and libraries.
  var o3dElement = clientElements[0];
  g_o3dElement = o3dElement;
  g_o3d = o3dElement.o3d;
  g_math = o3djs.math;
  g_client = o3dElement.client;

  // Creates a pack to manage our resources/assets
  g_pack = g_client.createPack();

  g_viewInfo = o3djs.rendergraph.createBasicView(
      g_pack,
      g_client.root,
      g_client.renderGraphRoot);

  // Set the background color to light gray.
  g_viewInfo.clearBuffer.clearColor = [0.8, 0.8, 0.8, 1];

  // Load the file.
  var loader = new SVGLoader();
  loader.load(g_filename,
              true,
              g_pack,
              g_viewInfo.zOrderedDrawList,
              g_client.root,
              function(url, success, detail) {
                if (!success) {
                  window.console.log('Failed to load ' + url + ": " + detail);
                } else {
                  var tmpManager =
                      o3djs.picking.createPickManager(g_client.root);
                  tmpManager.update();
                  var bbox = tmpManager.getTransformInfo(g_client.root).
                      getBoundingBox();
                  g_cameraController.viewAll(bbox,
                                             g_client.width / g_client.height);
                  updateViewAndProjectionMatrices();
                }
              });

  // Set up the view and projection transformations.
  initContext();

  // Set up event handlers for mouse interaction.
  o3djs.event.addEventListener(o3dElement, 'mousedown', onMouseDown);
  o3djs.event.addEventListener(o3dElement, 'mousemove', onMouseMove);
  o3djs.event.addEventListener(o3dElement, 'mouseup', onMouseUp);

  g_finished = true;  // for selenium testing.
}

/**
 * Event handler that gets called when a mouse click takes place in
 * the O3D element. It changes the state of the camera controller
 * based on which modifier keys are pressed.
 * @param {!Event} e The mouse down event.
 */
function onMouseDown(e) {
  if (e.button == 0) {
    if (!e.shiftKey && !e.ctrlKey && !e.metaKey && !e.altKey) {
      g_cameraController.setDragMode(
        o3djs.cameracontroller.DragMode.MOVE_CENTER_IN_VIEW_PLANE, e.x, e.y);
    } else if (e.metaKey || e.altKey) {
      g_cameraController.setDragMode(
        o3djs.cameracontroller.DragMode.SPIN_ABOUT_CENTER, e.x, e.y);
    } else if (!e.shiftKey && e.ctrlKey) {
      g_cameraController.setDragMode(
        o3djs.cameracontroller.DragMode.DOLLY_IN_OUT, e.x, e.y);
    } else if (e.shiftKey && !e.ctrlKey) {
      g_cameraController.setDragMode(
        o3djs.cameracontroller.DragMode.ZOOM_IN_OUT, e.x, e.y);
    } else if (e.shiftKey && e.ctrlKey) {
      g_cameraController.setDragMode(
        o3djs.cameracontroller.DragMode.DOLLY_ZOOM, e.x, e.y);
    }
  }
}

/**
 * Event handler that gets called when a mouse move event takes place
 * in the O3D element. It tells the camera controller that the mouse
 * has moved.
 * @param {!Event} e The mouse move event.
 */
function onMouseMove(e) {
  g_cameraController.mouseMoved(e.x, e.y);
}

/**
 * Event handler that gets called when a mouse up event takes place in
 * the O3D element. It tells the camera controller that the mouse has
 * been released.
 * @param {!Event} e The mouse up event.
 */
function onMouseUp(e) {
  g_cameraController.setDragMode(
      o3djs.cameracontroller.DragMode.NONE, e.x, e.y);
}

/**
 * Sets up reasonable view and projection matrices.
 */
function initContext() {
  // Set up our CameraController.
  g_cameraController = o3djs.cameracontroller.createCameraController(
      [0, 0, 0],            // centerPos
      500,                  // backpedal
      0,                    // heightAngle
      0,                    // rotationAngle
      g_math.degToRad(15),  // fieldOfViewAngle
      updateViewAndProjectionMatrices); // opt_onChange
  g_cameraController.distancePerUnit = 100.0;

  updateViewAndProjectionMatrices();
}

/**
 * Updates the view and projection matrices.
 */
function updateViewAndProjectionMatrices() {
  g_viewInfo.drawContext.view = g_cameraController.calculateViewMatrix();

  // Set up a perspective transformation for the projection.
  g_viewInfo.drawContext.projection = g_math.matrix4.perspective(
      g_cameraController.fieldOfViewAngle * 2,  // Frustum angle.
      g_o3dElement.clientWidth / g_o3dElement.clientHeight, // Aspect ratio.
      1,                                        // Near plane.
      5000);                                    // Far plane.
}

