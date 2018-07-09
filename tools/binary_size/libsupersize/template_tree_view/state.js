// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check
'use strict';

/**
 * @fileoverview
 * Methods for manipulating the state and the DOM of the page
 */

/** @type {HTMLFormElement} Form containing options and filters */
const form = document.getElementById('options');

/** Utilities for working with the DOM */
const dom = {
  /**
   * Create a document fragment from the given nodes
   * @param {Iterable<Node>} nodes
   * @returns {DocumentFragment}
   */
  createFragment(nodes) {
    const fragment = document.createDocumentFragment();
    for (const node of nodes) fragment.appendChild(node);
    return fragment;
  },
  /**
   * Removes all the existing children of `parent` and inserts
   * `newChild` in their place
   * @param {Node} parent
   * @param {Node | null} newChild
   */
  replace(parent, newChild) {
    while (parent.firstChild) parent.removeChild(parent.firstChild);
    if (newChild != null) parent.appendChild(newChild);
  },
};

/** Build utilities for working with the state. */
function _initState() {
  /**
   * State is represented in the query string and
   * can be manipulated by this object. Keys in the query match with
   * input names.
   */
  let _filterParams = new URLSearchParams(location.search.slice(1));

  const state = Object.freeze({
    /**
     * Returns a string from the current query string state.
     * Can optionally restrict valid values for the query.
     * Values not present in the query will return null, or the default
     * value if supplied.
     * @param {string} key
     * @param {object} [options]
     * @param {string} [options.default] Default to use if key is not present
     * in the state
     * @param {Set<string>} [options.valid] If provided, values must be in this
     * set to be returned. Invalid values will return null or `defaultValue`.
     * @returns {string | null}
     */
    get(key, options = {}) {
      const [val = null] = state.getAll(key, {
        default: options.default ? [options.default] : null,
        valid: options.valid,
      });
      return val;
    },
    /**
     * Returns all string values for a key from the current query string state.
     * Can optionally provide default values used if there are no values.
     * @param {string} key
     * @param {object} [options]
     * @param {string[]} [options.default] Default to use if key is not present
     * in the state.
     * @param {Set<string>} [options.valid] If provided, values must be in this
     * set to be returned. Invalid values will be omitted.
     * @returns {string[]}
     */
    getAll(key, options = {}) {
      let vals = _filterParams.getAll(key);
      if (options.valid != null) {
        vals = vals.filter(val => options.valid.has(val));
      }
      if (options.default != null && vals.length === 0) {
        vals = options.default;
      }
      return vals;
    },
    /**
     * Checks if a key is present in the query string state.
     * @param {string} key
     * @returns {boolean}
     */
    has(key) {
      return _filterParams.has(key);
    },
  });

  // Update form inputs to reflect the state from URL.
  for (const element of form.elements) {
    if (element.name) {
      const input = /** @type {HTMLInputElement} */ (element);
      const values = _filterParams.getAll(input.name);
      const [value] = values;
      if (value) {
        switch (input.type) {
          case 'checkbox':
            input.checked = values.includes(input.value);
            break;
          case 'radio':
            input.checked = value === input.value;
            break;
          default:
            input.value = value;
            break;
        }
      }
    }
  }

  // Update the state when the form changes.
  form.addEventListener('change', event => {
    _filterParams = new URLSearchParams(new FormData(event.currentTarget));
    history.replaceState(null, null, '?' + _filterParams.toString());
  });

  return state;
}

function _startListeners() {
  const _SHOW_OPTIONS_STORAGE_KEY = 'show-options';

  /**
   * The settings dialog on the side can be toggled on and off by elements with
   * a 'toggle-options' class.
   */
  function _toggleOptions() {
    const openedOptions = document.body.classList.toggle('show-options');
    localStorage.setItem(_SHOW_OPTIONS_STORAGE_KEY, openedOptions.toString());
  }
  for (const button of document.getElementsByClassName('toggle-options')) {
    button.addEventListener('click', _toggleOptions);
  }
  if (localStorage.getItem(_SHOW_OPTIONS_STORAGE_KEY) === 'true') {
    document.body.classList.add('show-options');
  }

  /**
   * Disable some fields when method_count is set
   */
  function setMethodCountModeUI() {
    const sizeHeader = document.getElementById('size-header');
    const typesFilterContainer = document.getElementById('types-filter');
    if (form.method_count.checked) {
      sizeHeader.textContent = 'Methods';
      form.byteunit.setAttribute('disabled', '');
      typesFilterContainer.setAttribute('disabled', '');
    } else {
      sizeHeader.textContent = sizeHeader.dataset.value;
      form.byteunit.removeAttribute('disabled', '');
      typesFilterContainer.removeAttribute('disabled');
    }
  }
  setMethodCountModeUI();
  form.method_count.addEventListener('change', setMethodCountModeUI);
}

/** Utilities for working with the state */
const state = _initState();
_startListeners();
