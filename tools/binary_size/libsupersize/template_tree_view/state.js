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
  /**
   * Builds a text element in a single statement.
   * @param {string} tagName Type of the element, such as "span".
   * @param {string} text Text content for the element.
   * @param {string} [className] Class to apply to the element.
   */
  textElement(tagName, text, className) {
    const element = document.createElement(tagName);
    element.textContent = text;
    if (className) element.className = className;
    return element;
  },
};

/** Build utilities for working with the state. */
function _initState() {
  const _DEFAULT_FORM = new FormData(form);

  /**
   * State is represented in the query string and
   * can be manipulated by this object. Keys in the query match with
   * input names.
   */
  let _filterParams = new URLSearchParams(location.search.slice(1));
  const typeList = _filterParams.getAll(_TYPE_STATE_KEY);
  _filterParams.delete(_TYPE_STATE_KEY);
  for (const type of types(typeList)) {
    _filterParams.append(_TYPE_STATE_KEY, type);
  }

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
    /**
     * Formats the filter state as a string.
     */
    toString() {
      const copy = new URLSearchParams(_filterParams);
      const types = [...new Set(copy.getAll(_TYPE_STATE_KEY))];
      if (types.length > 0) copy.set(_TYPE_STATE_KEY, types.join(''));
      return `?${copy.toString()}`;
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

  /**
   * Yields only entries that have been modified in
   * comparison to `_DEFAULT_FORM`.
   * @param {FormData} modifiedForm
   */
  function* onlyChangedEntries(modifiedForm) {
    // Remove default values
    for (const key of modifiedForm.keys()) {
      const modifiedValues = modifiedForm.getAll(key);
      const defaultValues = _DEFAULT_FORM.getAll(key);

      const valuesChanged =
        modifiedValues.length !== defaultValues.length ||
        modifiedValues.some((v, i) => v !== defaultValues[i]);
      if (valuesChanged) {
        for (const value of modifiedValues) {
          yield [key, value];
        }
      }
    }
  }

  // Update the state when the form changes.
  form.addEventListener('change', () => {
    const modifiedForm = new FormData(form);
    _filterParams = new URLSearchParams(onlyChangedEntries(modifiedForm));
    history.replaceState(null, null, state.toString());
  });

  return state;
}

function _startListeners() {
  const _SHOW_OPTIONS_STORAGE_KEY = 'show-options';

  /** @type {HTMLFieldSetElement} */
  const typesFilterContainer = document.getElementById('types-filter');
  /** @type {HTMLInputElement} */
  const methodCountInput = form.elements.namedItem('method_count');
  /** @type {HTMLFieldSetElement} */
  const byteunit = form.elements.namedItem('byteunit');
  /** @type {HTMLCollectionOf<HTMLInputElement>} */
  const typeCheckboxes = form.elements.namedItem(_TYPE_STATE_KEY);
  /** @type {HTMLSpanElement} */
  const sizeHeader = document.getElementById('size-header');

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
  // Default to open if getItem returns null
  if (localStorage.getItem(_SHOW_OPTIONS_STORAGE_KEY) !== 'false') {
    document.body.classList.add('show-options');
  }

  /**
   * Disable some fields when method_count is set
   */
  function setMethodCountModeUI() {
    if (methodCountInput.checked) {
      byteunit.setAttribute('disabled', '');
      typesFilterContainer.setAttribute('disabled', '');
      sizeHeader.textContent = 'Methods';
    } else {
      byteunit.removeAttribute('disabled');
      typesFilterContainer.removeAttribute('disabled');
      sizeHeader.textContent = 'Size';
    }
  }
  setMethodCountModeUI();
  methodCountInput.addEventListener('change', setMethodCountModeUI);

  document.getElementById('type-all').addEventListener('click', () => {
    for (const checkbox of typeCheckboxes) {
      checkbox.checked = true;
    }
    form.dispatchEvent(new Event('change'));
  });
  document.getElementById('type-none').addEventListener('click', () => {
    for (const checkbox of typeCheckboxes) {
      checkbox.checked = false;
    }
    form.dispatchEvent(new Event('change'));
  });
}

function _makeIconTemplateGetter() {
  const _icons = document.getElementById('icons');

  /**
   * @type {{[type:string]: SVGSVGElement}} Icon elements
   * that correspond to each symbol type.
   */
  const symbolIcons = {
    D: _icons.querySelector('.foldericon'),
    C: _icons.querySelector('.componenticon'),
    J: _icons.querySelector('.javaclassicon'),
    F: _icons.querySelector('.fileicon'),
    b: _icons.querySelector('.bssicon'),
    d: _icons.querySelector('.dataicon'),
    r: _icons.querySelector('.readonlyicon'),
    t: _icons.querySelector('.codeicon'),
    v: _icons.querySelector('.vtableicon'),
    '*': _icons.querySelector('.generatedicon'),
    x: _icons.querySelector('.dexicon'),
    m: _icons.querySelector('.dexmethodicon'),
    p: _icons.querySelector('.localpakicon'),
    P: _icons.querySelector('.nonlocalpakicon'),
    o: _icons.querySelector('.othericon'), // used as default icon
  };

  /** @type {Map<string, {color:string,description:string}>} */
  const iconInfoCache = new Map();

  /**
   * Returns the SVG icon template element corresponding to the given type.
   * @param {string} type Symbol type character.
   * @param {boolean} readonly If true, the original template is returned.
   * If false, a copy is returned that can be modified.
   * @returns {SVGSVGElement}
   */
  function getIconTemplate(type, readonly = false) {
    const iconTemplate = symbolIcons[type] || symbolIcons.o;
    return readonly ? iconTemplate : iconTemplate.cloneNode(true);
  }

  /**
   * Returns style info about SVG icon template element corresponding to the
   * given type.
   * @param {string} type Symbol type character.
   */
  function getIconStyle(type) {
    let info = iconInfoCache.get(type);
    if (info == null) {
      const icon = getIconTemplate(type, true);
      info = {
        color: icon.getAttribute('fill'),
        description: icon.querySelector('title').textContent,
      };
      iconInfoCache.set(type, info);
    }
    return info;
  }

  return {getIconTemplate, getIconStyle};
}

/** Utilities for working with the state */
const state = _initState();
const {getIconTemplate, getIconStyle} = _makeIconTemplateGetter();
_startListeners();
