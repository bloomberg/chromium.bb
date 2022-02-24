// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {BackgroundManager} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createBackgroundImage} from './test_support.js';

class FakeIFrameElement extends HTMLIFrameElement {
  url: string|null = null;

  get contentWindow() {
    return {location: {replace: (url: string) => this.url = url}} as unknown as
        Window;
  }
}

customElements.define('fake-iframe', FakeIFrameElement, {extends: 'iframe'});

suite('NewTabPageBackgroundManagerTest', () => {
  let backgroundManager: BackgroundManager;
  let backgroundImage: FakeIFrameElement;

  function wrapImageUrl(url: string): string {
    return `chrome-untrusted://new-tab-page/custom_background_image?url=${
        encodeURIComponent(url)}`;
  }

  setup(() => {
    document.body.innerHTML = '';

    backgroundImage = new FakeIFrameElement();
    backgroundImage.id = 'backgroundImage';
    document.body.appendChild(backgroundImage);

    backgroundManager = new BackgroundManager();
  });

  test('showing background image sets attribute', () => {
    // Act.
    backgroundManager.setShowBackgroundImage(true);

    // Assert.
    assertTrue(document.body.hasAttribute('show-background-image'));
  });

  test('hiding background image removes attribute', () => {
    // Arrange.
    backgroundManager.setShowBackgroundImage(true);

    // Act.
    backgroundManager.setShowBackgroundImage(false);

    // Assert.
    assertFalse(document.body.hasAttribute('show-background-image'));
  });

  test('setting url updates src', () => {
    // Act.
    backgroundManager.setBackgroundImage(
        createBackgroundImage('https://example.com'));

    // Assert.
    assertEquals(wrapImageUrl('https://example.com'), backgroundImage.url);
  });

  test('setting same url does not update src', () => {
    // Arrange.
    const image = createBackgroundImage('https://example.com');
    backgroundManager.setBackgroundImage(image);
    backgroundImage.url = null;

    // Act.
    backgroundManager.setBackgroundImage(image);

    // Assert.
    assertEquals(null, backgroundImage.url);
  });

  test('setting custom style updates src', () => {
    // Act.
    backgroundManager.setBackgroundImage({
      url: {url: 'https://example.com'},
      url2x: {url: 'https://example2x.com'},
      size: 'cover',
      repeatX: 'no-repeat',
      repeatY: 'repeat',
      positionX: 'left',
      positionY: 'top',
      attributionUrl: undefined,
    });

    // Assert.
    const expected =
        'chrome-untrusted://new-tab-page/custom_background_image?' +
        `url=${encodeURIComponent('https://example.com')}&` +
        `url2x=${encodeURIComponent('https://example2x.com')}&` +
        'size=cover&repeatX=no-repeat&repeatY=repeat&positionX=left&' +
        'positionY=top';
    assertEquals(expected, backgroundImage.url);
  });

  test.skip('receiving load time resolves promise', async () => {
    // Arrange.
    backgroundManager.setBackgroundImage(
        createBackgroundImage('https://example.com'));

    // Act.
    const promise = backgroundManager.getBackgroundImageLoadTime();
    window.dispatchEvent(new MessageEvent('message', {
      data: {
        frameType: 'background-image',
        messageType: 'loaded',
        url: wrapImageUrl('https://example.com'),
        time: 123,
      }
    }));

    // Assert.
    assertEquals(123, await promise);
  });

  test.skip('receiving load time resolves multiple promises', async () => {
    // Arrange.
    backgroundManager.setBackgroundImage(
        createBackgroundImage('https://example.com'));

    // Act.
    const promises = [
      backgroundManager.getBackgroundImageLoadTime(),
      backgroundManager.getBackgroundImageLoadTime(),
      backgroundManager.getBackgroundImageLoadTime(),
    ];
    window.dispatchEvent(new MessageEvent('message', {
      data: {
        frameType: 'background-image',
        messageType: 'loaded',
        url: wrapImageUrl('https://example.com'),
        time: 123,
      }
    }));

    // Assert.
    const values = await Promise.all(promises);
    values.forEach((value) => {
      assertEquals(123, value);
    });
  });

  test.skip('setting new url rejects promise', async () => {
    // Arrange.
    backgroundManager.setBackgroundImage(
        createBackgroundImage('https://example.com'));

    // Act.
    const promise = backgroundManager.getBackgroundImageLoadTime();
    backgroundManager.setBackgroundImage(
        createBackgroundImage('https://other.com'));

    // Assert.
    return promise.then(
        () => assertNotReached(),
        () => {
            // Success. Nothing to do here.
        });
  });
});
