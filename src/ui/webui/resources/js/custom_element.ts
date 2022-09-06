// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Base class for Web Components that don't use Polymer.
 * See the following file for usage:
 * chrome/test/data/webui/js/custom_element_test.js
 */

function emptyHTML(): string|TrustedHTML {
  return window.trustedTypes ? window.trustedTypes.emptyHTML : '';
}

export class CustomElement extends HTMLElement {
  static get template() {
    return emptyHTML();
  }

  constructor() {
    super();

    this.attachShadow({mode: 'open'});
    const template = document.createElement('template');
    const html =
        (this.constructor as typeof CustomElement).template || emptyHTML();
    // This is a workaround for the fact that the innerHTML setter only accepts
    // a string and not TrustedHTML.
    template.innerHTML = html as unknown as string;
    this.shadowRoot!.appendChild(template.content.cloneNode(true));
  }

  $<E extends Element = Element>(query: string): E|null {
    return this.shadowRoot!.querySelector<E>(query);
  }

  $all<E extends Element = Element>(query: string): NodeListOf<E> {
    return this.shadowRoot!.querySelectorAll<E>(query);
  }
}
