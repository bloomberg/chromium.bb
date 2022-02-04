// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-selected component.  */

import {Paths} from 'chrome://personalization/trusted/personalization_router_element.js';
import {WallpaperSelected} from 'chrome://personalization/trusted/wallpaper/wallpaper_selected_element.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

export function WallpaperSelectedTest() {
  let wallpaperSelectedElement: WallpaperSelected|null;
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    if (wallpaperSelectedElement) {
      wallpaperSelectedElement.remove();
    }
    wallpaperSelectedElement = null;
    await flushTasks();
  });

  test(
      'shows loading placeholder when there are in-flight requests',
      async () => {
        personalizationStore.data.wallpaper.loading = {
          ...personalizationStore.data.wallpaper.loading,
          selected: 1,
          setImage: 0,
        };
        wallpaperSelectedElement = initElement(WallpaperSelected);

        assertEquals(
            null, wallpaperSelectedElement.shadowRoot!.querySelector('img'));

        assertEquals(
            null,
            wallpaperSelectedElement.shadowRoot!.getElementById(
                'textContainer'));

        const placeholder = wallpaperSelectedElement.shadowRoot!.getElementById(
            'imagePlaceholder');

        assertTrue(!!placeholder);

        // Loading placeholder should be hidden.
        personalizationStore.data.wallpaper.loading = {
          ...personalizationStore.data.wallpaper.loading,
          selected: 0,
          setImage: 0,
        };
        personalizationStore.data.wallpaper.currentSelected =
            wallpaperProvider.currentWallpaper;
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperSelectedElement);

        assertEquals('none', placeholder.style.display);

        // Sent a request to update user wallpaper. Loading placeholder should
        // come back.
        personalizationStore.data.wallpaper.loading = {
          ...personalizationStore.data.wallpaper.loading,
          selected: 0,
          setImage: 1,
        };
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperSelectedElement);

        assertEquals('', placeholder.style.display);
      });

  test('shows wallpaper image and attribution when loaded', async () => {
    personalizationStore.data.wallpaper.currentSelected =
        wallpaperProvider.currentWallpaper;

    wallpaperSelectedElement = initElement(WallpaperSelected);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot!.querySelector('img');
    assertEquals(
        `chrome://image/?${wallpaperProvider.currentWallpaper.url.url}`,
        img!.src);

    const textContainerElements =
        wallpaperSelectedElement.shadowRoot!.querySelectorAll(
            '#textContainer span');

    // First span tag is 'Currently Set' text.
    assertEquals('currentlySet', textContainerElements[0]!.id);
    assertEquals(
        wallpaperSelectedElement.i18n('currentlySet'),
        textContainerElements[0]!.textContent);

    // Following text elements are for the photo attribution text.
    const attributionLines =
        Array.from(textContainerElements).slice(1) as HTMLElement[];

    assertEquals(
        wallpaperProvider.currentWallpaper.attribution.length,
        attributionLines.length);
    wallpaperProvider.currentWallpaper.attribution.forEach((line, i) => {
      assertEquals(line, attributionLines[i]!.innerText);
    });
  });

  test('shows unknown for empty attribution', async () => {
    personalizationStore.data.wallpaper.currentSelected = {
      url: {url: 'data:image/png;base64,abc='},
      attribution: [],
      assetId: BigInt(100),
    };
    personalizationStore.data.wallpaper.loading.selected = false;
    wallpaperSelectedElement = initElement(WallpaperSelected);
    await waitAfterNextRender(wallpaperSelectedElement);

    const title =
        wallpaperSelectedElement.shadowRoot!.getElementById('imageTitle');
    assertEquals(
        wallpaperSelectedElement.i18n('unknownImageAttribution'),
        title!.textContent!.trim());
  });

  test('removes high resolution suffix from image url', async () => {
    personalizationStore.data.wallpaper.currentSelected = {
      url: {url: 'https://images.googleusercontent.com/abc12=w456'},
      attribution: [],
      assetId: BigInt(100),
    };
    personalizationStore.data.wallpaper.loading.selected = false;
    wallpaperSelectedElement = initElement(WallpaperSelected);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot!.querySelector('img');
    assertEquals(
        'chrome://image/?https://images.googleusercontent.com/abc12', img!.src);
  });

  test('updates image when store is updated', async () => {
    personalizationStore.data.wallpaper.currentSelected =
        wallpaperProvider.currentWallpaper;
    personalizationStore.data.wallpaper.loading.selected = false;

    wallpaperSelectedElement = initElement(WallpaperSelected);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot!.querySelector('img') as
        HTMLImageElement;
    assertEquals(
        `chrome://image/?${wallpaperProvider.currentWallpaper.url.url}`,
        img.src);

    personalizationStore.data.wallpaper.currentSelected = {
      url: {url: 'https://testing'},
      attribution: ['New attribution'],
      assetId: BigInt(100),
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    assertEquals('chrome://image/?https://testing', img.src);
  });

  test('shows placeholders when image fails to load', async () => {
    wallpaperSelectedElement = initElement(WallpaperSelected);
    await waitAfterNextRender(wallpaperSelectedElement);

    // Still loading.
    personalizationStore.data.wallpaper.loading.selected = true;
    personalizationStore.data.wallpaper.currentSelected = null;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    const placeholder =
        wallpaperSelectedElement.shadowRoot!.getElementById('imagePlaceholder');
    assertTrue(!!placeholder);

    // Loading finished and still no current wallpaper.
    personalizationStore.data.wallpaper.loading.selected = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    // Dom-if will set display: none if the element is hidden. Make sure it is
    // not hidden.
    assertNotEquals('none', placeholder.style.display);
    assertEquals(
        null, wallpaperSelectedElement.shadowRoot!.querySelector('img'));
  });

  test('shows image url with data scheme', async () => {
    personalizationStore.data.wallpaper.currentSelected = {
      url: {url: 'data:image/png;base64,abc='},
      attribution: [],
      assetId: BigInt(100),
    };
    personalizationStore.data.wallpaper.loading.selected = false;
    wallpaperSelectedElement = initElement(WallpaperSelected);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot!.querySelector('img');
    assertEquals('data:image/png;base64,abc=', img!.src);
  });

  test('shows daily refresh option on the collection view', async () => {
    personalizationStore.data.wallpaper.currentSelected = {
      url: {url: 'data:image/png;base64,abc='},
      attribution: [],
      assetId: BigInt(100),
    };
    personalizationStore.data.wallpaper.loading.selected = false;

    wallpaperSelectedElement =
        initElement(WallpaperSelected, {'path': Paths.CollectionImages});
    await waitAfterNextRender(wallpaperSelectedElement);

    const dailyRefresh =
        wallpaperSelectedElement.shadowRoot!.getElementById('dailyRefresh');
    assertTrue(!!dailyRefresh);

    const refreshWallpaper =
        wallpaperSelectedElement.shadowRoot!.getElementById('refreshWallpaper');
    assertTrue(refreshWallpaper!.hidden);
  });

  test(
      'shows refresh button only on collection with daily refresh enabled',
      async () => {
        personalizationStore.data.wallpaper.currentSelected = {
          url: {url: 'data:image/png;base64,abc='},
          attribution: [],
          assetId: BigInt(100),
        };
        personalizationStore.data.wallpaper.loading.selected = false;
        const collection_id = wallpaperProvider.collections![0]!.id;
        personalizationStore.data.wallpaper.dailyRefresh = {
          collectionId: collection_id,
        };

        wallpaperSelectedElement = initElement(
            WallpaperSelected,
            {'path': Paths.CollectionImages, 'collectionId': collection_id});
        personalizationStore.notifyObservers();

        await waitAfterNextRender(wallpaperSelectedElement);

        const newRefreshWallpaper =
            wallpaperSelectedElement.shadowRoot!.getElementById(
                'refreshWallpaper');
        assertFalse(newRefreshWallpaper!.hidden);
      });
}
