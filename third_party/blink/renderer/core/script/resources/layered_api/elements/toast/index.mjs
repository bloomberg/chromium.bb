/**
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview This file defines the class for the Standard Toast LAPI
 * and the accompanying showToast() function.
 * EXPLAINER: https://github.com/jackbsteinberg/std-toast
 * TEST PATH: /chromium/src/third_party/blink/web_tests/external/wpt/std-toast/*
 * @package
 */

import * as reflection from '../internal/reflection.mjs';

const DEFAULT_DURATION = 3000;

function stylesheetFactory() {
  let stylesheet;
  return function generate() {
    if (!stylesheet) {
      stylesheet = new CSSStyleSheet();
      stylesheet.replaceSync(`
        :host {
          position: fixed;
          bottom: 1em;
          right: 1em;
          border: solid;
          padding: 1em;
          background: white;
          color: black;
          z-index: 1;
        }

        :host(:not([open])) {
          display: none;
        }
      `);
      // TODO(jacksteinberg): use offset-block-end: / offset-inline-end: over bottom: / right:
      // when implemented https://bugs.chromium.org/p/chromium/issues/detail?id=538475
    }
    return stylesheet;
  };
}

const generateStylesheet = stylesheetFactory();

export class StdToastElement extends HTMLElement {
  static observedAttributes = ['open', 'closebutton'];
  #shadow = this.attachShadow({mode: 'closed'});
  #timeoutID;
  #actionSlot;
  #closeButtonElement;

  constructor(message) {
    super();

    this.#shadow.adoptedStyleSheets = [generateStylesheet()];

    this.#shadow.appendChild(document.createElement('slot'));

    this.#actionSlot = document.createElement('slot');
    this.#actionSlot.setAttribute('name', 'action');
    this.#shadow.appendChild(this.#actionSlot);

    this.#closeButtonElement = document.createElement('button');
    this.#closeButtonElement.setAttribute('part', 'closebutton');
    this.#closeButtonElement.setAttribute('aria-label', 'close');
    this.#closeButtonElement.textContent = '×';
    this.#shadow.appendChild(this.#closeButtonElement);

    this.#closeButtonElement.addEventListener('click', () => {
      this.hide();
    });

    if (message !== undefined) {
      this.textContent = message;
    }
  }

  connectedCallback() {
    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'status');
    }
    // TODO(jacksteinberg): use https://github.com/whatwg/html/pull/4658 when implemented
  }

  get action() {
    return this.#actionSlot.assignedNodes().length !== 0 ?
      this.#actionSlot.assignedNodes()[0] :
      null;
  }

  set action(val) {
    const previousAction = this.action;
    if (val !== null) {
      if (!isElement(val)) {
        throw new TypeError('Invalid argument: must be type Element');
      }

      val.setAttribute('slot', 'action');
      this.insertBefore(val, previousAction);
    }

    if (previousAction !== null) {
      previousAction.remove();
    }
  }

  get closeButton() {
    if (this.hasAttribute('closebutton')) {
      const closeAttr = this.getAttribute('closebutton');
      return closeAttr === '' ? true : closeAttr;
    }
    return false;
  }

  set closeButton(val) {
    if (val === true) {
      this.setAttribute('closebutton', '');
    } else if (val === false) {
      this.removeAttribute('closebutton');
    } else {
      this.setAttribute('closebutton', val);
    }
  }

  show({duration = DEFAULT_DURATION} = {}) {
    this.setAttribute('open', '');
    clearTimeout(this.#timeoutID);
    this.#timeoutID = setTimeout(() => {
      this.removeAttribute('open');
    }, duration);
  }

  hide() {
    this.removeAttribute('open');
  }

  toggle(force) {
    this.toggleAttribute('open', force);
  }

  attributeChangedCallback(name, oldValue, newValue) {
    switch (name) {
      case 'open':
        if (newValue !== null && oldValue === null) {
          this.dispatchEvent(new Event('show'));
        } else if (newValue === null) {
          this.dispatchEvent(new Event('hide'));
          clearTimeout(this.#timeoutID);
          this.#timeoutID = null;
        }
        break;
      case 'closebutton':
        if (newValue !== null) {
          if (newValue === '') {
            this.#closeButtonElement.textContent = '×';
            this.#closeButtonElement.setAttribute('aria-label', 'close');
          } else {
            this.#closeButtonElement.textContent = newValue;
            this.#closeButtonElement.removeAttribute('aria-label');
          }
        }
        // if newValue === null we do nothing, since CSS will hide the button
        break;
    }
  }
}

reflection.installBool(StdToastElement.prototype, 'open');

customElements.define('std-toast', StdToastElement);

delete StdToastElement.prototype.attributeChangedCallback;
delete StdToastElement.prototype.observedAttributes;
delete StdToastElement.prototype.connectedCallback;

export function showToast(message, options = {}) {
  const toast = new StdToastElement(message);

  const {action, closeButton, ...showOptions} = options;
  if (isElement(action)) {
    toast.action = action;
  } else if (action !== undefined) {
    const actionButton = document.createElement('button');

    // Unlike String(), this performs the desired JavaScript ToString operation.
    // https://gist.github.com/domenic/82adbe7edc4a33a70f42f255479cec39
    actionButton.textContent = `${action}`;

    actionButton.setAttribute('slot', 'action');
    toast.appendChild(actionButton);
  }

  if (closeButton !== undefined) {
    toast.closeButton = closeButton;
  }

  document.body.append(toast);
  toast.show(showOptions);

  return toast;
}

const idGetter =
  Object.getOwnPropertyDescriptor(Element.prototype, 'id').get;
function isElement(value) {
  try {
    idGetter.call(value);
    return true;
  } catch {
    return false;
  }
}
