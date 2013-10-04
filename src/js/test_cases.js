// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for the testing stuff.
 */
camera.test = camera.test || {};

/**
 * Namespace for the test cases.
 */
camera.test.cases = {};

/**
 * Checks if the window gets opened.
 * @param {function()} callback Completion callback.
 */
camera.test.cases.basic = function(callback) {
  camera.test.waitForTrue('Wait for the window.', function() {
    return !!camera.bg.appWindow;
  }, function() {
    camera.test.success('Window created.');
    callback();
  });
};

/**
 * Checks if the window gets opened and if the stream is available.
 * @param {function()} callback Completion callback.
 */
camera.test.cases.capture = function(callback) {
  var instance;
  camera.test.runSteps([
    function(next) {
      camera.test.waitForTrue('Wait for the window.', function() {
        return !!camera.bg.appWindow;
      }, next);
    },
    function(next) {
      camera.test.waitForTrue('Wait for the camera instance.', function() {
        return camera.bg.appWindow.contentWindow.camera &&
          camera.bg.appWindow.contentWindow.camera.Camera &&
          camera.bg.appWindow.contentWindow.camera.Camera.getInstance();
      }, next);
    },
    function(next) {
      instance = camera.bg.appWindow.contentWindow.camera.Camera.
            getInstance();
      camera.test.waitForTrue('Wait for the Camera view.', function() {
        return instance.currentView &&
               instance.currentView == instance.cameraView;
      }, next);
    },
    function(next) {
      camera.test.waitForTrue('Wait for the stream.', function() {
        return instance.currentView.capturing;
      }, callback);
    }
  ]);
};

/**
 * Checks if the stream is restored after the camera is gone for some time.
 * This will happen, when a chromebook is suspended.
 * @param {function()} callback Completion callback.
 */
camera.test.cases.restore = function(callback) {
  var instance;
  camera.test.runSteps([
    function(next) {
      camera.test.waitForTrue('Wait for the window.', function() {
        return !!camera.bg.appWindow;
      }, next);
    },
    function(next) {
      camera.test.waitForTrue('Wait for the camera instance.', function() {
        return camera.bg.appWindow.contentWindow.camera &&
          camera.bg.appWindow.contentWindow.camera.Camera &&
          camera.bg.appWindow.contentWindow.camera.Camera.getInstance();
      }, next);
    },
    function(next) {
      instance = camera.bg.appWindow.contentWindow.camera.Camera.
            getInstance();
      camera.test.waitForTrue('Wait for the Camera view.', function() {
        return instance.currentView &&
               instance.currentView == instance.cameraView;
      }, next);
    },
    function(next) {
      camera.test.waitForTrue('Wait for the stream.', function() {
        return instance.currentView.capturing;
      }, next);
    },
    function(next) {
      camera.test.command('detach', 'Detach the camera device.');
      camera.test.waitForTrue('Wait until the end of the stream.', function() {
        return !instance.currentView.capturing;
      }, next);
    },
    function(next) {
      camera.test.command('attach', 'Attach the camera device.');
      camera.test.waitForTrue('Wait for the stream.', function() {
        return instance.currentView.capturing;
      }, callback);
    }
  ]);
};

