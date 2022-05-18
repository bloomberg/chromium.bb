// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {KeyboardBacklight, KeyboardBacklightActionName, KeyboardBacklightObserver, SetBacklightColorAction} from 'chrome://personalization/trusted/personalization_app.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestKeyboardBacklightProvider} from './test_keyboard_backlight_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

suite('KeyboardBacklightTest', function() {
  let keyboardBacklightElement: KeyboardBacklight|null;
  let keyboardBacklightProvider: TestKeyboardBacklightProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    keyboardBacklightProvider = mocks.keyboardBacklightProvider;
    personalizationStore = mocks.personalizationStore;
    KeyboardBacklightObserver.initKeyboardBacklightObserverIfNeeded();
  });

  teardown(async () => {
    await teardownElement(keyboardBacklightElement);
    keyboardBacklightElement = null;
    KeyboardBacklightObserver.shutdown();
  });


  test('displays content', async () => {
    keyboardBacklightElement = initElement(KeyboardBacklight);
    const labelContainer = keyboardBacklightElement.shadowRoot!.getElementById(
        'keyboardBacklightLabel');
    assertTrue(!!labelContainer);
    const text = labelContainer.querySelector('p');
    assertTrue(!!text);
    assertEquals(
        keyboardBacklightElement.i18n('keyboardBacklightTitle'),
        text.textContent);

    const selectorContainer =
        keyboardBacklightElement.shadowRoot!.getElementById('selector');
    assertTrue(!!selectorContainer);
    const colorContainers =
        selectorContainer.querySelectorAll('.color-container');
    assertEquals(9, colorContainers!.length);
  });

  test('sets backlight color when a color preset is clicked', async () => {
    keyboardBacklightElement = initElement(KeyboardBacklight);
    const selectorContainer =
        keyboardBacklightElement.shadowRoot!.getElementById('selector');
    assertTrue(!!selectorContainer);
    const colorContainers =
        selectorContainer.querySelectorAll('.color-container');
    assertEquals(9, colorContainers!.length);

    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_BACKLIGHT_COLOR);
    (colorContainers[1] as HTMLElement).click();
    await keyboardBacklightProvider.whenCalled('setBacklightColor');

    const action = await personalizationStore.waitForAction(
                       KeyboardBacklightActionName.SET_BACKLIGHT_COLOR) as
        SetBacklightColorAction;
    assertTrue(!!action.backlightColor);
    assertTrue(!!personalizationStore.data.keyboardBacklight.backlightColor);
  });

  test('sets backlight color in store on first load', async () => {
    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_BACKLIGHT_COLOR);
    keyboardBacklightElement = initElement(KeyboardBacklight);
    await keyboardBacklightProvider.whenCalled('setKeyboardBacklightObserver');
    keyboardBacklightProvider.fireOnBacklightColorChanged(
        keyboardBacklightProvider.backlightColor);
    const action = await personalizationStore.waitForAction(
                       KeyboardBacklightActionName.SET_BACKLIGHT_COLOR) as
        SetBacklightColorAction;
    assertEquals(
        keyboardBacklightProvider.backlightColor, action.backlightColor);
  });

  test('sets backlight color data in store on changed', async () => {
    await keyboardBacklightProvider.whenCalled('setKeyboardBacklightObserver');

    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_BACKLIGHT_COLOR);
    keyboardBacklightProvider.keyboardBacklightObserverRemote!
        .onBacklightColorChanged(keyboardBacklightProvider.backlightColor);

    const {backlightColor} =
        await personalizationStore.waitForAction(
            KeyboardBacklightActionName.SET_BACKLIGHT_COLOR) as
        SetBacklightColorAction;
    assertEquals(keyboardBacklightProvider.backlightColor, backlightColor);
  });
});
