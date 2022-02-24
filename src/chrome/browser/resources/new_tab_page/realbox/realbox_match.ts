// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './realbox_icon.js';
import './realbox_action.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {ACMatchClassification, AutocompleteMatch, PageHandlerInterface} from '../realbox.mojom-webui.js';
import {decodeString16, mojoTimeTicks} from '../utils.js';

import {RealboxBrowserProxy} from './realbox_browser_proxy.js';
import {RealboxIconElement} from './realbox_icon.js';
import {getTemplate} from './realbox_match.html.js';
// clang-format off

/**
 * Bitmap used to decode the value of ACMatchClassification style
 * field.
 * See components/omnibox/browser/autocomplete_match.h.
 */
enum ACMatchClassificationStyle {
  NONE = 0,
  URL =   1 << 0,  // A URL.
  MATCH = 1 << 1,  // A match for the user's search term.
  DIM =   1 << 2,  // A "helper text".
}
// clang-format on

export interface RealboxMatchElement {
  $: {
    icon: RealboxIconElement,
    contents: HTMLElement,
    description: HTMLElement,
    remove: HTMLElement,
    separator: HTMLElement,
    'focus-indicator': HTMLElement,
  };
}

// Displays an autocomplete match similar to those in the Omnibox.
export class RealboxMatchElement extends PolymerElement {
  static get is() {
    return 'ntp-realbox-match';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /** Element's 'aria-label' attribute. */
      ariaLabel: {
        type: String,
        computed: `computeAriaLabel_(match.a11yLabel)`,
        reflectToAttribute: true,
      },

      /**
       * Whether the match features an image (as opposed to an icon or favicon).
       */
      hasImage: {
        type: Boolean,
        computed: `computeHasImage_(match)`,
        reflectToAttribute: true,
      },

      match: {
        type: Object,
      },

      /**
       * Index of the match in the autocomplete result. Used to inform embedder
       * of events such as deletion, click, etc.
       */
      matchIndex: {
        type: Number,
        value: -1,
      },

      //========================================================================
      // Private properties
      //========================================================================

      actionIsVisible_: {
        type: Boolean,
        computed: `computeActionIsVisible_(match)`,
      },

      /** Rendered match contents based on autocomplete provided styling. */
      contentsHtml_: {
        type: String,
        computed: `computeContentsHtml_(match)`,
      },

      /** Rendered match description based on autocomplete provided styling. */
      descriptionHtml_: {
        type: String,
        computed: `computeDescriptionHtml_(match)`,
      },

      /** Remove button's 'aria-label' attribute. */
      removeButtonAriaLabel_: {
        type: String,
        computed: `computeRemoveButtonAriaLabel_(match.removeButtonA11yLabel)`,
      },

      removeButtonTitle_: {
        type: String,
        value: () => loadTimeData.getString('removeSuggestion'),
      },

      /** Used to separate the contents from the description. */
      separatorText_: {
        type: String,
        computed: `computeSeparatorText_(match)`,
      },

      /** Rendered tail suggest common prefix. */
      tailSuggestPrefix_: {
        type: String,
        computed: `computeTailSuggestPrefix_(match)`,
      },
    };
  }

  ariaLabel: string;
  hasImage: boolean;
  match: AutocompleteMatch;
  matchIndex: number;
  private actionIsVisible_: boolean;
  private contentsHtml_: string;
  private descriptionHtml_: string;
  private removeButtonAriaLabel_: string;
  private removeButtonTitle_: string;
  private separatorText_: string;
  private tailSuggestPrefix_: string;

  private pageHandler_: PageHandlerInterface;

  constructor() {
    super();
    this.pageHandler_ = RealboxBrowserProxy.getInstance().handler;
  }

  ready() {
    super.ready();

    this.addEventListener('click', (event) => this.onMatchClick_(event));
    this.addEventListener('focusin', () => this.onMatchFocusin_());
  }

  //============================================================================
  // Event handlers
  //============================================================================

  /**
   * containing index of the match that was removed as well as modifier key
   * presses.
   */
  private executeAction_(e: MouseEvent|KeyboardEvent) {
    this.pageHandler_.executeAction(
        this.matchIndex, mojoTimeTicks(Date.now()),
        (e as MouseEvent).button || 0, e.altKey, e.ctrlKey, e.metaKey,
        e.shiftKey);
  }

  private onActionClick_(e: MouseEvent|KeyboardEvent) {
    this.executeAction_(e);

    e.preventDefault();   // Prevents default browser action (navigation).
    e.stopPropagation();  // Prevents <iron-selector> from selecting the match.
  }

  private onActionKeyDown_(e: KeyboardEvent) {
    if (e.key && (e.key === 'Enter' || e.key === ' ')) {
      this.onActionClick_(e);
    }
  }

  private onMatchClick_(e: MouseEvent) {
    if (e.button > 1) {
      // Only handle main (generally left) and middle button presses.
      return;
    }

    this.dispatchEvent(new CustomEvent('match-click', {
      bubbles: true,
      composed: true,
      detail: {index: this.matchIndex, event: e},
    }));

    e.preventDefault();   // Prevents default browser action (navigation).
    e.stopPropagation();  // Prevents <iron-selector> from selecting the match.
  }

  private onMatchFocusin_() {
    this.dispatchEvent(new CustomEvent('match-focusin', {
      bubbles: true,
      composed: true,
      detail: this.matchIndex,
    }));
  }

  private onRemoveButtonClick_(e: MouseEvent) {
    if (e.button !== 0) {
      // Only handle main (generally left) button presses.
      return;
    }
    this.dispatchEvent(new CustomEvent('match-remove', {
      bubbles: true,
      composed: true,
      detail: this.matchIndex,
    }));

    e.preventDefault();   // Prevents default browser action (navigation).
    e.stopPropagation();  // Prevents <iron-selector> from selecting the match.
  }

  private onRemoveButtonMouseDown_(e: Event) {
    e.preventDefault();  // Prevents default browser action (focus).
  }

  //============================================================================
  // Helpers
  //============================================================================


  private computeAriaLabel_(): string {
    if (!this.match) {
      return '';
    }
    return decodeString16(this.match.a11yLabel);
  }

  private computeContentsHtml_(): string {
    if (!this.match) {
      return '';
    }
    const match = this.match;
    // `match.answer.firstLine` is generated by appending an optional additional
    // text from the answer's first line to `match.contents`, making the latter
    // a prefix of the former. Thus `match.answer.firstLine` can be rendered
    // using the markup in `match.contentsClass` which contains positions in
    // `match.contents` and the markup to be applied to those positions.
    // See //chrome/browser/ui/webui/realbox/realbox_handler.cc
    const matchContents =
        match.answer ? match.answer.firstLine : match.contents;
    return match.swapContentsAndDescription ?
        this.renderTextWithClassifications_(
                decodeString16(match.description), match.descriptionClass)
            .innerHTML :
        this.renderTextWithClassifications_(
                decodeString16(matchContents), match.contentsClass)
            .innerHTML;
  }

  private computeDescriptionHtml_(): string {
    if (!this.match) {
      return '';
    }
    const match = this.match;
    if (match.answer) {
      return decodeString16(match.answer.secondLine);
    }
    return match.swapContentsAndDescription ?
        this.renderTextWithClassifications_(
                decodeString16(match.contents), match.contentsClass)
            .innerHTML :
        this.renderTextWithClassifications_(
                decodeString16(match.description), match.descriptionClass)
            .innerHTML;
  }

  private computeTailSuggestPrefix_(): string {
    if (!this.match || !this.match.tailSuggestCommonPrefix) {
      return '';
    }
    const prefix = decodeString16(this.match.tailSuggestCommonPrefix);
    // Replace last space with non breaking space since spans collapse
    // trailing white spaces and the prefix always ends with a white space.
    if (prefix.slice(-1) === ' ') {
      return prefix.slice(0, -1) + '\u00A0';
    }
    return prefix;
  }

  private computeHasImage_(): boolean {
    return this.match && !!this.match.imageUrl;
  }

  private computeActionIsVisible_(): boolean {
    return this.match && !!this.match.action;
  }

  private computeRemoveButtonAriaLabel_(): string {
    if (!this.match) {
      return '';
    }
    return decodeString16(this.match.removeButtonA11yLabel);
  }

  private computeSeparatorText_(): string {
    return this.match && decodeString16(this.match.description) ?
        loadTimeData.getString('realboxSeparator') :
        '';
  }

  /**
   * Decodes the ACMatchClassificationStyle enteries encoded in the given
   * ACMatchClassification style field, maps each entry to a CSS
   * class and returns them.
   */
  private convertClassificationStyleToCSSClasses_(style: number): string[] {
    const classes = [];
    if (style & ACMatchClassificationStyle.DIM) {
      classes.push('dim');
    }
    if (style & ACMatchClassificationStyle.MATCH) {
      classes.push('match');
    }
    if (style & ACMatchClassificationStyle.URL) {
      classes.push('url');
    }
    return classes;
  }

  private createSpanWithClasses_(text: string, classes: string[]): Element {
    const span = document.createElement('span');
    if (classes.length) {
      span.classList.add(...classes);
    }
    span.textContent = text;
    return span;
  }

  /**
   * Renders |text| based on the given ACMatchClassification(s)
   * Each classification contains an 'offset' and an encoded list of styles for
   * styling a substring starting with the 'offset' and ending with the next.
   * @return A <span> with <span> children for each styled substring.
   */
  private renderTextWithClassifications_(
      text: string, classifications: ACMatchClassification[]): Element {
    return classifications
        .map(({offset, style}, index) => {
          const next = classifications[index + 1] || {offset: text.length};
          const subText = text.substring(offset, next.offset);
          const classes = this.convertClassificationStyleToCSSClasses_(style);
          return this.createSpanWithClasses_(subText, classes);
        })
        .reduce((container, currentElement) => {
          container.appendChild(currentElement);
          return container;
        }, document.createElement('span'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-realbox-match': RealboxMatchElement;
  }
}

customElements.define(RealboxMatchElement.is, RealboxMatchElement);
