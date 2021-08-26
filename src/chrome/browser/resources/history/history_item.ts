// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './searched_label.js';
import './shared_style.js';
import './strings.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/icon.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserService} from './browser_service.js';
import {UMA_MAX_BUCKET_VALUE, UMA_MAX_SUBSET_BUCKET_VALUE} from './constants.js';
import {HistoryEntry} from './externs.js';

export interface HistoryItemElement {
  $: {
    'checkbox': CrCheckboxElement,
    'icon': HTMLElement,
    'link': HTMLElement,
    'menu-button': CrIconButtonElement,
    'time-accessed': HTMLElement,
  };
}

const HistoryItemElementBase =
    mixinBehaviors([FocusRowBehavior], PolymerElement) as
    {new (): PolymerElement & FocusRowBehavior};

export class HistoryItemElement extends HistoryItemElementBase {
  static get is() {
    return 'history-item';
  }

  static get properties() {
    return {
      // Underlying HistoryEntry data for this.item. Contains read-only fields
      // from the history backend, as well as fields computed by history-list.
      item: {
        type: Object,
        observer: 'itemChanged_',
      },

      selected: {
        type: Boolean,
        reflectToAttribute: true,
      },

      isCardStart: {
        type: Boolean,
        reflectToAttribute: true,
      },

      isCardEnd: {
        type: Boolean,
        reflectToAttribute: true,
      },

      lastFocused: {
        type: Object,
        notify: true,
      },

      listBlurred: {
        type: Boolean,
        notify: true,
      },

      ironListTabIndex: {
        type: Number,
        observer: 'ironListTabIndexChanged_',
      },

      selectionNotAllowed_: Boolean,

      hasTimeGap: Boolean,

      index: Number,

      numberOfItems: Number,

      // Search term used to obtain this history-item.
      searchTerm: String,

      overrideCustomEquivalent: {
        type: Boolean,
        value: true,
      },

      ariaDescribedByForHeading_: {
        type: String,
        computed: 'getAriaDescribedByForHeading_(isCardStart, isCardEnd)',
      },
    };
  }

  private isShiftKeyDown_: boolean = false;
  private selectionNotAllowed_: boolean =
      !loadTimeData.getBoolean('allowDeletingHistory');
  private eventTracker_: EventTracker = new EventTracker();

  item: HistoryEntry;
  index: number;
  searchTerm: string;
  isCardStart: boolean;
  isCardEnd: boolean;
  numberOfItems: number;

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    afterNextRender(this, () => {
      // Adding listeners asynchronously to reduce blocking time, since these
      // history items are items in a potentially long list.
      this.eventTracker_.add(
          this.$.checkbox, 'keydown',
          e => this.onCheckboxKeydown_(e as KeyboardEvent));
    });
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.remove(this.$.checkbox, 'keydown');
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  focusOnMenuButton() {
    focusWithoutInk(this.$['menu-button']);
  }

  private onCheckboxKeydown_(e: KeyboardEvent) {
    if (e.shiftKey && e.key === 'Tab') {
      this.focus();
    }
  }

  /**
   * Toggle item selection whenever the checkbox or any non-interactive part
   * of the item is clicked.
   */
  private onRowClick_(e: MouseEvent) {
    const path = e.composedPath();
    // VoiceOver has issues with click events within elements that have a role
    // of row, so this event listeners has to be on the row itself.
    // (See crbug.com/1185827.)
    let inItemContainer = false;
    for (let i = 0; i < path.length; i++) {
      const elem = path[i] as HTMLElement;
      if (elem.id !== 'checkbox' &&
          (elem.nodeName === 'A' || elem.nodeName === 'CR-ICON-BUTTON')) {
        return;
      }

      if (!inItemContainer && elem.id === 'item-container') {
        inItemContainer = true;
      }
    }

    if (this.selectionNotAllowed_ || !inItemContainer) {
      return;
    }

    this.$.checkbox.focus();
    this.fire_('history-checkbox-select', {
      index: this.index,
      shiftKey: e.shiftKey,
    });
  }

  /**
   * This is bound to mouse/keydown instead of click/press because this
   * has to fire before onCheckboxChange_. If we bind it to click/press,
   * it might trigger out of desired order.
   */
  private onCheckboxClick_(e: MouseEvent) {
    this.isShiftKeyDown_ = e.shiftKey;
  }

  private onCheckboxChange_() {
    this.fire_('history-checkbox-select', {
      index: this.index,
      // If the user clicks or press enter/space key, oncheckboxClick_ will
      // trigger before this function, so a shift-key might be recorded.
      shiftKey: this.isShiftKeyDown_,
    });

    this.isShiftKeyDown_ = false;
  }

  private onRowMousedown_(e: MouseEvent) {
    // Prevent shift clicking a checkbox from selecting text.
    if (e.shiftKey) {
      e.preventDefault();
    }
  }

