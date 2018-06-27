// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
   * @param {Node} newChild
   */
  replace(parent, newChild) {
    while (parent.firstChild) parent.removeChild(parent.firstChild);
    parent.appendChild(newChild);
  },
};

{
  /**
   * State is represented in the query string and
   * can be manipulated by this object. Keys in the query match with
   * input names.
   */
  const _filterParams = new URLSearchParams(location.search.slice(1));

  /** Utilities for working with the state */
  const state = {
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
      let val = _filterParams.get(key);
      if (options.valid != null && !options.valid.has(val)) {
        val = null;
      }
      if (options.default != null && val == null) {
        val = options.default;
      }
      return val;
    },
    /**
     * Set the value in the state, overriding any existing value. Afterwards
     * display the new state in the URL by replacing the current history entry.
     * @param {string} name
     * @param {string} value
     */
    set(name, value) {
      _filterParams.set(name, value);
      history.replaceState(null, null, '?' + _filterParams.toString());
    },
  };

  // Update form inputs to reflect the state from URL.
  for (const [name, value] of _filterParams) {
    if (form[name] != null) form[name].value = value;
  }

  /**
   * Some form inputs can be changed without needing to refresh the tree.
   * When these inputs change, update the state and the change event can
   * be subscribed to elsewhere in the code.
   * @param {Event} event Change event fired when a dynamic value is updated
   */
  function _onDynamicValueChange(event) {
    const {name, value} = event.currentTarget;
    state.set(name, value);
  }
  for (const dynamicInput of document.querySelectorAll('form [data-dynamic]')) {
    dynamicInput.addEventListener('change', _onDynamicValueChange);
  }

  self.state = state;
}
