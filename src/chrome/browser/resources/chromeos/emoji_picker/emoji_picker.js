// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.js';
import './emoji_group.js';
import './emoji_group_button.js';
import './emoji_search.js';
import './emoticon_group.js';
import './text_group_button.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {afterNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EMOJI_GROUP_SIZE_PX, EMOJI_ICON_SIZE, EMOJI_PER_ROW, EMOJI_PICKER_HEIGHT_PX, EMOJI_PICKER_SIDE_PADDING, EMOJI_PICKER_SIDE_PADDING_PX, EMOJI_PICKER_TOP_PADDING_PX, EMOJI_PICKER_TOTAL_EMOJI_WIDTH, EMOJI_PICKER_TOTAL_EMOJI_WIDTH_PX, EMOJI_PICKER_WIDTH, EMOJI_PICKER_WIDTH_PX, EMOJI_SIZE_PX, EMOJI_SPACING_PX, GROUP_ICON_SIZE, GROUP_PER_ROW, V2_EMOJI_GROUP_SPACING_PX, V2_EMOJI_ICON_SIZE, V2_EMOJI_ICON_SIZE_PX, V2_EMOJI_PICKER_HEIGHT_PX, V2_EMOJI_PICKER_SIDE_PADDING_PX, V2_EMOJI_PICKER_TOTAL_EMOJI_WIDTH, V2_EMOJI_PICKER_WIDTH_PX, V2_EMOJI_SPACING_PX, V2_TAB_BUTTON_MARGIN, V2_TAB_BUTTON_MARGIN_PX, V2_TEXT_GROUP_BUTTON_PADDING, V2_TEXT_GROUP_BUTTON_PADDING_PX} from './constants.js';
import {EmojiButton} from './emoji_button.js';
import {Feature} from './emoji_picker.mojom-webui.js';
import {EmojiPickerApiProxy, EmojiPickerApiProxyImpl} from './emoji_picker_api_proxy.js';
import {CATEGORY_BUTTON_CLICK, createCustomEvent, EMOJI_BUTTON_CLICK, EMOJI_CLEAR_RECENTS_CLICK, EMOJI_DATA_LOADED, EMOJI_REMAINING_DATA_LOADED, EMOJI_VARIANTS_SHOWN, EmojiVariantsShownEvent, GROUP_BUTTON_CLICK, V2_CONTENT_LOADED} from './events.js';
import {CATEGORY_METADATA, V2_SUBCATEGORY_TABS} from './metadata_extension.js';
import {RecentlyUsedStore} from './store.js';
import {CategoryData, CategoryEnum, Emoji, EmojiGroup, EmojiGroupData, EmojiVariants, StoredItem, SubcategoryData} from './types.js';

const EMOJI_ORDERING_JSON_TEMPLATE = '/emoji_14_0_ordering';
const EMOTICON_ORDERING_JSON_TEMPLATE = '/emoticon_ordering.json';

// the name attributes below are used to label the group buttons.
// the ordering group names are used for the group headings in the emoji picker.
const GROUP_TABS = [
  {
    name: 'Recently Used',
    icon: 'emoji_picker:schedule',
    groupId: 'history',
    active: false,
    disabled: true
  },
  {
    name: 'Smileys & Emotion',
    icon: 'emoji_picker:insert_emoticon',
    groupId: '0',
    active: false,
    disabled: false
  },
  {
    name: 'People',
    icon: 'emoji_picker:emoji_people',
    groupId: '1',
    active: false,
    disabled: false
  },
  {
    name: 'Animals & Nature',
    icon: 'emoji_picker:emoji_nature',
    groupId: '2',
    active: false,
    disabled: false
  },
  {
    name: 'Food & Drink',
    icon: 'emoji_picker:emoji_food_beverage',
    groupId: '3',
    active: false,
    disabled: false
  },
  {
    name: 'Travel & Places',
    icon: 'emoji_picker:emoji_transportation',
    groupId: '4',
    active: false,
    disabled: false
  },
  {
    name: 'Activities',
    icon: 'emoji_picker:emoji_events',
    groupId: '5',
    active: false,
    disabled: false
  },
  {
    name: 'Objects',
    icon: 'emoji_picker:emoji_objects',
    groupId: '6',
    active: false,
    disabled: false
  },
  {
    name: 'Symbols',
    icon: 'emoji_picker:emoji_symbols',
    groupId: '7',
    active: false,
    disabled: false
  },
  {
    name: 'Flags',
    icon: 'emoji_picker:flag',
    groupId: '8',
    active: false,
    disabled: false
  },
];