  private getEntrySummary_(): string {
    const item = this.item;
    return loadTimeData.getStringF(
        'entrySummary',
        this.isCardStart || this.isCardEnd ?
            this.cardTitle_(this.numberOfItems, this.searchTerm) :
            '',
        item.dateTimeOfDay,
        item.starred ? loadTimeData.getString('bookmarked') : '', item.title,
        item.domain);
  }

  /**
   * The first and last rows of a card have a described-by field pointing to
   * the date header, to make sure users know if they have jumped between cards
   * when navigating up or down with the keyboard.
   */
  private getAriaDescribedByForHeading_(): string {
    return this.isCardStart || this.isCardEnd ? 'date-accessed' : '';
  }

  private getAriaChecked_(selected: boolean): string {
    return selected ? 'true' : 'false';
  }

  /**
   * Remove bookmark of current item when bookmark-star is clicked.
   */
  private onRemoveBookmarkTap_() {
    if (!this.item.starred) {
      return;
    }

    if (this.shadowRoot!.querySelector('#bookmark-star') ===
        this.shadowRoot!.activeElement) {
      focusWithoutInk(this.$['menu-button']);
    }

    const browserService = BrowserService.getInstance();
    browserService.removeBookmark(this.item.url);
    browserService.recordAction('BookmarkStarClicked');

    this.fire_('remove-bookmark-stars', this.item.url);
  }

  /**
   * Fires a custom event when the menu button is clicked. Sends the details
   * of the history item and where the menu should appear.
   */
  private onMenuButtonTap_(e: Event) {
    this.fire_('open-menu', {
      target: e.target,
      index: this.index,
      item: this.item,
    });

    // Stops the 'click' event from closing the menu when it opens.
    e.stopPropagation();
  }

  /**
   * Record metrics when a result is clicked.
   */
  private onLinkClick_() {
    const browserService = BrowserService.getInstance();
    browserService.recordAction('EntryLinkClick');

    if (this.searchTerm) {
      browserService.recordAction('SearchResultClick');
    }

    if (this.index === undefined) {
      return;
    }

    const ageInDays = Math.ceil(
        (new Date().getTime() - new Date(this.item.time).getTime()) /
        1000 /* s/ms */ / 60 /* m/s */ / 60 /* h/m */ / 24 /* d/h */);

    browserService.recordHistogram(
        'HistoryPage.ClickPosition', Math.min(this.index, UMA_MAX_BUCKET_VALUE),
        UMA_MAX_BUCKET_VALUE);

    browserService.recordHistogram(
        'HistoryPage.ClickAgeInDays', Math.min(ageInDays, UMA_MAX_BUCKET_VALUE),
        UMA_MAX_BUCKET_VALUE);

    if (this.index <= UMA_MAX_SUBSET_BUCKET_VALUE) {
      browserService.recordHistogram(
          'HistoryPage.ClickPositionSubset', this.index,
          UMA_MAX_SUBSET_BUCKET_VALUE);
    }

    if (ageInDays <= UMA_MAX_SUBSET_BUCKET_VALUE) {
      browserService.recordHistogram(
          'HistoryPage.ClickAgeInDaysSubset', ageInDays,
          UMA_MAX_SUBSET_BUCKET_VALUE);
    }
  }

  onLinkRightClick_() {
    BrowserService.getInstance().recordAction('EntryLinkRightClick');
  }

  /**
   * Set the favicon image, based on the URL of the history item.
   */
  private itemChanged_() {
    this.$.icon.style.backgroundImage = getFaviconForPageURL(
        this.item.url, this.item.isUrlInRemoteUserData,
        this.item.remoteIconUrlForUma);
    this.eventTracker_.add(
        this.$['time-accessed'], 'mouseover', () => this.addTimeTitle_());
  }

  /**
   * @param numberOfItems The number of items in the card.
   * @param search The search term associated with these results.
   * @return The title for this history card.
   */
  private cardTitle_(numberOfItems: number, search: string): string {
    if (this.item === undefined) {
      return '';
    }

    if (!search) {
      return this.item.dateRelativeDay;
    }
    return searchResultsTitle(numberOfItems, search);
  }

  private addTimeTitle_() {
    const el = this.$['time-accessed'];
    el.setAttribute('title', new Date(this.item.time).toString());
    this.eventTracker_.remove(el, 'mouseover');
  }

  /**
   * @param sampleElement An element to find an equivalent for.
   * @return An equivalent element to focus, or null to use the
   *     default element.
   */
  getCustomEquivalent(sampleElement: Element): Element|null {
    return sampleElement.getAttribute('focus-type') === 'star' ? this.$.link :
                                                                 null;
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(HistoryItemElement.is, HistoryItemElement);

/**
 * @return The title for a page of search results.
 */
export function searchResultsTitle(
    numberOfResults: number, searchTerm: string): string {
  const resultId = numberOfResults === 1 ? 'searchResult' : 'searchResults';
  return loadTimeData.getStringF(
      'foundSearchResults', numberOfResults, loadTimeData.getString(resultId),
      searchTerm);
}
