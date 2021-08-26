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
// #import 'chrome://nearby/shared/nearby_progress.m.js';
// #import {assertEquals} from '../../chai_assert.js';
// clang-format on

suite('ProgressTest', function() {
  /** @type {!NearbyProgressElement} */
  let progressElement;

  setup(function() {
    progressElement = /** @type {!NearbyProgressElement} */ (
        document.createElement('nearby-progress'));
    document.body.appendChild(progressElement);
  });

  teardown(function() {
    progressElement.remove();
  });

  /** @return {!nearbyShare.mojom.ShareTarget} */
  function getDefaultShareTarget() {
    return /** @type {!nearbyShare.mojom.ShareTarget} */ ({
      id: {high: 0, low: 0},
      name: 'Default Device Name',
      type: nearbyShare.mojom.ShareTargetType.kPhone,
      imageUrl: {
        url: 'http://google.com/image',
      },
    });
  }

  test('renders component', function() {
    assertEquals('NEARBY-PROGRESS', progressElement.tagName);
  });

  test('renders device name', function() {
    const name = 'Device Name';
    const shareTarget = getDefaultShareTarget();
    shareTarget.name = name;
    progressElement.shareTarget = shareTarget;

    const renderedName = progressElement.$$('#device-name').innerText;
    assertEquals(name, renderedName);
  });

  test('renders target image', function() {
    progressElement.shareTarget = getDefaultShareTarget();

    const renderedSource = progressElement.$$('#share-target-image').src;
    assertEquals('chrome://image/?http://google.com/image=s68', renderedSource);
  });

  test('renders blank target image', function() {
    const shareTarget = getDefaultShareTarget();
    shareTarget.imageUrl.url = '';
    progressElement.shareTarget = shareTarget;

    const renderedSource = progressElement.$$('#share-target-image').src;
    assertEquals('', renderedSource);
  });
});