/**
 * Constructs the emoji group data structure from a given list of recent emoji
 * data from localstorage.
 *
 * @param {!Array<StoredItem>} recentEmoji list of recently used emoji strings.
 * @return {!Array<EmojiVariants>} list of emoji data structures
 */
function makeRecentlyUsed(recentEmoji) {
  return recentEmoji.map(
      emoji => ({
        base: {string: emoji.base, name: emoji.name, keywords: []},
        alternates: emoji.alternates
      }));
}

export class EmojiPicker extends PolymerElement {
  static get is() {
    return 'emoji-picker';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** {string} */
      emojiDataUrl: {type: String, value: EMOJI_ORDERING_JSON_TEMPLATE},
      /** {string} */
      emoticonDataUrl: {type: String, value: EMOTICON_ORDERING_JSON_TEMPLATE},
      /** @private {string} */
      category: {type: String, value: 'emoji', observer: 'onCategoryChanged'},
      /** @type {string} */
      /** @private {!Array<!SubcategoryData>} */
      emojiGroupTabs: {type: Array},
      /** @private {?EmojiGroupData} */
      emojiData: {
        type: Array,
        observer: 'onEmojiDataChanged',
      },
      /** @type {?EmojiGroupData} */
      emoticonData: {type: Array, value: []},
      /** @private {Object<string,string>} */
      preferenceMapping: {type: Object},
      /** @private {!EmojiGroup} */
      emojiHistory: {type: Object},
      /** @private {!EmojiGroup} */
      emoticonHistory: {type: Object},
      /** @private {number} */
      pagination: {type: Number, value: 1, observer: 'onPaginationChanged'},
      /** @private {string} */
      search: {type: String, value: '', observer: 'onSearchChanged'},
      /** @private {boolean} */
      textSubcategoryBarEnabled: {
        type: Boolean,
        value: false,
        computed: 'isTextSubcategoryBarEnabled(v2Enabled, category)',
        reflectToAttribute: true
      },
      /** @private {boolean} */
      v2Enabled: {type: Boolean, value: false, reflectToAttribute: true},
      /** @private {boolean} */
      searchExtensionEnabled: {type: Boolean, value: false}
    };
  }

  constructor() {
    super();

    /** @type {!RecentlyUsedStore} */
    this.recentEmojiStore = new RecentlyUsedStore('emoji-recently-used');
    /** @type {!RecentlyUsedStore} */
    this.recentEmoticonStore = new RecentlyUsedStore('emoticon-recently-used');

    this.emojiGroupTabs = GROUP_TABS;
    this.emojiData = [];

    // TODO(b/216475720): rename the data structure below for a generic naming.
    this.emojiHistory = {'group': 'Recently used', 'emoji': []};
    this.emoticonHistory = {'group': 'Recently used', 'emoji': []};

    this.preferenceMapping = {};

    /** @private {?number} */
    this.scrollTimeout = null;

    /** @private {?number} */
    this.groupScrollTimeout = null;

    /** @private {?number} */
    this.groupButtonScrollTimeout = null;

    /** @private {?EmojiButton} */
    this.activeVariant = null;

    /** @private {!EmojiPickerApiProxy} */
    this.apiProxy_ = EmojiPickerApiProxyImpl.getInstance();

    /** @private {boolean} */
    this.autoScrollingToGroup = false;

    /** @private {boolean} */
    this.highlightBarMoving = false;

    /** @private {boolean} */
    this.groupTabsMoving = false;

    this.addEventListener(
        GROUP_BUTTON_CLICK, ev => this.selectGroup(ev.detail.group));
    this.addEventListener(
        EMOJI_BUTTON_CLICK, (ev) => this.onEmojiButtonClick(ev));
    this.addEventListener(
        EMOJI_CLEAR_RECENTS_CLICK, ev => this.clearRecentEmoji());
    // variant popup related handlers
    this.addEventListener(
        EMOJI_VARIANTS_SHOWN,
        ev => this.onShowEmojiVariants(
            /** @type {!EmojiVariantsShownEvent} */ (ev)));
    this.addEventListener('click', () => this.hideDialogs());
    this.getHistory();
  }

  /**
   * @private
   * @param {number} pageNumber
   */
  filterGroupTabByPagination(pageNumber) {
    return function(tab) {
      return tab.pagination === pageNumber && !tab.groupId.includes('history');
    };
  }

  async getHistory() {
    const incognito = (await this.apiProxy_.isIncognitoTextField()).incognito;
    if (incognito) {
      this.set(['emojiHistory', 'emoji'], makeRecentlyUsed([]));
      this.set(['emoticonHistory', 'emoji'], makeRecentlyUsed([]));
    } else {
      this.set(
          ['emojiHistory', 'emoji'],
          makeRecentlyUsed(this.recentEmojiStore.data.history || []));
      this.set(
          ['emoticonHistory', 'emoji'],
          makeRecentlyUsed(this.recentEmoticonStore.data.history || []));
      this.set(
          ['preferenceMapping'], this.recentEmojiStore.getPreferenceMapping());
    }
    this.set(
        ['emojiGroupTabs', 0, 'disabled'],
        this.emojiHistory.emoji.length === 0);
    // Make highlight bar visible (now we know where it should be) and
    // add smooth sliding.
    this.updateActiveGroup(/*updateTabsScroll=*/ true);
    this.$.bar.style.display = 'block';
    this.$.bar.style.transition = 'left 200ms';
  }

  ready() {
    super.ready();

    // TODO(b/211520561): Handle loading of emoticon data.
    const initializationPromise = Promise.all([
      this.apiProxy_.getFeatureList().then(
          (response) => this.setActiveFeatures(response.featureList)),
      this.fetchOrderingData(this.emojiDataUrl + '_start.json')
          .then(data => this.onEmojiDataLoaded(data))
    ]);

    initializationPromise.then(() => {
      afterNextRender(this, () => {
        this.apiProxy_.showUI();
      });
      if (this.v2Enabled) {
        this.addEventListener(
            CATEGORY_BUTTON_CLICK,
            ev => this.onCategoryButtonClick(ev.detail.categoryName));
        this.addEventListener(EMOJI_REMAINING_DATA_LOADED, () => {
          this.fetchOrderingData(this.emoticonDataUrl).then((data) => {
            this.emoticonData = data;
            this.dispatchEvent(createCustomEvent(V2_CONTENT_LOADED));
          });
        });
      }
    });

    this.updateStyles({
      '--emoji-group-button-size': EMOJI_GROUP_SIZE_PX,
      '--emoji-picker-width': EMOJI_PICKER_WIDTH_PX,
      '--emoji-picker-height': EMOJI_PICKER_HEIGHT_PX,
      '--emoji-size': EMOJI_SIZE_PX,
      '--emoji-per-row': EMOJI_PER_ROW,
      '--emoji-picker-side-padding': EMOJI_PICKER_SIDE_PADDING_PX,
      '--emoji-picker-top-padding': EMOJI_PICKER_TOP_PADDING_PX,
      '--emoji-spacing': EMOJI_SPACING_PX,
      '--v2-emoji-picker-width': V2_EMOJI_PICKER_WIDTH_PX,
      '--v2-emoji-picker-height': V2_EMOJI_PICKER_HEIGHT_PX,
      '--v2-emoji-picker-side-padding': V2_EMOJI_PICKER_SIDE_PADDING_PX,
      '--v2-emoji-size': V2_EMOJI_ICON_SIZE_PX,
      '--v2-emoji-group-spacing': V2_EMOJI_GROUP_SPACING_PX,
      '--v2-tab-button-margin': V2_TAB_BUTTON_MARGIN_PX,
      '--v2-text-group-button-padding': V2_TEXT_GROUP_BUTTON_PADDING_PX,
      '--v2-emoji-spacing': V2_EMOJI_SPACING_PX,
    });
  }

  /**
   * @param {!Array<!Feature>} featureList
   */
  setActiveFeatures(featureList) {
    this.v2Enabled = featureList.includes(Feature.EMOJI_PICKER_EXTENSION);
    this.searchExtensionEnabled =
        featureList.includes(Feature.EMOJI_PICKER_SEARCH_EXTENSION);
  }

  /**
   * @param {string} url
   */
  fetchOrderingData(url) {
    return new Promise((resolve) => {
      const xhr = new XMLHttpRequest();
      xhr.onloadend = () => resolve(JSON.parse(xhr.responseText));
      xhr.open('GET', url);
      xhr.send();
    });
  }

  onSearchChanged(newValue) {
    this.$['list-container'].style.display = newValue ? 'none' : '';
  }

  onBarTransitionStart() {
    this.highlightBarMoving = true;
  }

  onBarTransitionEnd() {
    this.highlightBarMoving = false;
  }

  /**
   * @private
   * @param {Event} ev
   */
  onEmojiButtonClick(ev) {
    const category = ev.detail.category;
    delete ev.detail.category;
    this.insertText(category, ev.detail);
  }

  /**
   * @param {CategoryEnum} category
   * @param {{emoji: string, isVariant: boolean, baseEmoji: string, allVariants:
   *     !Array<!string>, name: string}} item
   */
  async insertText(category, item) {
    const {text, isVariant, baseEmoji, allVariants, name} = item;
    this.$.message.textContent = text + ' inserted.';
    const incognito = (await this.apiProxy_.isIncognitoTextField()).incognito;

    if (!incognito) {
      switch (category) {
        case CategoryEnum.EMOJI:
          this.recentEmojiStore.bumpItem(
              {base: text, alternates: allVariants, name: name});
          this.recentEmojiStore.savePreferredVariant(baseEmoji, text);
          this.set(
              ['emojiHistory', 'emoji'],
              makeRecentlyUsed(this.recentEmojiStore.data.history));
          break;

        case CategoryEnum.EMOTICON:
          this.recentEmoticonStore.bumpItem({base: text, name, alternates: []});
          this.set(
              ['emoticonHistory', 'emoji'],
              makeRecentlyUsed(this.recentEmoticonStore.data.history));
          break;

        default:
          throw new Error('Unknown category');
      }
    }
    const searchLength = this.$['search-container']
                             .shadowRoot.querySelector('cr-search-field')
                             .getSearchInput()
                             .value.length;

    // TODO(b/217276960): change to a more generic name
    this.apiProxy_.insertEmoji(text, isVariant, searchLength);
  }

  clearRecentEmoji() {
    this.set(['emojiHistory', 'emoji'], makeRecentlyUsed([]));
    this.set(['emojiGroupTabs', 0, 'disabled'], true);
    this.recentEmojiStore.clearRecents();
    afterNextRender(
        this, () => this.updateActiveGroup(/*updateTabsScroll=*/ true));
  }

  /**
   * @param {string} newGroup
   */
  selectGroup(newGroup) {
    // focus and scroll to selected group's first emoji.
    const group =
        this.shadowRoot.querySelector(`div[data-group="${newGroup}"]`);

    if (group) {
      group.querySelector('.group')
          .shadowRoot.querySelector('#fake-focus-target')
          .focus();
      group.scrollIntoView();
    }
  }

  onEmojiScroll() {
    // the scroll event is fired very frequently while scrolling.
    // only update active tab 100ms after last scroll event by setting
    // a timeout.
    if (this.scrollTimeout) {
      clearTimeout(this.scrollTimeout);
    }
    this.scrollTimeout = setTimeout(() => {
      this.updateActiveCategory();
      this.updateActiveGroup(/*updateTabsScroll=*/ true);
    }, 100);
  }

  onRightChevronClick() {
    if (!this.textSubcategoryBarEnabled) {
      const numTabsInFirstPage = 8;
      this.$.tabs.scrollLeft =
          ((EMOJI_PICKER_TOTAL_EMOJI_WIDTH) * numTabsInFirstPage);
      this.scrollToGroup(GROUP_TABS[GROUP_PER_ROW - 1].groupId);
      this.groupTabsMoving = true;
      this.$.bar.style.left = EMOJI_PICKER_TOTAL_EMOJI_WIDTH_PX;
    } else {
      const maxPagination = this.getPaginationArray(this.emojiGroupTabs).pop();
      this.pagination = Math.min(this.pagination + 1, maxPagination);

      const nextTab =
          this.emojiGroupTabs.find((tab) => tab.pagination === this.pagination);
      this.scrollToGroup(nextTab.groupId);
      this.groupTabsMoving = true;
    }
  }

  onLeftChevronClick() {
    if (!this.v2Enabled) {
      this.$.tabs.scrollLeft = 0;
      this.scrollToGroup(GROUP_TABS[0].groupId);
      this.groupTabsMoving = true;
      if (this.emojiHistory.emoji.length > 0) {
        this.$.bar.style.left = '0';
      } else {
        this.$.bar.style.left = EMOJI_PICKER_TOTAL_EMOJI_WIDTH_PX;
      }
    } else {
      this.pagination = Math.max(this.pagination - 1, 1);

      const nextTab =
          this.emojiGroupTabs.find((tab) => tab.pagination === this.pagination);
      this.scrollToGroup(nextTab.groupId);
      this.groupTabsMoving = true;
    }
  }

  /**
   * @param {string} newGroup The group ID to scroll to
   */
  scrollToGroup(newGroup) {
    // TODO(crbug/1152237): This should use behaviour:'smooth', but when you do
    // that it doesn't scroll.
    this.shadowRoot.querySelector(`div[data-group="${newGroup}"]`)
        .scrollIntoView();
  }

  /**
   * @private
   */
  onGroupsScroll() {
    this.updateChevrons();
    this.groupTabsMoving = true;

    if (this.groupButtonScrollTimeout) {
      clearTimeout(this.groupButtonScrollTimeout);
    }
    this.groupButtonScrollTimeout =
        setTimeout(this.groupTabScrollFinished.bind(this), 100);
  }

  /**
   * @private
   */
  groupTabScrollFinished() {
    this.groupTabsMoving = false;
    this.updateActiveGroup(/*updateTabsScroll=*/ false);
  }

  /**
   * @private
   */
  updateChevrons() {
    if (!this.v2Enabled) {
      if (this.$.tabs.scrollLeft > GROUP_ICON_SIZE) {
        this.$['left-chevron'].style.display = 'flex';
      } else {
        this.$['left-chevron'].style.display = 'none';
      }
      // 1 less because we need to allow room for the chevrons
      if (this.$.tabs.scrollLeft + GROUP_ICON_SIZE * GROUP_PER_ROW <
          GROUP_ICON_SIZE * (GROUP_TABS.length + 1)) {
        this.$['right-chevron'].style.display = 'flex';
      } else {
        this.$['right-chevron'].style.display = 'none';
      }
    } else if (this.v2Enabled && !this.textSubcategoryBarEnabled) {
      this.$['left-chevron'].style.display = 'none';
      this.$['right-chevron'].style.display = 'none';
    } else {
      this.$['left-chevron'].style.display =
          this.pagination >= 2 ? 'flex' : 'none';
      this.$['right-chevron'].style.display =
          this.pagination < this.getPaginationArray(this.emojiGroupTabs).pop() ?
          'flex' :
          'none';
    }
  }

  /**
   * @returns {string} the id of the emoji or emoticon group currently in view.
   */
  getActiveGroupIdFromScrollPosition() {
    // get bounding rect of scrollable emoji region.
    const thisRect = this.$.groups.getBoundingClientRect();

    const groupElements =
        Array.from(this.$.groups.querySelectorAll('[data-group]'));

    // activate the first group which is visible for at least 10 pixels,
    // i.e. whose bottom edge is at least 10px below the top edge of the
    // scrollable region.
    const activeGroup = groupElements.find(
        el => el.getBoundingClientRect().bottom - thisRect.top >= 10);

    const activeGroupId = activeGroup ? activeGroup.dataset.group : 'history';

    return activeGroupId;
  }

  /**
   * @param {boolean} updateTabsScroll
   */
  updateActiveGroup(updateTabsScroll) {
    // no need to update scroll state if search is showing.
    if (this.search)
      return;

    const activeGroupId = this.getActiveGroupIdFromScrollPosition();
    this.set('pagination', this.getPaginationFromGroupId(activeGroupId));
    this.updateChevrons();

    let index = 0;
    // set active to true for selected group and false for others.
    this.emojiGroupTabs.forEach((g, i) => {
      const isActive = g.groupId === activeGroupId;
      if (isActive) {
        index = i;
      }
      this.set(['emojiGroupTabs', i, 'active'], isActive);
    });

    const shouldDeactivateEmojiHistoryTab = this.category === 'emoji' &&
        index === 0 && this.emojiHistory.emoji.length === 0;
    const shouldDeactivateEmoticonHistoryTab = this.category === 'emoticon' &&
        index === 0 && this.emoticonHistory.emoji.length === 0;
    // Ensure that the history tab is not set as active if it is empty.
    if (shouldDeactivateEmojiHistoryTab || shouldDeactivateEmoticonHistoryTab) {
      this.set(['emojiGroupTabs', 0, 'active'], false);
      this.set(['emojiGroupTabs', 1, 'active'], true);
      index = 1;
    }

    // Once tab scroll is updated, update the position of the highlight bar.
    if (!this.highlightBarMoving && !this.groupTabsMoving) {
      // Update the scroll position of the emoji groups so that active group is
      // visible.
      if (!this.v2Enabled) {
        // for emoji group buttons, their highlighter always has a fixed width.
        const emojiHighlighterWidth = 24;
        this.$.bar.style.width = `${emojiHighlighterWidth}px`;

        // TODO(b/213120632): Convert the following number literals into
        // contextualized constants.
        let tabscrollLeft = this.$.tabs.scrollLeft;
        if (tabscrollLeft > EMOJI_PICKER_TOTAL_EMOJI_WIDTH * (index - 0.5)) {
          tabscrollLeft = 0;
        }
        if (tabscrollLeft +
                EMOJI_PICKER_TOTAL_EMOJI_WIDTH * (GROUP_PER_ROW - 2) <
            GROUP_ICON_SIZE * index) {
          // 5 = We want the seventh icon to be first. Then -1 for chevron, -1
          // for 1 based indexing.
          tabscrollLeft = EMOJI_PICKER_TOTAL_EMOJI_WIDTH * (7);
        }

        if (updateTabsScroll) {
          this.$.bar.style.left =
              ((index * EMOJI_PICKER_TOTAL_EMOJI_WIDTH - tabscrollLeft)) + 'px';
        } else {
          this.$.bar.style.left = ((index * EMOJI_PICKER_TOTAL_EMOJI_WIDTH -
                                    this.$.tabs.scrollLeft)) +
              'px';
        }

        // only update tab scroll when using emoji-based subcategory, because
        // the scroll position of the text-based subcategory bar is controlled
        // differently.
        if (updateTabsScroll && !this.textSubcategoryBarEnabled) {
          this.$.tabs.scrollLeft = tabscrollLeft;
        }
      } else if (this.v2Enabled && !this.textSubcategoryBarEnabled) {
        const emojiHighlighterWidth = 24;
        this.$.bar.style.width = `${emojiHighlighterWidth}px`;
        this.$.bar.style.left =
            `${index * V2_EMOJI_PICKER_TOTAL_EMOJI_WIDTH}px`;
      } else {
        const subcategoryTabs =
            Array.from(this.$.tabs.getElementsByClassName('tab'));

        // for text group button, the highlight bar only spans its inner width,
        // which excludes both padding and margin.
        if (index < subcategoryTabs.length) {
          const barInlineGap =
              V2_TAB_BUTTON_MARGIN + V2_TEXT_GROUP_BUTTON_PADDING;
          const currentTab = subcategoryTabs[index];
          this.$.bar.style.left = `${
              currentTab.offsetLeft - EMOJI_PICKER_SIDE_PADDING -
              this.calculateTabScrollLeftPosition(this.pagination)}px`;
          this.$.bar.style.width =
              `${subcategoryTabs[index].clientWidth - barInlineGap * 2}px`;
        } else {
          this.$.bar.style.left = `0px`;
          this.$.bar.style.width = `0px`;
        }
        // TODO(b/213230435): fix the bar width and left position when the
        // history tab is active
      }
    }
  }

  /**
   * Update active category by using vertical scroll position.
   */
  updateActiveCategory() {
    if (this.v2Enabled) {
      const activeGroupId = this.getActiveGroupIdFromScrollPosition();

      const currentCategory =
          V2_SUBCATEGORY_TABS.find((tab) => tab.groupId === activeGroupId)
              .category;
      this.set('category', currentCategory);
    }
  }

  hideDialogs() {
    this.hideEmojiVariants();
    if (this.emojiHistory.emoji.length > 0) {
      this.shadowRoot.querySelector(`div[data-group="history"]`)
          .querySelector('emoji-group')
          .showClearRecents = false;
    }
  }

  hideEmojiVariants() {
    if (this.activeVariant) {
      this.activeVariant.variantsVisible = false;
      this.activeVariant = null;
    }
  }

  /**
   * @param {!EmojiVariantsShownEvent} ev
   */
  onShowEmojiVariants(ev) {
    this.hideEmojiVariants();
    this.activeVariant = /** @type {EmojiButton} */ (ev.detail.button);
    if (this.activeVariant) {
      this.$.message.textContent = this.activeVariant + ' variants shown.';
      this.positionEmojiVariants(ev.detail.variants);
    }
  }

  positionEmojiVariants(variants) {
    // TODO(crbug.com/1174311): currently positions horizontally within page.
    // ideal UI would be overflowing the bounds of the page.
    // also need to account for vertical positioning.

    // compute width required for the variant popup as: SIZE * columns + 10.
    // SIZE is emoji width in pixels. number of columns is determined by width
    // of variantRows, then one column each for the base emoji and skin tone
    // indicators if present. 10 pixels are added for padding and the shadow.

    // Reset any existing left margin before calculating a new position.
    variants.style.marginLeft = 0;

    // get size of emoji picker
    const pickerRect = this.getBoundingClientRect();

    // determine how much overflows the right edge of the window.
    const rect = variants.getBoundingClientRect();
    const overflowWidth = rect.x + rect.width - pickerRect.width;
    // shift left by overflowWidth rounded up to next multiple of EMOJI_SIZE.
    const shift = EMOJI_ICON_SIZE * Math.ceil(overflowWidth / EMOJI_ICON_SIZE);
    // negative value means we are already within bounds, so no shift needed.
    variants.style.marginLeft = `-${Math.max(shift, 0)}px`;
    // Now, examine vertical scrolling and scroll if needed. Not quire sure why
    // we need listcontainer.offsetTop, but it makes things work.
    const groups = this.$.groups;
    const scrollTop = groups.scrollTop;
    const variantTop = variants.offsetTop;
    const variantBottom = variantTop + variants.offsetHeight;
    const listTop = this.$['list-container'].offsetTop;
    if (variantBottom > scrollTop + groups.offsetHeight + listTop) {
      groups.scrollTo({
        top: variantBottom - groups.offsetHeight - listTop,
        left: 0,
        behavior: 'smooth',
      });
    }
  }

  /**
   * @param {!EmojiGroupData} data
   */
  onEmojiDataLoaded(data) {
    // There is quite a lot of emoji data to load which causes slow rendering.
    // Just load the first emoji category immediately, and defer loading of the
    // other categories (which will be off screen).
    this.emojiData = [data[0]];
    afterNextRender(
        this,
        () => this.fetchOrderingData(`${this.emojiDataUrl}_remaining.json`)
                  .then(data => this.onEmojiDataLoadedRemaining(data)));
  }

  onEmojiDataLoadedRemaining(data) {
    this.push('emojiData', ...data);
    this.dispatchEvent(createCustomEvent(EMOJI_REMAINING_DATA_LOADED));
  }

  /**
   * Fires DATA_LOADED_EVENT when emoji data is loaded and the emoji picker
   * is ready to use.
   */
  onEmojiDataChanged(newValue, oldValue) {
    // This is separate from onEmojiDataLoaded because we need to ensure
    // Polymer has created the components for the emoji after setting
    // this.emojiData. This is an observer, so will run after the component
    // tree has been updated.

    // see:
    // https://polymer-library.polymer-project.org/3.0/docs/devguide/data-system#property-effects

    if (newValue && newValue.length) {
      this.dispatchEvent(createCustomEvent(EMOJI_DATA_LOADED));
    }
  }

  /**
   * Triggers when category property changes
   * @param {string} newCategoryName
   */
  onCategoryChanged(newCategoryName) {
    const categoryTabs =
        V2_SUBCATEGORY_TABS.filter(tab => tab.category === newCategoryName);
    this.set('emojiGroupTabs', categoryTabs);
    afterNextRender(this, () => {
      this.updateActiveGroup(true);
      this.$.tabs.scrollLeft =
          this.calculateTabScrollLeftPosition(this.pagination);
    });
  }

  /**
   * @param {string} newCategoryName
   */
  onCategoryButtonClick(newCategoryName) {
    this.set('category', newCategoryName);
    this.set('pagination', 1);
    this.scrollToGroup(this.emojiGroupTabs[0].groupId);
  }

  /**
   * Trigger when pagination changes
   * @param {number} newPage
   */
  onPaginationChanged(newPage) {
    if (this.v2Enabled) {
      // Left chevron has the same margin as the text subcategory button.
      this.$.tabs.scrollLeft = this.calculateTabScrollLeftPosition(newPage);
    }
  }

  /**
   * @private
   * @param {SubcategoryData} tab
   * @return {boolean}
   */
  isNonHistoryTab(tab) {
    return tab.groupId !== 'history';
  }

  /**
   * Returns true if the subcategory bar requires text group buttons.
   * @private
   * @param {boolean} v2Enabled
   * @param {string} category
   */
  isTextSubcategoryBarEnabled(v2Enabled, category) {
    // Categories that require its subcategory bar to be labelled by text.
    const textCategories = ['symbol', 'emoticon'];
    return v2Enabled && textCategories.includes(category);
  }

  /**
   * Returns the array of page numbers which starts at 1 and finishes at the
   * last pagination.
   * @private
   * @param {Array<SubcategoryData>} tabs
   */
  getPaginationArray(tabs) {
    const paginations = tabs.map(tab => tab.pagination).filter(num => num);
    const lastPagination = Math.max(...paginations);
    return Array.from(Array(lastPagination), (_, idx) => idx + 1);
  }

  /**
   * Returns true if the page is not the first.
   * @private
   * @param {number} pageNumber
   */
  isNotFirstPage(pageNumber) {
    return pageNumber !== 1;
  }

  /**
   * Calculate the data group index for emoji-group and emoticon-group that
   * matches with the group id from subcategory metadata.
   * @param {string} category
   * @param {number} offsetIndex
   * @returns
   */
  getDataGroupIndex(category, offsetIndex) {
    const firstTabByCategory = V2_SUBCATEGORY_TABS.find(
        tab => tab.category === category && !tab.groupId.includes('history'));
    return parseInt(firstTabByCategory.groupId, 10) + offsetIndex;
  }

  /**
   * @param {string} groupId
   * @returns {number}
   * @throws Thrown when no tab with id that matches the given groupId is found.
   */
  getPaginationFromGroupId(groupId) {
    const tab = V2_SUBCATEGORY_TABS.find((tab) => tab.groupId === groupId);
    if (tab) {
      return tab.pagination;
    } else {
      throw new Error('Tab not found.');
    }
  }

  /**
   * @param {number} page
   * @returns {number}
   */
  calculateTabScrollLeftPosition(page) {
    const chevronMargin = V2_TAB_BUTTON_MARGIN;
    const offsetByLeftChevron = V2_EMOJI_ICON_SIZE + chevronMargin;
    return (page === 1) ? 0 :
                          (page - 1) * EMOJI_PICKER_WIDTH - offsetByLeftChevron;
  }

  /**
   * @private
   * @param {string} category
   * @returns {!Array<!CategoryData>}
   */
  getCategoryMetadata(category) {
    return CATEGORY_METADATA.map(data => ({
                                   name: data.name,
                                   icon: data.icon,
                                   active: data.name === category
                                 }));
  }
}

customElements.define(EmojiPicker.is, EmojiPicker);
