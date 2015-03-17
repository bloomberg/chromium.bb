// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

/**
 * @param {number} width
 * @param {number} height
 * @return {{width:number, height:number}}
 */
function size(width, height) {
  return {width: width, height: height};
}

/**
 * @param {number} x
 * @param {number} y
 * @return {{x:number, y:number}}
 */
function dpi(x, y) {
  return {x: x, y: y};
}

QUnit.module('DesktopViewport');

QUnit.test('choosePluginSize() handles low-DPI client & host',
  function(assert) {
    // 1. Client & host size the same.
    var pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 1.0, size(640, 480), dpi(96, 96), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, 480));

    // 2. Client logical dimensions smaller than host's.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 1.0, size(1024, 600), dpi(96, 96), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, (640 / 1024) * 600));

    // 3. Client Y dimension larger than host's, X dimension smaller.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 640), 1.0, size(1024, 600), dpi(96, 96), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, (640 / 1024) * 600));

    // 4. Client dimensions larger than host's by <2x.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1280, 900), 1.0, size(640, 480), dpi(96, 96), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, 480));

    // 5. Client dimensions larger than host's by >2x.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1280, 1024), 1.0, size(640, 480), dpi(96, 96), 1.0, false, true);
    assert.deepEqual(pluginSize, size(2 * 640, 2 * 480));

    // 6. Client X dimension larger than host's, Y dimension smaller.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1152, 600), 1.0, size(1024, 768), dpi(96, 96), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1024 * (600 / 768), 600));
});

QUnit.test('choosePluginSize() handles high-DPI client, low-DPI host',
  function(assert) {
    // 1. Client & host size the same.
    var pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 2.0, size(640, 480), dpi(96, 96), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, 480));

    // 2. Client logical dimensions smaller than host's.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 2.0, size(1024, 600), dpi(96, 96), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, (640 / 1024) * 600));

    // 3. Client Y dimension larger than host's, X dimension smaller.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 640), 2.0, size(1024, 600), dpi(96, 96), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, (640 / 1024) * 600));

    // 4. Client logical dimensions larger than host's by <2x.
    // Host dimensions fit into the client's _device_ dimensions 3x, so the
    // size in client DIPs should be 1:3/2.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1280, 900), 2.0, size(640, 480), dpi(96, 96), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640 * 3 / 2.0, 480 * 3 / 2.0));

    // 5. Client dimensions larger than host's by >2x.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1280, 1024), 2.0, size(640, 480), dpi(96, 96), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1280, (1280 / 640) * 480));

    // 6. Client X dimension larger than host's, Y dimension smaller.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1152, 600), 2.0, size(1024, 768), dpi(96, 96), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1024 * (600 / 768), 600));
});

QUnit.test('choosePluginSize() handles low-DPI client, high-DPI host',
  function(assert) {
    // 1. Client & host size the same.
    var pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 1.0, size(640, 480), dpi(192, 192), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, 480));

    // 2. Client logical dimensions smaller than host's.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 1.0, size(1024, 600), dpi(192, 192), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, (640 / 1024) * 600));

    // 3. Client Y dimension larger than host's, X dimension smaller.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 640), 1.0, size(1024, 600), dpi(192, 192), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, (640 / 1024) * 600));

    // 4. Client dimensions larger than host's by <2x.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1280, 900), 1.0, size(640, 480), dpi(192, 192), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, 480));

    // 5. Client dimensions larger than host's by >2x.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1280, 1024), 1.0, size(640, 480), dpi(192, 192), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1280, (1280 / 640) * 480));

    // 6. Client X dimension larger than host's, Y dimension smaller.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1152, 600), 1.0, size(1024, 768), dpi(192, 192), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1024 * (600 / 768), 600));
});

