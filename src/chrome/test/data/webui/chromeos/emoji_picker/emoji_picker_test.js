// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EMOJI_PICKER_TOTAL_EMOJI_WIDTH} from 'chrome://emoji-picker/constants.js';
import {EmojiButton} from 'chrome://emoji-picker/emoji_button.js';
import {EmojiPicker} from 'chrome://emoji-picker/emoji_picker.js';
import {EmojiPickerApiProxyImpl} from 'chrome://emoji-picker/emoji_picker_api_proxy.js';
import {EmojiVariants} from 'chrome://emoji-picker/emoji_variants.js';
import {EMOJI_DATA_LOADED, EMOJI_VARIANTS_SHOWN} from 'chrome://emoji-picker/events.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertGT, assertLT, assertTrue} from '../../chai_assert.js';

import {assertCloseTo, deepQuerySelector, dispatchMouseEvent, isGroupButtonActive, timeout, waitForCondition, waitForEvent, waitWithTimeout} from './emoji_picker_test_util.js';


suite('<emoji-picker>', () => {
  /** @type {!EmojiPicker} */
  let emojiPicker;
  /** @type {function(...!string): ?HTMLElement} */
  let findInEmojiPicker;

  setup(() => {
    // Reset DOM state.
    document.body.innerHTML = '';
    window.localStorage.clear();

    emojiPicker =
        /** @type {!EmojiPicker} */ (document.createElement('emoji-picker'));
    emojiPicker.emojiDataUrl = '/emoji_test_ordering';

    findInEmojiPicker = (...path) => deepQuerySelector(emojiPicker, path);

    // Wait until emoji data is loaded before executing tests.
    return new Promise((resolve) => {
      emojiPicker.addEventListener(EMOJI_DATA_LOADED, resolve);
      document.body.appendChild(emojiPicker);
      flush();
    });
  });

  test('custom element should be defined', () => {
    assert(customElements.get('emoji-picker'));
  });

  test('first non-chevron, tab should be active by default', async () => {
    const button = findInEmojiPicker(
        'emoji-group-button[data-group="history"]', 'cr-icon-button');
    assertFalse(isGroupButtonActive(button));
  });

  test('second non-chevron tab should be inactive by default', () => {
    const button = findInEmojiPicker(
        'emoji-group-button[data-group="0"]', 'cr-icon-button');
    assertTrue(isGroupButtonActive(button));
  });

  test('Highlight bar should under emotions on start', () => {
    const button = findInEmojiPicker('#bar');
    assertCloseTo(
        EMOJI_PICKER_TOTAL_EMOJI_WIDTH, parseFloat(button.style.left));
  });

  test('clicking second tab should activate it and scroll', async () => {
    // store initial scroll position of emoji groups.
    const emojiGroups = findInEmojiPicker('#groups');
    const initialScroll = emojiGroups.scrollTop;

    // History group doesn't exist when there is no history, so scrolling to
    // the first non-history group (0) may not trigger a scroll, so scroll to
    // group (1).
    const firstButton = findInEmojiPicker(
        'emoji-group-button[data-group="history"]', 'cr-icon-button');
    const thirdButton = findInEmojiPicker(
        'emoji-group-button[data-group="1"]', 'cr-icon-button');

    // wait so emoji-groups render and we have something to scroll to.
    await waitForCondition(
        () => findInEmojiPicker(
            '[data-group="2"] > emoji-group', 'emoji-button', 'button'));
    thirdButton.click();

    // wait while waiting for scroll to happen and update buttons.
    await waitForCondition(
        () => isGroupButtonActive(thirdButton) &&
            !isGroupButtonActive(firstButton));

    const newScroll = emojiGroups.scrollTop;
    assertGT(newScroll, initialScroll);

    // check the highlight bar also moved
    const bar = findInEmojiPicker('#bar');
    assertGT(parseInt(bar.style.left, 10), 0);
  });

  test('recently used should be hidden when empty', () => {
    const recentlyUsed =
        findInEmojiPicker('[data-group=history] > emoji-group');
    assert(!recentlyUsed);
  });

  test(
      'recently used should be populated after emoji is clicked normally',
      async () => {
        EmojiPickerApiProxyImpl.getInstance().isIncognitoTextField = () =>
            new Promise((resolve) => resolve({incognito: false}));
        // yield to allow emoji-group and emoji buttons to render.
        const emojiButton = await waitForCondition(
            () => findInEmojiPicker(
                '[data-group="0"] > emoji-group', 'emoji-button', 'button'));
        emojiButton.click();

        // wait until emoji exists in recently used section.
        const recentlyUsed = await waitForCondition(
            () => findInEmojiPicker(
                '[data-group=history] > emoji-group', 'emoji-button',
                'button'));

        // check text is correct.
        const recentText = recentlyUsed.innerText;
        assertTrue(recentText.includes(String.fromCodePoint(128512)));
      });

  test(
      'clicking an emoji with no text field should copy it to the clipboard',
      async () => {
        // Note: this whole test has no text field, so we should always copy to
        // the clipboard.
        EmojiPickerApiProxyImpl.getInstance().isIncognitoTextField = () =>
            new Promise((resolve) => resolve({incognito: false}));
        // yield to allow emoji-group and emoji buttons to render.
        const emojiButton = await waitForCondition(
            () => findInEmojiPicker(
                '[data-group="0"] > emoji-group', 'emoji-button', 'button'));
        emojiButton.click();

        // wait until emoji exists in recently used section.
        const recentlyUsed = await waitForCondition(
            () => findInEmojiPicker(
                '[data-group=history] > emoji-group', 'emoji-button',
                'button'));

        // check text is correct.
        await (waitForCondition(async () => {
          const clipboardtext = await navigator.clipboard.readText();
          return clipboardtext === String.fromCodePoint(128512);
        }));
      });

  test('recently-used should have variants for variant emoji', async () => {
    EmojiPickerApiProxyImpl.getInstance().isIncognitoTextField = () =>
        new Promise((resolve) => resolve({incognito: false}));
    // yield to allow emoji-group and emoji buttons to render.
    const emojiButton = (await waitForCondition(
                             () => findInEmojiPicker(
                                 '[data-group="0"] > emoji-group',
                                 'emoji-button:nth-child(3)')))
                            .shadowRoot.querySelector('button');
    emojiButton.click();

    // wait until emoji exists in recently used section.
    const recentlyUsed =
        (await waitForCondition(
             () => findInEmojiPicker(
                 '[data-group=history] > emoji-group', 'emoji-button')))
            .shadowRoot.querySelector('button');

    // check variants class is applied
    assertTrue(recentlyUsed.classList.contains('has-variants'));
  });

  test(
      'recently-used should have no variants for non-variant emoji',
      async () => {
        EmojiPickerApiProxyImpl.getInstance().isIncognitoTextField = () =>
            new Promise((resolve) => resolve({incognito: false}));
        // yield to allow emoji-group and emoji buttons to render.
        const emojiButton = (await waitForCondition(
                                 () => findInEmojiPicker(
                                     '[data-group="0"] > emoji-group',
                                     'emoji-button:nth-child(2)')))
                                .shadowRoot.querySelector('button');
        emojiButton.click();

        // wait until emoji exists in recently used section.
        const recentlyUsed =
            (await waitForCondition(
                 () => findInEmojiPicker(
                     '[data-group=history] > emoji-group', 'emoji-button')))
                .shadowRoot.querySelector('button');

        // check variants class is not applied
        assertFalse(recentlyUsed.classList.contains('has-variants'));
      });

  test(
      'recently-used should be empty after emoji is clicked in incognito mode',
      async () => {
        EmojiPickerApiProxyImpl.getInstance().isIncognitoTextField = () =>
            new Promise((resolve) => resolve({incognito: true}));
        // yield to allow emoji-group and emoji buttons to render.
        const emojiButton = await waitForCondition(
            () => findInEmojiPicker(
                '[data-group="0"] > emoji-group', 'emoji-button', 'button'));
        emojiButton.click();

        // Wait to ensure recents has a chance to render if we have a bug.
        await timeout(1000);

        const recentlyUsed =
            findInEmojiPicker('[data-group=history] > emoji-group');
        assert(!recentlyUsed);
      });


  suite('<emoji-variants>', () => {
    /** @type {!EmojiButton} */
    let firstEmojiButton;

    /** @type {function(!EmojiButton): ?EmojiVariants} */
    const findEmojiVariants = el => {
      const variants =
          /** @type {?EmojiVariants} */ (el.querySelector('emoji-variants'));
      return variants && variants.style.display !== 'none' ? variants : null;
    };

    setup(async () => {
      firstEmojiButton = (await waitForCondition(
                              () => findInEmojiPicker(
                                  '[data-group="0"] > emoji-group',
                                  'emoji-button:nth-child(3)')))
                             .shadowRoot;


      // right click and wait for variants to appear.
      const variantsPromise = waitForEvent(emojiPicker, EMOJI_VARIANTS_SHOWN);
      dispatchMouseEvent(firstEmojiButton.querySelector('button'), 2);

      await waitWithTimeout(
          variantsPromise, 1000, 'did not receive emoji variants event.');
      await waitForCondition(
          () => findEmojiVariants(firstEmojiButton),
          'emoji-variants failed to appear.', 5000);
    });

    test('right clicking emoji again should close popup', async () => {
      // right click again and variants should disappear.
      dispatchMouseEvent(firstEmojiButton.querySelector('button'), 2);
      await waitForCondition(
          () => !findEmojiVariants(firstEmojiButton),
          'emoji-variants failed to disappear.');
    });

    test('clicking elsewhere should close popup', async () => {
      // click in some empty space of main emoji picker.
      emojiPicker.click();

      await waitForCondition(
          () => !findEmojiVariants(firstEmojiButton),
          'emoji-variants failed to disappear.');
    });

    test('opening different variants should close first variants', async () => {
      const emojiButton2 = await waitForCondition(
          () =>
              findInEmojiPicker(
                  '[data-group="0"] > emoji-group', 'emoji-button:nth-child(4)')
                  .shadowRoot);

      // right click on second emoji button
      dispatchMouseEvent(emojiButton2.querySelector('button'), 2);
      // ensure first popup disappears and second popup appears.
      await waitForCondition(
          () => !findEmojiVariants(firstEmojiButton),
          'first emoji-variants failed to disappear.');
      await waitForCondition(
          () => findEmojiVariants(emojiButton2),
          'second emoji-variants failed to appear.');
    });

    test('opening variants on the left side should not overflow', async () => {
      const groupsRect = emojiPicker.getBoundingClientRect();

      const variants = findEmojiVariants(firstEmojiButton);
      const variantsRect = variants.getBoundingClientRect();

      assertLT(groupsRect.left, variantsRect.left);
      assertLT(variantsRect.right, groupsRect.right);
    });

    test('opening large variants (twice) should not overflow', async () => {
      const coupleEmojiButton = await waitForCondition(
          () =>
              findInEmojiPicker(
                  '[data-group="0"] > emoji-group', 'emoji-button:nth-child(5)')
                  .shadowRoot);

      // listen for emoji variants event.
      const variantsPromise = waitForEvent(emojiPicker, EMOJI_VARIANTS_SHOWN);

      // right click on couple emoji to show 5x5 grid with skin tone.
      dispatchMouseEvent(coupleEmojiButton.querySelector('button'), 2);
      const variants =
          await waitForCondition(() => findEmojiVariants(coupleEmojiButton));

      // ensure variants are positioned before we get bounding rectangle.
      await waitWithTimeout(
          variantsPromise, 1000, 'did not receive emoji variants event.');
      const variantsRect = variants.getBoundingClientRect();
      const pickerRect = emojiPicker.getBoundingClientRect();

      assertLT(pickerRect.left, variantsRect.left);
      assertLT(variantsRect.right, pickerRect.right);

      // Now close the variants and reopen, should still be positioned properly.
      emojiPicker.click();
      await waitForCondition(
          () => !findEmojiVariants(firstEmojiButton),
          'emoji-variants failed to disappear.');

      const variantsPromise2 = waitForEvent(emojiPicker, EMOJI_VARIANTS_SHOWN);
      // reshow
      dispatchMouseEvent(coupleEmojiButton.querySelector('button'), 2);
      const variants2 =
          await waitForCondition(() => findEmojiVariants(coupleEmojiButton));
      // ensure variants are positioned before we get bounding rectangle.
      await waitWithTimeout(
          variantsPromise2, 1000,
          'did not receive second emoji variants event.');
      const variantsRect2 = variants2.getBoundingClientRect();

      assertLT(pickerRect.left, variantsRect2.left);
      assertLT(variantsRect2.right, pickerRect.right);
    });
  });

  suite('<emoji-search>', () => {
    test('works when there are no results', async () => {
      // This test just ensures that no errors are thrown.
      const enterEvent = new KeyboardEvent(
          'keydown', {cancelable: true, key: 'Enter', keyCode: 13});
      const search = findInEmojiPicker('emoji-search');
      search.onKeyDown(enterEvent);
    });
  });
});
