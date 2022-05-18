// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {fetchGooglePhotosPhotos, getNumberOfGridItemsPerRow, GooglePhotosPhoto, GooglePhotosPhotos, GooglePhotosPhotosSection, initializeGooglePhotosData, PersonalizationActionName, SetErrorAction, WallpaperGridItem, WallpaperLayout, WallpaperType} from 'chrome://personalization/trusted/personalization_app.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('GooglePhotosPhotosTest', function() {
  let googlePhotosPhotosElement: GooglePhotosPhotos|null;
  let personalizationStore: TestPersonalizationStore;
  let wallpaperProvider: TestWallpaperProvider;

  /** Dispatches a keydown event to |element| for the specified |key|. */
  function dispatchKeydown(element: HTMLElement, key: string) {
    const init: KeyboardEventInit = {bubbles: true, key};
    switch (key) {
      case 'ArrowDown':
        init.keyCode = 40;
        break;
      case 'ArrowRight':
        init.keyCode = 39;
        break;
      case 'ArrowLeft':
        init.keyCode = 37;
        break;
      case 'ArrowUp':
        init.keyCode = 38;
        break;
    }
    element.dispatchEvent(new KeyboardEvent('keydown', init));
  }

  /** Returns the active element in |googlePhotosPhotosElement|'s shadow DOM. */
  function getActiveElement(): Element|null {
    return googlePhotosPhotosElement!.shadowRoot!.activeElement;
  }

  /**
   * Returns the match for |selector| in |googlePhotosPhotosElement|'s shadow
   * DOM.
   */
  function querySelector(selector: string): Element|null {
    return googlePhotosPhotosElement!.shadowRoot!.querySelector(selector);
  }

  /**
   * Returns all matches for |selector| in |googlePhotosPhotosElement|'s shadow
   * DOM.
   */
  function querySelectorAll(selector: string): Element[]|null {
    const matches =
        googlePhotosPhotosElement!.shadowRoot!.querySelectorAll(selector);
    return matches ? [...matches] : null;
  }

  /** Scrolls the specified |element| to the bottom. */
  async function scrollToBottom(element: HTMLElement) {
    element.scrollTop = element.scrollHeight;
    await waitAfterNextRender(googlePhotosPhotosElement!);
  }

  /**
   * Returns a list of |GooglePhotosPhotosSection|'s for the specified |photos|
   * and number of |photosPerRow|.
   */
  function toSections(photos: GooglePhotosPhoto[], photosPerRow: number):
      GooglePhotosPhotosSection[] {
    const sections: GooglePhotosPhotosSection[] = [];

    photos.forEach((photo, i) => {
      const date = toString(photo.date);

      // Find/create the appropriate |section| in which to insert |photo|.
      let section = sections[sections.length - 1];
      if (section?.date !== date) {
        section = {date, locations: new Set<string>(), rows: []};
        sections.push(section);
      }

      // Find/create the appropriate |row| in which to insert |photo|.
      let row = section.rows[section.rows.length - 1];
      if ((row?.length ?? photosPerRow) === photosPerRow) {
        row = [];
        section.rows.push(row);
      }

      row!.push({...photo, index: i});

      if (photo.location) {
        section.locations.add(photo.location);
      }
    });

    return sections;
  }

  /** Returns a |string| from the specified |value|. */
  function toString(value: String16): string {
    return value.data.map(c => String.fromCodePoint(c)).join('');
  }

  /** Returns a |String16| from the specified |value|. */
  function toString16(value: string): String16 {
    const data = [];
    for (let i = 0; i < value.length; ++i) {
      data[i] = value.charCodeAt(i);
    }
    return {data};
  }

  /**
   * Waits for the specified |element| to be the active element in
   * |googlePhotosPhotosElement|'s shadow DOM.
   */
  async function waitForActiveElement(element: Element) {
    while (googlePhotosPhotosElement!.shadowRoot!.activeElement !== element) {
      await waitAfterNextRender(googlePhotosPhotosElement!);
    }
  }

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    personalizationStore.setReducersEnabled(true);
    wallpaperProvider = mocks.wallpaperProvider;
  });

  teardown(async () => {
    await teardownElement(googlePhotosPhotosElement);
    googlePhotosPhotosElement = null;
  });

  test('advances focus', async () => {
    // Initialize |photos| to result in the following formation:
    //   First row
    //   [1]
    //   Second row
    //   [2] [3]
    //   Third row
    //   [4]
    const photos: GooglePhotosPhoto[] = [
      // First row.
      {
        id: '1',
        name: '1',
        date: toString16('First row'),
        url: {url: '1'},
        location: '1'
      },
      // Second row.
      {
        id: '2',
        name: '2',
        date: toString16('Second row'),
        url: {url: '2'},
        location: '2'
      },
      {
        id: '3',
        name: '3',
        date: toString16('Second row'),
        url: {url: '3'},
        location: '3'
      },
      // Third row.
      {
        id: '4',
        name: '4',
        date: toString16('Third row'),
        url: {url: '4'},
        location: '4'
      }
    ];

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosPhotos(photos);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    await fetchGooglePhotosPhotos(wallpaperProvider, personalizationStore);

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Focus the first photo.
    const photoSelector =
        'wallpaper-grid-item:not([hidden]).photo:not([placeholder])';
    const photoEls = querySelectorAll(photoSelector);
    assertEquals(photoEls?.length, 4);
    (photoEls?.[0] as HTMLElement).focus();
    await waitForActiveElement(photoEls?.[0]!);

    // Use the right arrow key to traverse to the last photo. Focus should pass
    // through all the photos in between.
    for (let i = 1; i <= 3; ++i) {
      dispatchKeydown(getActiveElement()?.closest('.row')!, 'ArrowRight');
      await waitForActiveElement(photoEls?.[i]!);
    }

    // Use the left arrow key to traverse to the first photo. Focus should pass
    // through all the photos in between.
    for (let i = 2; i >= 0; --i) {
      dispatchKeydown(getActiveElement()?.closest('.row')!, 'ArrowLeft');
      await waitForActiveElement(photoEls?.[i]!);
    }

    // Use the down arrow key to traverse to the last photo. Focus should only
    // pass through the photos in between which are in the same column.
    for (let i = 1; i <= 3; i = i + 2) {
      dispatchKeydown(getActiveElement()?.closest('.row')!, 'ArrowDown');
      await waitForActiveElement(photoEls?.[i]!);
    }

    // Use the up arrow key to traverse to the first photo. Focus should only
    // pass through the photos in between which are in the same column.
    for (let i = 1; i >= 0; --i) {
      dispatchKeydown(getActiveElement()?.closest('.row')!, 'ArrowUp');
      await waitForActiveElement(photoEls?.[i]!);
    }

    // Focus the third photo.
    dispatchKeydown(getActiveElement()?.closest('.row')!, 'ArrowRight');
    dispatchKeydown(getActiveElement()?.closest('.row')!, 'ArrowRight');
    await waitForActiveElement(photoEls?.[2]!);

    // Because no photo exists directly below the third photo, the down arrow
    // key should do nothing.
    dispatchKeydown(getActiveElement()?.closest('.row')!, 'ArrowDown');
    await new Promise<void>(resolve => setTimeout(resolve, 100));
    assertEquals(getActiveElement(), photoEls?.[2]);

    // Because no photo exists directly above the third photo, the up arrow key
    // should do nothing.
    dispatchKeydown(getActiveElement()?.closest('.row')!, 'ArrowUp');
    await new Promise<void>(resolve => setTimeout(resolve, 100));
    assertEquals(getActiveElement(), photoEls?.[2]);
  });

  [true, false].forEach(
      (dismissFromUser: boolean) =>
          test('displays error when photos fail to load', async () => {
            // Set values returned by |wallpaperProvider|.
            wallpaperProvider.setGooglePhotosPhotos(undefined);

            // Initialize |googlePhotosPhotosElement|.
            googlePhotosPhotosElement =
                initElement(GooglePhotosPhotos, {hidden: false});
            await waitAfterNextRender(googlePhotosPhotosElement);

            // Initialize Google Photos data in the |personalizationStore| and
            // expect an |error|.
            personalizationStore.expectAction(
                PersonalizationActionName.SET_ERROR);
            await initializeGooglePhotosData(
                wallpaperProvider, personalizationStore);
            await fetchGooglePhotosPhotos(
                wallpaperProvider, personalizationStore);
            const {error} =
                await personalizationStore.waitForAction(
                    PersonalizationActionName.SET_ERROR) as SetErrorAction;

            // Verify |error| expectations.
            assertEquals(error.message, 'Something went wrong.');
            assertEquals(error.dismiss?.message, 'Retry');
            assertNotEquals(error.dismiss?.callback, undefined);

            wallpaperProvider.reset();

            // Simulate dismissal of |error| conditionally |fromUser| and verify
            // expected interactions with wallpaper provider.
            error.dismiss?.callback?.(/*fromUser=*/ dismissFromUser);
            await new Promise<void>(resolve => setTimeout(resolve));
            assertEquals(
                wallpaperProvider.getCallCount('fetchGooglePhotosPhotos'),
                dismissFromUser ? 1 : 0);

            wallpaperProvider.reset();

            // Simulate hiding |googlePhotosPhotosElement| and verify the
            // |error| is dismissed though not |fromUser|.
            const dismissCallbackPromise = new Promise<boolean>(resolve => {
              personalizationStore.data.error!.dismiss!.callback = resolve;
            });
            googlePhotosPhotosElement.hidden = true;
            assertEquals(await dismissCallbackPromise, /*fromUser=*/ false);
            await new Promise<void>(resolve => setTimeout(resolve));
            assertEquals(
                wallpaperProvider.getCallCount('fetchGooglePhotosPhotos'), 0);
          }));

  test('displays photos', async () => {
    const photos: GooglePhotosPhoto[] = [
      // Section of photos without location.
      {
        id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
        name: 'foo',
        date: toString16('Wednesday, February 16, 2022'),
        url: {url: 'foo.com'},
        location: undefined
      },
      // Section of photos with one location.
      {
        id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
        name: 'bar',
        date: toString16('Friday, November 12, 2021'),
        url: {url: 'bar.com'},
        location: 'home1'
      },
      {
        id: '0a268a37-877a-4936-81d4-38cc84b0f596',
        name: 'baz',
        date: toString16('Friday, November 12, 2021'),
        url: {url: 'baz.com'},
        location: 'home1'
      },
      // Section of photos with different locations.
      {
        id: '0a5231as-97a2-42e1-bdbf-3e75870ca042',
        name: 'bare',
        date: toString16('Friday, July 16, 2021'),
        url: {url: 'bare.com'},
        location: 'home2'
      },
      {
        id: '0a268a11-877a-4936-81d4-38cc8s9dn396',
        name: 'baze',
        date: toString16('Friday, July 16, 2021'),
        url: {url: 'baze.com'},
        location: 'home3'
      }
    ];

    const sections =
        toSections(photos, /*photosPerRow=*/ getNumberOfGridItemsPerRow());

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosPhotos(photos);

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // The |personalizationStore| should be empty, so no info or |photos|
    // should be rendered initially.
    const photoRowInfo = '.photo-row-info:not([hidden])';
    assertEquals(querySelectorAll(photoRowInfo)!.length, 0);
    const photoSelector =
        'wallpaper-grid-item:not([hidden]).photo:not([placeholder])';
    assertEquals(querySelectorAll(photoSelector)!.length, 0);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    await fetchGooglePhotosPhotos(wallpaperProvider, personalizationStore);
    await waitAfterNextRender(googlePhotosPhotosElement);

    // The wallpaper controller is expected to impose max resolution.
    photos.forEach(photo => photo.url.url += '=s512');

    // Verify that the number of rendered row-info and |photos| is as expected.
    assertEquals(querySelectorAll(photoRowInfo)!.length, sections.length);
    assertEquals(querySelectorAll(photoSelector)!.length, photos.length);

    // Verify that the expected |sections| are rendered.
    let absoluteRowIndex = 0;
    sections.forEach(section => {
      section.rows.forEach((row, rowIndex) => {
        // Verify that the expected row is rendered.
        const rowEl = querySelector(
            `.row:not([hidden]):nth-of-type(${absoluteRowIndex + 1})`);
        assertNotEquals(rowEl, null);

        // Verify that the expected date is rendered.
        if (rowIndex === 0) {
          const dateEl = rowEl!.querySelector(`${photoRowInfo} .date`);
          assertNotEquals(dateEl, null);
          assertEquals(dateEl!.innerHTML, section.date);
        }

        // Verify that the expected location is rendered.
        if (rowIndex === 0) {
          const locationEl = rowEl!.querySelector(`${photoRowInfo} .location`);
          assertNotEquals(locationEl, null);
          assertEquals(
              locationEl!.innerHTML,
              Array.from(section.locations).sort().join(' · '));
        }

        // Verify that the expected |photos| are rendered.
        row.forEach((photo, photoIndex) => {
          const photoEl =
              rowEl!.querySelector(
                  `${photoSelector}:nth-of-type(${photoIndex + 1})`) as
                  WallpaperGridItem |
              null;
          assertNotEquals(photoEl, null);
          assertEquals(photoEl!.imageSrc, photo.url.url);
          assertEquals(photoEl!.primaryText, undefined);
          assertEquals(photoEl!.secondaryText, undefined);
        });

        ++absoluteRowIndex;
      });
    });
  });

  test('displays photo selected', async () => {
    const photo: GooglePhotosPhoto = {
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'},
      location: 'home1'
    };

    const anotherPhoto: GooglePhotosPhoto = {
      id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
      name: 'bar',
      date: {data: []},
      url: {url: 'bar.com'},
      location: 'home2'
    };

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosPhotos([photo, anotherPhoto]);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    await fetchGooglePhotosPhotos(wallpaperProvider, personalizationStore);

    // The wallpaper controller is expected to impose max resolution.
    photo.url.url += '=s512';
    anotherPhoto.url.url += '=s512';

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify that the expected photos are rendered.
    const photoSelector = 'wallpaper-grid-item:not([hidden]).photo';
    const photoEls = querySelectorAll(photoSelector) as WallpaperGridItem[];
    assertEquals(photoEls.length, 2);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, false);

    // Start a pending selection for |photo|.
    personalizationStore.data.wallpaper.pendingSelected = photo;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, true);
    assertEquals(photoEls[1]!.selected, false);

    // Complete the pending selection.
    personalizationStore.data.wallpaper.pendingSelected = null;
    personalizationStore.data.wallpaper.currentSelected = {
      url: photo.url,
      attribution: [],
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kOnceGooglePhotos,
      key: photo.id
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, true);
    assertEquals(photoEls[1]!.selected, false);

    // Start a pending selection for |anotherPhoto|.
    personalizationStore.data.wallpaper.pendingSelected = anotherPhoto;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, true);

    // Complete the pending selection.
    personalizationStore.data.wallpaper.pendingSelected = null;
    personalizationStore.data.wallpaper.currentSelected = {
      url: anotherPhoto.url,
      attribution: [],
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kOnceGooglePhotos,
      key: anotherPhoto.id
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, true);

    // Start a pending selection for a |FilePath| backed wallpaper.
    personalizationStore.data.wallpaper.pendingSelected = {path: '//foo'};
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, false);

    // Complete the pending selection.
    personalizationStore.data.wallpaper.pendingSelected = null;
    personalizationStore.data.wallpaper.currentSelected = {
      url: {url: 'foo://'},
      attribution: [],
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kCustomized,
      key: '//foo'
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, false);
  });

  test('displays placeholders until photos are present', async () => {
    // Prepare Google Photos data.
    const photosCount = 5;
    const photos: GooglePhotosPhoto[] =
        Array.from({length: photosCount}, (_, i) => ({
                                            id: `id-${i}`,
                                            name: `name-${i}`,
                                            date: {data: []},
                                            url: {url: `url-${i}`},
                                            location: `location-${i}`,
                                          }));

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Initially only placeholders should be present.
    const selector =
        '.row:not([hidden]) wallpaper-grid-item:not([hidden]).photo';
    const photoSelector = `${selector}:not([placeholder])`;
    const placeholderSelector = `${selector}[placeholder]`;
    assertEquals(querySelectorAll(photoSelector)!.length, 0);
    const placeholderEls = querySelectorAll(placeholderSelector);
    assertNotEquals(placeholderEls!.length, 0);

    // Placeholders should be aria-labeled.
    placeholderEls!.forEach(placeholderEl => {
      assertEquals(placeholderEl.getAttribute('aria-label'), 'Loading');
    });

    // Clicking a placeholder should do nothing.
    const clickHandler = 'selectGooglePhotosPhoto';
    (placeholderEls![0] as HTMLElement).click();
    await new Promise<void>(resolve => setTimeout(resolve));
    assertEquals(wallpaperProvider.getCallCount(clickHandler), 0);

    // Provide Google Photos data.
    personalizationStore.data.wallpaper.googlePhotos.photos = photos;
    personalizationStore.notifyObservers();

    // Only photos should be present.
    await waitAfterNextRender(googlePhotosPhotosElement);
    const photoEls = querySelectorAll(photoSelector);
    assertNotEquals(photoEls!.length, 0);
    assertEquals(querySelectorAll(placeholderSelector)!.length, 0);

    // Photos should be aria-labeled.
    photoEls!.forEach((photoEl, i) => {
      assertEquals(photoEl.getAttribute('aria-label'), photos[i]!.name);
    });

    // Clicking a photo should do something.
    (photoEls![0] as HTMLElement).click();
    assertEquals(
        await wallpaperProvider.whenCalled(clickHandler), photos[0]!.id);
  });

  test('incrementally loads photos', async () => {
    // Set initial list of photos returned by |wallpaperProvider|.
    let nextPhotoId = 1;
    const photosCount = 200;
    wallpaperProvider.setGooglePhotosPhotos(
        Array.from({length: photosCount / 2}).map(() => {
          return {
            id: `id-${nextPhotoId}`,
            name: `name-${nextPhotoId}`,
            date: {data: []},
            url: {url: `url-${nextPhotoId}`},
            location: `location-${nextPhotoId++}`,
          };
        }));

    // Set initial photos resume token returned  by |wallpaperProvider|. When
    // resume token is defined, it indicates additional photos exist.
    const resumeToken = 'resumeToken';
    wallpaperProvider.setGooglePhotosPhotosResumeToken(resumeToken);

    // Initialize Google Photos data in |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    await fetchGooglePhotosPhotos(wallpaperProvider, personalizationStore);
    assertDeepEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosPhotos'),
        [/*itemId=*/ null, /*albumId=*/ null, /*resumeToken=*/ null]);

    // Reset |wallpaperProvider| expectations.
    wallpaperProvider.resetResolver('fetchGooglePhotosPhotos');

    // Set the next list of photos returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosPhotos(
        Array.from({length: photosCount / 2}).map(() => {
          return {
            id: `id-${nextPhotoId}`,
            name: `name-${nextPhotoId}`,
            date: {data: []},
            url: {url: `url-${nextPhotoId}`},
            location: `location-${nextPhotoId++}`,
          };
        }));

    // Set the next photos resume token returned by |wallpaperProvider|. When
    // resume token is undefined, it indicates no additional photos exist.
    wallpaperProvider.setGooglePhotosPhotosResumeToken(undefined);

    // Restrict the viewport so that |googlePhotosPhotosElement| will lazily
    // create photos instead of creating them all at once.
    const style = document.createElement('style');
    style.appendChild(document.createTextNode(`
      html,
      body {
        height: 100%;
        width: 100%;
      }
    `));
    document.head.appendChild(style);

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Scroll to the bottom of the grid.
    const gridScrollThreshold = googlePhotosPhotosElement.$.gridScrollThreshold;
    scrollToBottom(gridScrollThreshold);

    // Wait for and verify that the next batch of photos have been requested.
    assertDeepEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosPhotos'),
        [/*itemId=*/ null, /*albumId=*/ null, /*resumeToken=*/ resumeToken]);
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Reset |wallpaperProvider| expectations.
    wallpaperProvider.resetResolver('fetchGooglePhotosPhotos');

    // Scroll to the bottom of the grid.
    scrollToBottom(gridScrollThreshold);

    // Verify that no next batch of photos has been requested.
    assertEquals(wallpaperProvider.getCallCount('fetchGooglePhotosPhotos'), 0);
  });

  test('regenerates placeholders on resize', async () => {
    // Mock |window.innerWidth|.
    window.innerWidth = 721;

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    const rowSelector = '.row:not([hidden])';

    // No photos should be present.
    const selector = `${rowSelector} wallpaper-grid-item:not([hidden]).photo`;
    const photoSelector = `${selector}:not([placeholder])`;
    assertEquals(querySelectorAll(photoSelector)!.length, 0);

    // Only placeholders should be present and there should be exactly four
    // placeholders per row given the mocked |window.innerWidth|.
    const placeholderSelector = `${selector}[placeholder]`;
    assertEquals(querySelectorAll(placeholderSelector)!.length % 4, 0);
    querySelectorAll(rowSelector)!.forEach(rowEl => {
      assertEquals(rowEl.querySelectorAll(placeholderSelector)!.length, 4);
    });

    // Mock |window.innerWidth| and dispatch a resize event.
    window.innerWidth = 720;
    googlePhotosPhotosElement.dispatchEvent(new CustomEvent('iron-resize'));
    await waitAfterNextRender(googlePhotosPhotosElement);

    // No photos should be present.
    assertEquals(querySelectorAll(photoSelector)!.length, 0);

    // Only placeholders should be present and there should be exactly three
    // placeholders per row given the mocked |window.innerWidth|.
    assertEquals(querySelectorAll(placeholderSelector)!.length % 3, 0);
    querySelectorAll(rowSelector)!.forEach(rowEl => {
      assertEquals(rowEl.querySelectorAll(placeholderSelector)!.length, 3);
    });
  });

  test('reattempts failed photos load on show', async () => {
    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosPhotos(undefined);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    await fetchGooglePhotosPhotos(wallpaperProvider, personalizationStore);
    wallpaperProvider.reset();

    // Initialize |googlePhotosPhotosElement| in hidden state.
    googlePhotosPhotosElement = initElement(GooglePhotosPhotos, {hidden: true});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify that showing |googlePhotosPhotosElement| results in an automatic
    // reattempt to fetch photos.
    assertEquals(wallpaperProvider.getCallCount('fetchGooglePhotosPhotos'), 0);
    googlePhotosPhotosElement.hidden = false;
    await waitAfterNextRender(googlePhotosPhotosElement);
    assertDeepEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosPhotos'),
        [/*itemId=*/ null, /*albumId=*/ null, /*resumeToken=*/ null]);

    // Only placeholders should be present while loading.
    const selector = 'wallpaper-grid-item:not([hidden]).photo';
    const photoSelector = `${selector}:not([placeholder])`;
    const placeholderSelector = `${selector}[placeholder]`;
    assertEquals(querySelectorAll(photoSelector)!.length, 0);
    assertNotEquals(querySelectorAll(placeholderSelector)!.length, 0);
  });

  test('selects photo', async () => {
    const photo: GooglePhotosPhoto = {
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'},
      location: 'home'
    };

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosPhotos([photo]);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    await fetchGooglePhotosPhotos(wallpaperProvider, personalizationStore);

    // The wallpaper controller is expected to impose max resolution.
    photo.url.url += '=s512';

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify that the expected |photo| is rendered.
    const photoSelector = 'wallpaper-grid-item:not([hidden]).photo';
    const photoEls = querySelectorAll(photoSelector) as WallpaperGridItem[];
    assertEquals(photoEls.length, 1);
    assertEquals(photoEls[0]!.imageSrc, photo.url.url);
    assertEquals(photoEls[0]!.primaryText, undefined);
    assertEquals(photoEls[0]!.secondaryText, undefined);

    // Select |photo| and verify selection started.
    photoEls[0]!.click();
    assertEquals(personalizationStore.data.wallpaper.loading.setImage, 1);
    assertEquals(personalizationStore.data.wallpaper.loading.selected, true);
    assertDeepEquals(
        personalizationStore.data.wallpaper.pendingSelected,
        {...photo, index: 0});

    // Wait for and verify hard-coded selection failure.
    const methodName = 'selectGooglePhotosPhoto';
    wallpaperProvider.selectGooglePhotosPhotoResponse = false;
    assertEquals(await wallpaperProvider.whenCalled(methodName), photo.id);
    await waitAfterNextRender(googlePhotosPhotosElement);
    assertEquals(personalizationStore.data.wallpaper.loading.setImage, 0);
    assertEquals(personalizationStore.data.wallpaper.loading.selected, false);
    assertEquals(personalizationStore.data.wallpaper.pendingSelected, null);
  });
});
