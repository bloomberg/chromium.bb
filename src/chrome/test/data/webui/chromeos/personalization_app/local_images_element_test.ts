// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LocalImages} from 'chrome://personalization/trusted/wallpaper/local_images_element.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

export function LocalImagesTest() {
  let localImagesElement: LocalImages|null;

  let wallpaperProvider: TestWallpaperProvider;

  let personalizationStore: TestPersonalizationStore;

  /**
   * Get all currently visible photo loading placeholders.
   */
  function getLoadingPlaceholders(): HTMLElement[] {
    if (!localImagesElement) {
      return [];
    }
    const selectors = [
      '.photo-container:not([hidden])',
      '.photo-inner-container.placeholder:not([style*="display: none"])',
    ];
    return Array.from(
        localImagesElement.shadowRoot!.querySelectorAll(selectors.join(' ')));
  }

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    await teardownElement(localImagesElement);
    localImagesElement = null;
    await flushTasks();
  });

  test('displays a loading placeholder for unloaded local images', async () => {
    personalizationStore.data.wallpaper.local = {
      images: wallpaperProvider.localImages,
      data: wallpaperProvider.localImageData,
    };
    personalizationStore.data.wallpaper.loading.local = {
      images: false,
      data: {}
    };

    localImagesElement = initElement(LocalImages, {hidden: false});
    await waitAfterNextRender(localImagesElement);

    // Iron-list creates some extra dom elements as a scroll buffer and
    // hides them.  Only select visible elements here to get the real ones.
    let loadingPlaceholders = getLoadingPlaceholders();
    // Counts as loading if store.loading.local.data does not contain an
    // entry for the image. Therefore should be 2 loading tiles.
    assertEquals(2, loadingPlaceholders.length);

    personalizationStore.data.wallpaper.loading.local = {
      images: false,
      data: {'LocalImage0.png': true, 'LocalImage1.png': true},
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(localImagesElement);


    loadingPlaceholders = getLoadingPlaceholders();
    // Still 2 loading tiles.
    assertEquals(2, loadingPlaceholders.length);

    personalizationStore.data.wallpaper.loading.local = {
      images: false,
      data: {'LocalImage0.png': false, 'LocalImage1.png': true},
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(localImagesElement);

    loadingPlaceholders = getLoadingPlaceholders();
    assertEquals(1, loadingPlaceholders.length);
  });

  test(
      'displays images for current local images that have successfully loaded',
      async () => {
        personalizationStore.data.wallpaper.local = {
          images: wallpaperProvider.localImages,
          data: wallpaperProvider.localImageData,
        };
        personalizationStore.data.wallpaper.loading.local = {
          images: false,
          data: {}
        };

        localImagesElement = initElement(LocalImages, {hidden: false});

        const ironList =
            localImagesElement.shadowRoot!.querySelector('iron-list');
        assertTrue(!!ironList);

        // Both items are sent. No images are rendered yet because they are not
        // done loading thumbnails.
        assertEquals(2, ironList.items!.length);
        assertEquals(0, ironList.shadowRoot!.querySelectorAll('img').length);

        // Set loading finished for first thumbnail.
        personalizationStore.data.wallpaper.loading.local.data = {
          'LocalImage0.png': false
        };
        personalizationStore.notifyObservers();
        await waitAfterNextRender(localImagesElement);
        assertEquals(2, ironList.items!.length);
        let imgTags = localImagesElement.shadowRoot!.querySelectorAll('img');
        assertEquals(1, imgTags.length);
        assertEquals('data://localimage0data', imgTags![0]!.src);

        // Set loading failed for second thumbnail.
        personalizationStore.data.wallpaper.loading.local.data = {
          'LocalImage0.png': false,
          'LocalImage1.png': false
        };
        personalizationStore.data.wallpaper.local.data = {
          'LocalImage0.png': 'data://localimage0data',
          'LocalImage1.png': null
        };
        personalizationStore.notifyObservers();
        await waitAfterNextRender(localImagesElement);
        // Still only first thumbnail displayed.
        imgTags = localImagesElement.shadowRoot!.querySelectorAll('img');
        assertEquals(1, imgTags.length);
        assertEquals('data://localimage0data', imgTags![0]!.src);
      });

  test(
      'sets aria-selected attribute if image name matches currently selected',
      async () => {
        personalizationStore.data.wallpaper.local = {
          images: wallpaperProvider.localImages,
          data: wallpaperProvider.localImageData,
        };
        // Done loading.
        personalizationStore.data.wallpaper.loading.local = {
          images: false,
          data: {'LocalImage0.png': false, 'LocalImage1.png': false},
        };

        localImagesElement = initElement(LocalImages, {hidden: false});
        await waitAfterNextRender(localImagesElement);

        // iron-list pre-creates some extra DOM elements but marks them as
        // hidden. Ignore them here to only get visible images.
        const images = localImagesElement.shadowRoot!.querySelectorAll(
            '.photo-container:not([hidden]) .photo-inner-container');

        assertEquals(2, images.length);
        // Every image is aria-selected false.
        assertTrue(Array.from(images).every(
            image => image.getAttribute('aria-selected') === 'false'));

        personalizationStore.data.wallpaper.currentSelected = {
          key: 'LocalImage1.png'
        };
        personalizationStore.notifyObservers();

        assertEquals(2, images.length);
        assertEquals(images[0]!.getAttribute('aria-selected'), 'false');
        assertEquals(images[1]!.getAttribute('aria-selected'), 'true');
      });

  test('images have proper aria label when loaded', async () => {
    personalizationStore.data.wallpaper.local = {
      images: wallpaperProvider.localImages,
      data: wallpaperProvider.localImageData,
    };
    // Done loading.
    personalizationStore.data.wallpaper.loading.local = {
      images: false,
      data: {'LocalImage0.png': false, 'LocalImage1.png': false},
    };

    localImagesElement = initElement(LocalImages, {hidden: false});
    await waitAfterNextRender(localImagesElement);

    // iron-list pre-creates some extra DOM elements but marks them as
    // hidden. Ignore them here to only get visible images.
    const images = localImagesElement.shadowRoot!.querySelectorAll(
        '.photo-container:not([hidden]) .photo-inner-container');

    assertEquals(2, images.length);
    // Every image is aria-selected false.
    assertTrue(Array.from(images).every(
        image => image.getAttribute('aria-selected') === 'false'));
    // Every image has aria-label set.
    assertEquals(
        images[0]!.getAttribute('aria-label'),
        wallpaperProvider.localImages![0]!.path);
    assertEquals(
        images[1]!.getAttribute('aria-label'),
        wallpaperProvider.localImages![1]!.path);
  });
}