QUnit.test('choosePluginSize() handles high-DPI client and host',
  function(assert) {
    // 1. Client & host size the same.
    var pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 2.0, size(640, 480), dpi(192, 192), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, 480));

    // 2. Client logical dimensions smaller than host's.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 2.0, size(1024, 600), dpi(192, 192), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1024 / 2.0, 600 / 2.0));

    // 3. Client Y dimension larger than host's, X dimension smaller.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 640), 2.0, size(1024, 600), dpi(192, 192), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1024 / 2.0, 600 / 2.0));

    // 4. Client logical dimensions larger than host's by <2x.
    // Host dimensions fit into the client's _device_ dimensions 3x, so the
    // size in client DIPs should be 1:3/2.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1280, 900), 2.0, size(640, 480), dpi(192, 192), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640 * 3 / 2.0, 480 * 3 / 2.0));

    // 5. Client dimensions larger than host's by >2x.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1280, 1024), 2.0, size(640, 480), dpi(192, 192), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1280, (1280 / 640) * 480));

    // 6. Client X dimension larger than host's, Y dimension smaller.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1152, 600), 2.0, size(1024, 768), dpi(192, 192), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1024 / 2.0, 768 / 2.0));
});

QUnit.test('choosePluginSize() handles high-DPI client, 150% DPI host',
  function(assert) {
    // 1. Client & host size the same.
    var pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 2.0, size(640, 480), dpi(144, 144), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, 480));

    // 2. Client dimensions smaller than host's.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 2.0, size(1024, 600), dpi(144, 144), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1024 / 2.0, 600 / 2.0));

    // 3. Client Y dimension larger than host's, X dimension smaller.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 640), 2.0, size(1024, 600), dpi(144, 144), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1024 / 2.0, 600 / 2.0));

    // 4. Client dimensions larger than host's by <2x.
    // Host dimensions fit into the client's _device_ dimensions 3x, so the
    // size in client DIPs should be 1:3/2.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1280, 900), 2.0, size(640, 480), dpi(144, 144), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640 * 3 / 2.0, 480 * 3 / 2.0));

    // 5. Client dimensions larger than host's by >2x.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1280, 1024), 2.0, size(640, 480), dpi(144, 144), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1280, (1280 / 640) * 480));

    // 6. Client X dimension larger than host's, Y dimension smaller.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1152, 600), 2.0, size(1024, 768), dpi(144, 144), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1024 / 2.0, 768 / 2.0));
});

QUnit.test('choosePluginSize() handles high-DPI client, 125% DPI host',
  function(assert) {
    // 1. Client & host size the same.
    var pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 2.0, size(640, 480), dpi(120, 120), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, 480));

    // 2. Client dimensions smaller than host's.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 2.0, size(1024, 600), dpi(120, 120), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, 600 * (640 / 1024)));

    // 3. Client Y dimension larger than host's, X dimension smaller.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 640), 2.0, size(1024, 600), dpi(120, 120), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640, 600 * (640 / 1024)));

    // 4. Client dimensions larger than host's by <2x.
    // Host dimensions fit into the client's _device_ dimensions 3x, so the
    // size in client DIPs should be 1:3/2.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1280, 900), 2.0, size(640, 480), dpi(120, 120), 1.0, false, true);
    assert.deepEqual(pluginSize, size(640 * 3 / 2.0, 480 * 3 / 2.0));

    // 5. Client dimensions larger than host's by >2x.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1280, 1024), 2.0, size(640, 480), dpi(120, 120), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1280, (1280 / 640) * 480));

    // 6. Client X dimension larger than host's, Y dimension smaller.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1152, 600), 2.0, size(1024, 768), dpi(120, 120), 1.0, false, true);
    assert.deepEqual(pluginSize, size(1024 * (600 / 768), 600));
});

