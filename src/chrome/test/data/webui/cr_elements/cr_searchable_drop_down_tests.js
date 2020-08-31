// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_searchable_drop_down/cr_searchable_drop_down.m.js';
// #import {Polymer, html, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// clang-format on

suite('cr-searchable-drop-down', function() {
  /** @type {CrSearchableDropDownElement} */
  let dropDown;

  /** @type {HTMLElement} */
  let outsideElement;

  /** @type {CrInputElement} */
  let searchInput;

  /**
   * @param {!Array<string>} items The list of items to be populated in the
   *   drop down.
   */
  function setItems(items) {
    dropDown.items = items;
    Polymer.dom.flush();
  }

  /** @return {!NodeList} */
  function getList() {
    return dropDown.shadowRoot.querySelectorAll('.list-item');
  }

  /**
   * @param {string} searchTerm The string used to filter the list of items in
   *  the drop down.
   */
  function search(searchTerm) {
    const input = dropDown.shadowRoot.querySelector('cr-input');
    input.value = searchTerm;
    input.fire('input');
    Polymer.dom.flush();
  }

  function blur() {
    const input = dropDown.shadowRoot.querySelector('cr-input');
    input.fire('blur');
    Polymer.dom.flush();
  }

  function down() {
    MockInteractions.keyDownOn(searchInput, 'ArrowDown', [], 'ArrowDown');
  }

  function up() {
    MockInteractions.keyDownOn(searchInput, 'ArrowUp', [], 'ArrowUp');
  }

  function enter() {
    MockInteractions.keyDownOn(searchInput, 'Enter', [], 'Enter');
  }

  function tab() {
    MockInteractions.keyDownOn(searchInput, 'Tab', [], 'Tab');
  }

  function pointerDown(element) {
    element.dispatchEvent(new PointerEvent('pointerdown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      buttons: 1,
    }));
  }

  function getSelectedElement() {
    return dropDown.shadowRoot.querySelector('[selected_]');
  }

  setup(function() {
    PolymerTest.clearBody();
    document.body.innerHTML = `
      <p id="outside">Nothing to see here</p>
      <cr-searchable-drop-down label="test drop down">
      </cr-searchable-drop-down>
    `;
    dropDown = document.querySelector('cr-searchable-drop-down');
    outsideElement = document.querySelector('#outside');
    searchInput = dropDown.$.search;
    Polymer.dom.flush();
  });

  test('correct list items', function() {
    setItems(['one', 'two', 'three']);

    const itemList = getList();

    assertEquals(3, itemList.length);
    assertEquals('one', itemList[0].textContent.trim());
    assertEquals('two', itemList[1].textContent.trim());
    assertEquals('three', itemList[2].textContent.trim());
  });

  test('filter works correctly', function() {
    setItems(['cat', 'hat', 'rat', 'rake']);

    search('c');
    assertEquals(1, getList().length);
    assertEquals('cat', getList()[0].textContent.trim());
    assertTrue(dropDown.invalid);

    search('at');
    assertEquals(3, getList().length);
    assertEquals('cat', getList()[0].textContent.trim());
    assertEquals('hat', getList()[1].textContent.trim());
    assertEquals('rat', getList()[2].textContent.trim());
    assertTrue(dropDown.invalid);

    search('ra');
    assertEquals(2, getList().length);
    assertEquals('rat', getList()[0].textContent.trim());
    assertEquals('rake', getList()[1].textContent.trim());
    assertTrue(dropDown.invalid);
  });

  test('value is set on click', function() {
    setItems(['dog', 'cat', 'mouse']);

    assertNotEquals('dog', dropDown.value);

    getList()[0].click();

    assertEquals('dog', dropDown.value);

    // Make sure final value does not change while searching.
    search('ta');
    assertEquals('dog', dropDown.value);
    assertTrue(dropDown.invalid);
  });

  // If the update-value-on-input flag is passed, final value should be whatever
  // is in the search box.
  test('value is set on click and on search', function() {
    dropDown.updateValueOnInput = true;
    setItems(['dog', 'cat', 'mouse']);

    assertNotEquals('dog', dropDown.value);

    getList()[0].click();

    assertEquals('dog', dropDown.value);

    // Make sure final value does change while searching.
    search('ta');
    assertEquals('ta', dropDown.value);
    assertFalse(dropDown.invalid);
  });

  test('click closes dropdown', function() {
    setItems(['dog', 'cat', 'mouse']);

    // Dropdown opening is tied to focus.
    dropDown.$.search.focus();
    assertTrue(dropDown.$$('iron-dropdown').opened);

    assertNotEquals('dog', dropDown.value);

    getList()[0].click();
    assertEquals('dog', dropDown.value);
    assertFalse(dropDown.$$('iron-dropdown').opened);
  });

  test('click outside closes dropdown', function() {
    setItems(['dog', 'cat', 'mouse']);

    // Dropdown opening is tied to focus.
    dropDown.$.search.focus();
    assertTrue(dropDown.$$('iron-dropdown').opened);
    assertNotEquals('dog', dropDown.value);

    pointerDown(outsideElement);
    assertNotEquals('dog', dropDown.value);
    assertFalse(dropDown.$$('iron-dropdown').opened);
  });

  test('tab closes dropdown', function() {
    setItems(['dog', 'cat', 'mouse']);

    // Dropdown opening is tied to focus.
    dropDown.$.search.focus();
    assertTrue(dropDown.$$('iron-dropdown').opened);

    tab();
    assertFalse(dropDown.$$('iron-dropdown').opened);
  });

  test('selected moves after up/down', function() {
    setItems(['dog', 'cat', 'mouse']);

    dropDown.$.search.focus();
    assertTrue(dropDown.$$('iron-dropdown').opened);

    assertEquals(null, getSelectedElement());

    down();
    assertEquals('dog', getSelectedElement().textContent.trim());
    down();
    assertEquals('cat', getSelectedElement().textContent.trim());
    down();
    assertEquals('mouse', getSelectedElement().textContent.trim());
    down();
    assertEquals('dog', getSelectedElement().textContent.trim());

    up();
    assertEquals('mouse', getSelectedElement().textContent.trim());
    up();
    assertEquals('cat', getSelectedElement().textContent.trim());
    up();
    assertEquals('dog', getSelectedElement().textContent.trim());
    up();
    assertEquals('mouse', getSelectedElement().textContent.trim());

    enter();
    assertEquals('mouse', dropDown.value);
    assertFalse(dropDown.$$('iron-dropdown').opened);
  });

  test('enter re-opens dropdown after selection', function() {
    setItems(['dog', 'cat', 'mouse']);

    dropDown.$.search.focus();
    assertTrue(dropDown.$$('iron-dropdown').opened);

    assertEquals(null, getSelectedElement());

    down();
    assertEquals('dog', getSelectedElement().textContent.trim());

    enter();
    assertEquals('dog', dropDown.value);
    assertFalse(dropDown.$$('iron-dropdown').opened);

    enter();
    assertTrue(dropDown.$$('iron-dropdown').opened);
    assertEquals(null, getSelectedElement());
  });

  test('focus and up selects last item', function() {
    setItems(['dog', 'cat', 'mouse']);

    dropDown.$.search.focus();
    assertTrue(dropDown.$$('iron-dropdown').opened);

    assertEquals(null, getSelectedElement());

    up();
    assertEquals('mouse', getSelectedElement().textContent.trim());
  });

  test('selected follows mouse', function() {
    setItems(['dog', 'cat', 'mouse']);

    dropDown.$.search.focus();
    assertTrue(dropDown.$$('iron-dropdown').opened);

    assertEquals(null, getSelectedElement());

    MockInteractions.move(getList()[1], {x: 0, y: 0}, {x: 0, y: 0}, 1);
    assertEquals('cat', getSelectedElement().textContent.trim());
    MockInteractions.move(getList()[2], {x: 0, y: 0}, {x: 0, y: 0}, 1);
    assertEquals('mouse', getSelectedElement().textContent.trim());

    // Interacting with the keyboard should update the selected element.
    up();
    assertEquals('cat', getSelectedElement().textContent.trim());

    // When the user moves the mouse again, the selected element should change.
    MockInteractions.move(getList()[0], {x: 0, y: 0}, {x: 0, y: 0}, 1);
    assertEquals('dog', getSelectedElement().textContent.trim());
  });

  test('input retains focus', function() {
    setItems(['dog', 'cat', 'mouse']);

    searchInput.focus();
    assertTrue(dropDown.$$('iron-dropdown').opened);
    assertEquals(searchInput, dropDown.shadowRoot.activeElement);

    assertEquals(null, getSelectedElement());

    down();
    assertEquals('dog', getSelectedElement().textContent.trim());
    assertEquals(searchInput, dropDown.shadowRoot.activeElement);
  });

  // If the error-message-allowed flag is passed and the |errorMessage| property
  // is set, then the error message should be displayed. If the |errorMessage|
  // property is not set or |errorMessageAllowed| is false, no error message
  // should be displayed.
  test('error message is displayed if set and allowed', function() {
    dropDown.errorMessageAllowed = true;
    dropDown.errorMessage = 'error message';

    const input = dropDown.$.search;

    assertEquals(dropDown.errorMessage, input.$.error.textContent);
    assertTrue(input.invalid);

    // Set |errorMessageAllowed| to false and verify no error message is shown.
    dropDown.errorMessageAllowed = false;

    assertNotEquals(dropDown.errorMessage, input.$.error.textContent);
    assertFalse(input.invalid);

    // Set |errorMessageAllowed| to true and verify it is displayed again.
    dropDown.errorMessageAllowed = true;

    assertEquals(dropDown.errorMessage, input.$.error.textContent);
    assertTrue(input.invalid);

    // Clearing |errorMessage| hides the error.
    dropDown.errorMessage = '';

    assertEquals(dropDown.errorMessage, input.$.error.textContent);
    assertFalse(input.invalid);
  });

  // The show-loading attribute should determine whether or not the loading
  // spinner and message are shown.
  test('loading spinner is shown and hidden', function() {
    const progress = dropDown.shadowRoot.querySelector('#loading-box');
    assertTrue(progress.hidden);

    dropDown.showLoading = true;
    assertFalse(progress.hidden);

    dropDown.showLoading = false;
    assertTrue(progress.hidden);
  });


  // The readonly attribute is passed through to the inner cr-input.
  test('readonly attribute', function() {
    const input = dropDown.shadowRoot.querySelector('cr-input');

    dropDown.readonly = true;
    assertTrue(input.readonly);

    dropDown.readonly = false;
    assertFalse(input.readonly);
  });

  // When a user types in the dropdown but does not choose a valid option, the
  // dropdown should revert to the previously selected option on loss of focus.
  test('value resets on loss of focus', function() {
    setItems(['dog', 'cat', 'mouse']);

    getList()[0].click();
    assertEquals('dog', searchInput.value);
    assertFalse(dropDown.invalid);

    // Make sure the search box value changes back to dog
    search('ta');
    assertTrue(dropDown.invalid);
    blur();
    assertEquals('dog', searchInput.value);
    assertFalse(dropDown.invalid);
  });

  // When a user types in the dropdown but does not choose a valid option, the
  // dropdown should keep the same text on loss of focus. (Only when
  // isupdateValueOnInput is set to true).
  test('value remains on loss of focus', function() {
    dropDown.updateValueOnInput = true;
    setItems(['dog', 'cat', 'mouse']);

    getList()[0].click();
    assertEquals('dog', searchInput.value);
    assertFalse(dropDown.invalid);

    // Make sure the search box value keeps the same text
    search('ta');
    assertFalse(dropDown.invalid);
    blur();
    assertEquals('ta', searchInput.value);
    assertFalse(dropDown.invalid);
  });

  // In certain cases when a user clicks their desired option from the dropdown,
  // the on-blur event is fired before the on-click event. This test is to
  // guarantee expected behavior given a proceeding blur event.
  test('blur event when option is clicked', function() {
    setItems(['cat', 'hat', 'rat', 'rake']);

    search('rat');
    assertEquals(1, getList().length);
    assertEquals('rat', getList()[0].textContent.trim());

    blur();
    getList()[0].click();

    assertEquals('rat', dropDown.value);
  });
});
