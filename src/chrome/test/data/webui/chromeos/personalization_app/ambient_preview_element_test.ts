// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {AmbientObserver, AmbientPreview, Paths, PersonalizationRouter, TopicSource} from 'chrome://personalization/trusted/personalization_app.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestAmbientProvider} from './test_ambient_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';


suite('AmbientPreviewTest', function() {
  let ambientPreviewElement: AmbientPreview|null;
  let ambientProvider: TestAmbientProvider;
  let personalizationStore: TestPersonalizationStore;
  const routerOriginal = PersonalizationRouter.instance;
  const routerMock = TestBrowserProxy.fromClass(PersonalizationRouter);

  setup(() => {
    const mocks = baseSetup();
    ambientProvider = mocks.ambientProvider;
    personalizationStore = mocks.personalizationStore;
    AmbientObserver.initAmbientObserverIfNeeded();
    PersonalizationRouter.instance = () => routerMock;
  });

  teardown(async () => {
    await teardownElement(ambientPreviewElement);
    ambientPreviewElement = null;
    AmbientObserver.shutdown();
    PersonalizationRouter.instance = routerOriginal;
  });

  test(
      'displays zero state message when ambient mode is disabled', async () => {
        personalizationStore.data.ambient.albums = ambientProvider.albums;
        personalizationStore.data.ambient.topicSource = TopicSource.kArtGallery;
        personalizationStore.data.ambient.ambientModeEnabled = false;
        const ambientPreviewElement = initElement(AmbientPreview);
        personalizationStore.notifyObservers();
        await waitAfterNextRender(ambientPreviewElement);

        const messageContainer =
            ambientPreviewElement.shadowRoot!.getElementById(
                'messageContainer');
        assertTrue(!!messageContainer);
        const textSpan = messageContainer.querySelector('span');
        assertTrue(!!textSpan);
        assertEquals(
            ambientPreviewElement.i18n('ambientModeMainPageZeroStateMessage'),
            textSpan.textContent);
      });

  test(
      'clicks turn on button enables ambient mode and navigates to ambient subpage',
      async () => {
        personalizationStore.data.ambient.albums = ambientProvider.albums;
        personalizationStore.data.ambient.topicSource = TopicSource.kArtGallery;
        personalizationStore.data.ambient.ambientModeEnabled = false;
        const ambientPreviewElement = initElement(AmbientPreview);
        personalizationStore.notifyObservers();
        await waitAfterNextRender(ambientPreviewElement);

        const messageContainer =
            ambientPreviewElement.shadowRoot!.getElementById(
                'messageContainer');
        assertTrue(!!messageContainer);
        const button = messageContainer.querySelector('cr-button');
        assertTrue(!!button);

        personalizationStore.setReducersEnabled(true);
        button.click();
        assertTrue(personalizationStore.data.ambient.ambientModeEnabled);

        const original = PersonalizationRouter.instance;
        const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
          PersonalizationRouter.instance = () => {
            return {
              goToRoute(path: Paths, queryParams: Object = {}) {
                resolve([path, queryParams]);
                PersonalizationRouter.instance = original;
              }
            } as PersonalizationRouter;
          };
        });
        const [path, queryParams] = await goToRoutePromise;
        assertEquals(Paths.Ambient, path);
        assertDeepEquals({}, queryParams);
      });
});
