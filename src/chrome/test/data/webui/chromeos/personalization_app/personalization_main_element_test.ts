// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {Paths, PersonalizationMain, PersonalizationRouter} from 'chrome://personalization/trusted/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';

suite('PersonalizationMainTest', function() {
  let personalizationMainElement: PersonalizationMain|null;

  setup(() => {
    baseSetup();
  });

  teardown(async () => {
    await teardownElement(personalizationMainElement);
    personalizationMainElement = null;
  });

  test('links to user subpage', async () => {
    personalizationMainElement = initElement(PersonalizationMain);
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
    const userSubpageLink =
        personalizationMainElement!.shadowRoot!.getElementById(
            'userSubpageLink')!;
    userSubpageLink.click();
    const [path, queryParams] = await goToRoutePromise;
    assertEquals(Paths.User, path);
    assertDeepEquals({}, queryParams);
  });

  test('links to ambient subpage', async () => {
    loadTimeData.overrideValues({'isAmbientModeAllowed': true});
    personalizationMainElement = initElement(PersonalizationMain);
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
    const ambientSubpageLink =
        personalizationMainElement!.shadowRoot!.getElementById(
            'ambientSubpageLink')!;
    ambientSubpageLink.click();
    const [path, queryParams] = await goToRoutePromise;
    assertEquals(Paths.Ambient, path);
    assertDeepEquals({}, queryParams);
  });

  test('no links to ambient subpage', async () => {
    loadTimeData.overrideValues({'isAmbientModeAllowed': false});
    personalizationMainElement = initElement(PersonalizationMain);

    const ambientSubpageLink =
        personalizationMainElement!.shadowRoot!.getElementById(
            'ambientSubpageLink')!;
    assertFalse(!!ambientSubpageLink);
  });

  test('has ambient preview', async () => {
    loadTimeData.overrideValues({'isAmbientModeAllowed': true});
    personalizationMainElement = initElement(PersonalizationMain);
    await waitAfterNextRender(personalizationMainElement);

    const preview = personalizationMainElement!.shadowRoot!.querySelector(
        'ambient-preview')!;
    assertTrue(!!preview);
  });

  test('has no ambient preview', async () => {
    loadTimeData.overrideValues({'isAmbientModeAllowed': false});
    personalizationMainElement = initElement(PersonalizationMain);
    await waitAfterNextRender(personalizationMainElement);


    const preview = personalizationMainElement!.shadowRoot!.querySelector(
        'ambient-preview')!;
    assertFalse(!!preview);
  });
});
