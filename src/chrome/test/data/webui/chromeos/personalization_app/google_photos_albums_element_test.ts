// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {getCountText, GooglePhotosAlbum, GooglePhotosAlbums, initializeGooglePhotosData, PersonalizationRouter, WallpaperGridItem} from 'chrome://personalization/trusted/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('GooglePhotosAlbumsTest', function() {
  let googlePhotosAlbumsElement: GooglePhotosAlbums|null;
  let personalizationStore: TestPersonalizationStore;
  let wallpaperProvider: TestWallpaperProvider;

  /**
   * Returns the match for |selector| in |googlePhotosAlbumsElement|'s shadow
   * DOM.
   */
  function querySelector(selector: string): Element|null {
    return googlePhotosAlbumsElement!.shadowRoot!.querySelector(selector);
  }

  /**
   * Returns all matches for |selector| in |googlePhotosAlbumsElement|'s shadow
   * DOM.
   */
  function querySelectorAll(selector: string): Element[]|null {
    const matches =
        googlePhotosAlbumsElement!.shadowRoot!.querySelectorAll(selector);
    return matches ? [...matches] : null;
  }

  /** Scrolls the specified |element| to the bottom. */
  async function scrollToBottom(element: HTMLElement) {
    element.scrollTop = element.scrollHeight;
    await waitAfterNextRender(googlePhotosAlbumsElement!);
  }

  setup(() => {
    loadTimeData.overrideValues({'isGooglePhotosIntegrationEnabled': true});

    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    personalizationStore.setReducersEnabled(true);
    wallpaperProvider = mocks.wallpaperProvider;
  });

  teardown(async () => {
    await teardownElement(googlePhotosAlbumsElement);
    googlePhotosAlbumsElement = null;
  });

  test('displays albums', async () => {
    const albums: GooglePhotosAlbum[] = [
      {
        id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
        title: 'Album 0',
        photoCount: 0,
        preview: {url: 'foo.com'}
      },
      {
        id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
        title: 'Album 1',
        photoCount: 1,
        preview: {url: 'bar.com'}
      },
      {
        id: '0a268a37-877a-4936-81d4-38cc84b0f596',
        title: 'Album 2',
        photoCount: 2,
        preview: {url: 'baz.com'}
      }
    ];

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosAlbums(albums);
    wallpaperProvider.setGooglePhotosCount(
        albums.reduce((photosCount, album) => {
          photosCount += album.photoCount;
          return photosCount;
        }, 0));

    // Initialize |googlePhotosAlbumsElement|.
    googlePhotosAlbumsElement =
        initElement(GooglePhotosAlbums, {hidden: false});
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // The |personalizationStore| should be empty, so no albums should be
    // rendered initially.
    const albumSelector =
        'wallpaper-grid-item:not([hidden]).album:not([placeholder])';
    assertEquals(querySelectorAll(albumSelector)!.length, 0);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // The wallpaper controller is expected to impose max resolution.
    albums.forEach(album => album.preview.url += '=s512');

    // Verify that the expected |albums| are rendered.
    const albumEls = querySelectorAll(albumSelector) as WallpaperGridItem[];
    assertEquals(albumEls.length, albums.length);
    albumEls.forEach((albumEl, i) => {
      assertEquals(albumEl.imageSrc, albums[i]!.preview.url);
      assertEquals(albumEl.primaryText, albums[i]!.title);
      assertEquals(albumEl.secondaryText, getCountText(albums[i]!.photoCount));
    });
  });

  test('displays placeholders until albums are present', async () => {
    // Prepare Google Photos data.
    const photosCount = 5;
    const albums: GooglePhotosAlbum[] =
        Array.from({length: photosCount}, (_, i) => ({
                                            id: `id-${i}`,
                                            title: `title-${i}`,
                                            photoCount: 1,
                                            preview: {url: `url-${i}`},
                                          }));

    // Initialize |googlePhotosAlbumsElement|.
    googlePhotosAlbumsElement =
        initElement(GooglePhotosAlbums, {hidden: false});
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // Initially only placeholders should be present.
    const selector = 'wallpaper-grid-item:not([hidden]).album';
    const albumSelector = `${selector}:not([placeholder])`;
    const placeholderSelector = `${selector}[placeholder]`;
    assertEquals(querySelectorAll(albumSelector)!.length, 0);
    assertNotEquals(querySelectorAll(placeholderSelector)!.length, 0);

    // Mock singleton |PersonalizationRouter|.
    const router = TestBrowserProxy.fromClass(PersonalizationRouter);
    PersonalizationRouter.instance = () => router;

    // Mock |PersonalizationRouter.selectGooglePhotosAlbum()|.
    let selectedGooglePhotosAlbum: GooglePhotosAlbum|undefined;
    router.selectGooglePhotosAlbum = (album: GooglePhotosAlbum) => {
      selectedGooglePhotosAlbum = album;
    };

    // Clicking a placeholder should do nothing.
    (querySelector(placeholderSelector) as HTMLElement).click();
    await new Promise<void>(resolve => setTimeout(resolve));
    assertEquals(selectedGooglePhotosAlbum, undefined);

    // Provide Google Photos data.
    personalizationStore.data.wallpaper.googlePhotos.count = photosCount;
    personalizationStore.data.wallpaper.googlePhotos.albums = albums;
    personalizationStore.notifyObservers();

    // Only albums should be present.
    await waitAfterNextRender(googlePhotosAlbumsElement);
    assertNotEquals(querySelectorAll(albumSelector)!.length, 0);
    assertEquals(querySelectorAll(placeholderSelector)!.length, 0);

    // Clicking an album should do something.
    (querySelector(albumSelector) as HTMLElement).click();
    assertEquals(selectedGooglePhotosAlbum, albums[0]);
  });

  test('incrementally loads albums', async () => {
    // Set photos count returned by |wallpaperProvider|.
    const photosCount = 200;
    wallpaperProvider.setGooglePhotosCount(photosCount);

    // Set initial list of albums returned by |wallpaperProvider|.
    const albumsCount = 200;
    let nextAlbumId = 1;
    wallpaperProvider.setGooglePhotosAlbums(
        Array.from({length: albumsCount / 2}).map(() => {
          return {
            id: `id-${nextAlbumId}`,
            title: `title-${nextAlbumId}`,
            photoCount: photosCount / albumsCount,
            preview: {url: `url-${nextAlbumId++}`}
          };
        }));

    // Set initial albums resume token returned by |wallpaperProvider|. When
    // resume token is defined, it indicates additional albums exist.
    const resumeToken = 'resumeToken';
    wallpaperProvider.setGooglePhotosAlbumsResumeToken(resumeToken);

    // Initialize Google Photos data in |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    assertEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosAlbums'),
        /*resumeToken=*/ null);

    // Reset |wallpaperProvider| expectations.
    wallpaperProvider.resetResolver('fetchGooglePhotosAlbums');

    // Set the next list of albums returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosAlbums(
        Array.from({length: albumsCount / 2}).map(() => {
          return {
            id: `id-${nextAlbumId}`,
            title: `title-${nextAlbumId}`,
            photoCount: photosCount / albumsCount,
            preview: {url: `url-${nextAlbumId++}`}
          };
        }));

    // Set the next albums resume token returned by |wallpaperProvider|. When
    // resume token is undefined, it indicates no additional albums exist.
    wallpaperProvider.setGooglePhotosAlbumsResumeToken(undefined);

    // Restrict the viewport so that |googlePhotosAlbumsElement| will lazily
    // create albums instead of creating them all at once.
    const style = document.createElement('style');
    style.appendChild(document.createTextNode(`
      html,
      body {
        height: 100%;
        width: 100%;
      }
    `));
    document.head.appendChild(style);

    // Initialize |googlePhotosAlbumsElement|.
    googlePhotosAlbumsElement =
        initElement(GooglePhotosAlbums, {hidden: false});
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // Scroll to the bottom of the grid.
    const gridScrollThreshold = googlePhotosAlbumsElement.$.gridScrollThreshold;
    scrollToBottom(gridScrollThreshold);

    // Wait for and verify that the next batch of albums has been
    // requested.
    assertEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosAlbums'),
        resumeToken);
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // Reset |wallpaperProvider| expectations.
    wallpaperProvider.resetResolver('fetchGooglePhotosAlbums');

    // Scroll to the bottom of the grid.
    scrollToBottom(gridScrollThreshold);

    // Verify that no next batch of albums has been requested.
    assertEquals(wallpaperProvider.getCallCount('fetchGooglePhotosAlbums'), 0);
  });
});
