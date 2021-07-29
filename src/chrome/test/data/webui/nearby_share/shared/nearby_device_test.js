// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// So that mojo is defined.
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
// #import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
// #import 'chrome://nearby/mojo/nearby_share_target_types.mojom-lite.js';
// #import 'chrome://nearby/mojo/nearby_share_share_type.mojom-lite.js';
// #import 'chrome://nearby/mojo/nearby_share.mojom-lite.js';
// #import 'chrome://nearby/shared/nearby_device.m.js';
// #import {assertEquals} from '../../chai_assert.js';
// clang-format on

suite('DeviceTest', function() {
  /** @type {!NearbyDeviceElement} */
  let deviceElement;

  setup(function() {
    deviceElement = /** @type {!NearbyDeviceElement} */ (
        document.createElement('nearby-device'));
    document.body.appendChild(deviceElement);
  });

  teardown(function() {
    deviceElement.remove();
  });

  /** @return {!nearbyShare.mojom.ShareTarget} */
  function getDefaultShareTarget() {
    return /** @type {!nearbyShare.mojom.ShareTarget} */ ({
      id: {high: 0, low: 0},
      name: 'Default Device Name',
      type: nearbyShare.mojom.ShareTargetType.kPhone,
      imageUrl: {
        url: 'http://goog.com/image',
      },
    });
  }

  test('renders component', function() {
    assertEquals('NEARBY-DEVICE', deviceElement.tagName);
  });

  test('renders name', function() {
    const name = 'Device Name';
    const shareTarget = getDefaultShareTarget();
    shareTarget.name = name;
    deviceElement.shareTarget = shareTarget;

    const renderedName = deviceElement.$$('#name').textContent;
    assertEquals(name, renderedName);
  });

  test('renders target image', function() {
    deviceElement.shareTarget = getDefaultShareTarget();

    const renderedSource = deviceElement.$$('#share-target-image').src;
    assertEquals('chrome://image/?http://goog.com/image=s26', renderedSource);
  });

  test('renders blank target image', function() {
    const shareTarget = getDefaultShareTarget();
    shareTarget.imageUrl.url = '';
    deviceElement.shareTarget = shareTarget;

    const renderedSource = deviceElement.$$('#share-target-image').src;
    assertEquals('', renderedSource);
  });
});