QUnit.test('choosePluginSize() with shrink-to-fit disabled',
  function(assert) {
    // 1. Client & host size the same.
    var pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 1.0, size(640, 480), dpi(96, 96), 1.0, false, false);
    assert.deepEqual(pluginSize, size(640, 480));

    // 2. Client logical dimensions smaller than host's.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 1.0, size(1024, 600), dpi(96, 96), 1.0, false, false);
    assert.deepEqual(pluginSize, size(1024, 600));

    // 3. Client dimensions larger than host's by <2x.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1280, 900), 1.0, size(640, 480), dpi(96, 96), 1.0, false, false);
    assert.deepEqual(pluginSize, size(640, 480));

    // 4. Client dimensions larger than host's by >2x.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1280, 1024), 1.0, size(640, 480), dpi(96, 96), 1.0, false, false);
    assert.deepEqual(pluginSize, size(1280, (1280 / 640) * 480));

    // 5. Client smaller than host, client high-DPI, host low-DPI.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 2.0, size(1024, 600), dpi(96, 96), 1.0, false, false);
    assert.deepEqual(pluginSize, size(1024, 600));

    // 6. Client smaller than host, client low-DPI, host high-DPI.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 1.0, size(1024, 600), dpi(192, 192), 1.0, false, false);
    assert.deepEqual(pluginSize, size(1024, 600));

    // 7. Client smaller than host, both high-DPI.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 2.0, size(1024, 600), dpi(192, 192), 1.0, false, false);
    assert.deepEqual(pluginSize, size(512, (512 / 1024) * 600));

    // 8. Client smaller than host, client high-DPI, host 150% DPI.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 2.0, size(1024, 600), dpi(144, 144), 1.0, false, false);
    assert.deepEqual(pluginSize, size(512, (512 / 1024) * 600));
});

QUnit.test('choosePluginSize() full-screen multi-monitor optimization',
  function(assert) {
    // Each test has a host sized to approximate two or more monitors.

    // 1. Client & host per-monitor dimensions match, two monitors side-by-side.
    var pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 1.0, size(2 * 640, 480), dpi(96, 96), 1.0, true, true);
    assert.deepEqual(pluginSize, size(2 * 640, 480));

    // 2. Client & host per-monitor dimensions match, two monitors stacked.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 1.0, size(640, 2 * 480), dpi(96, 96), 1.0, true, true);
    assert.deepEqual(pluginSize, size(640, 2 * 480));

    // 3. Client larger, two monitors stacked.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1024, 768), 1.0, size(640, 2 * 480), dpi(96, 96), 1.0, true, true);
    assert.deepEqual(pluginSize, size(640 * (768 / (2 * 480)), 768));

    // 4. Client smaller, two monitors stacked.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 1.0, size(1024, 2 * 768), dpi(96, 96), 1.0, true, true);
    assert.deepEqual(pluginSize, size(640, 2 * 768 * (640 / 1024)));

    // 5. Client wide-screen, host two standard monitors stacked.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(1920, 1080), 1.0, size(1024, 2 * 768), dpi(96, 96), 1.0,
        true, true);
    assert.deepEqual(pluginSize, size(1024 * (1080 / (2 * 768)), 1080));

    // 6. Client & host per-monitor dimensions match, two monitors stacked,
    //    high-DPI client.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 2.0, size(640, 2 * 480), dpi(96, 96), 1.0, true, true);
    assert.deepEqual(pluginSize, size(640, 2 * 480));

    // 7. Client & host per-monitor dimensions match, two monitors stacked,
    //    high-DPI host.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 1.0, size(640, 2 * 480), dpi(192, 192),
        1.0, true, true);
    assert.deepEqual(pluginSize, size(640, 2 * 480));

    // 8. Client & host per-monitor dimensions match, two monitors stacked,
    //    high-DPI client & host.
    pluginSize = remoting.DesktopViewport.choosePluginSize(
        size(640, 480), 2.0, size(640, 2 * 480), dpi(192, 192),
        1.0, true, true);
    assert.deepEqual(pluginSize, size(640 / 2.0, (2 * 480) / 2.0));
});

})();
