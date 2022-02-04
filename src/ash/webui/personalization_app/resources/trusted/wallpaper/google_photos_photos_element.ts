// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays Google Photos photos.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './styles.js';
import '../../common/styles.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getNumberOfGridItemsPerRow, isNonEmptyArray, isSelectionEvent, normalizeKeyForRTL} from '../../common/utils.js';
import {WithPersonalizationStore} from '../personalization_store.js';

type PhotosRow = Array<Url|undefined>;

export interface GooglePhotosPhotos {
  $: {grid: IronListElement;};
}

export class GooglePhotosPhotos extends WithPersonalizationStore {
  static get is() {
    return 'google-photos-photos';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      hidden: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
        observer: 'onHiddenChanged_',
      },

      focusedColIndex_: {
        type: Number,
        value: 0,
      },

      photos_: Array,

      photosByRow_: {
        type: Array,
        computed: 'computePhotosByRow_(photos_, photosLoading_, photosPerRow_)',
      },

      photosLoading_: Boolean,

      photosPerRow_: {
        type: Number,
        value: function() {
          return getNumberOfGridItemsPerRow();
        },
      },
    };
  }

  /** Whether or not this element is currently hidden. */
  hidden: boolean;

  /** The index of the currently focused column. */
  private focusedColIndex_: number;

  /** The list of photos. */
  private photos_: Url[]|null|undefined;

  /**
   * The list of |photos_| split into the appropriate number of |photosPerRow_|
   * so as to be rendered in a grid.
   */
  private photosByRow_: PhotosRow[]|null;

  /** Whether the list of photos is currently loading. */
  private photosLoading_: boolean;

  /** The number of photos to render per row in a grid. */
  private photosPerRow_: number;

  connectedCallback() {
    super.connectedCallback();

    this.addEventListener('iron-resize', this.onResized_.bind(this));

    this.watch<GooglePhotosPhotos['photos_']>(
        'photos_', state => state.wallpaper.googlePhotos.photos);
    this.watch<GooglePhotosPhotos['photosLoading_']>(
        'photosLoading_', state => state.wallpaper.loading.googlePhotos.photos);

    this.updateFromStore();
  }

  /** Invoked on changes to this element's |hidden| state. */
  private onHiddenChanged_() {
    if (this.hidden) {
      return;
    }

    // When iron-list items change while their parent element is hidden, the
    // iron-list will render incorrectly. Force relayout by invalidating the
    // iron-list when this element becomes visible.
    afterNextRender(this, () => this.$.grid.fire('iron-resize'));
  }

  /** Invoked on focus of a grid row. */
  private onGridRowFocused_(e: Event) {
    // When a grid row is focused, forward the focus event on to the grid item
    // at the focused column index.
    const currentTarget = e.currentTarget as HTMLElement;
    const selector = `.photo[colindex="${this.focusedColIndex_}"]`;
    (currentTarget.querySelector(selector) as HTMLElement | null)?.focus();
  }

  /** Invoked on key down of a grid row. */
  private onGridRowKeyDown_(e: KeyboardEvent&
                            {model: {index: number, row: Url[]}}) {
    switch (normalizeKeyForRTL(e.key, this.i18n('textdirection') === 'rtl')) {
      case 'ArrowLeft':
        if (this.focusedColIndex_ > 0) {
          // Left arrow moves focus to the preceding grid item.
          this.focusedColIndex_ -= 1;
          this.$.grid.focusItem(e.model.index);
        } else if (e.model.index > 0) {
          // Left arrow moves focus to the preceding grid item, wrapping to the
          // preceding grid row.
          this.focusedColIndex_ = e.model.row.length - 1;
          this.$.grid.focusItem(e.model.index - 1);
        }
        return;
      case 'ArrowRight':
        if (this.focusedColIndex_ < e.model.row.length - 1) {
          // Right arrow moves focus to the succeeding grid item.
          this.focusedColIndex_ += 1;
          this.$.grid.focusItem(e.model.index);
        } else if (e.model.index < this.photosByRow_!.length - 1) {
          // Right arrow moves focus to the succeeding grid item, wrapping to
          // the succeeding grid row.
          this.focusedColIndex_ = 0;
          this.$.grid.focusItem(e.model.index + 1);
        }
        return;
      case 'Tab':
        // The grid contains a single |focusable| row which becomes a focus trap
        // due to the synthetic redirect of focus events to grid items. To
        // escape the trap, make the |focusable| row unfocusable until has
        // advanced to the next candidate.
        const focusable = this.$.grid.querySelector('[tabindex="0"]')!;
        focusable.setAttribute('tabindex', '-1');
        afterNextRender(this, () => focusable.setAttribute('tabindex', '0'));
        return;
    }
  }

  /** Invoked on resize of this element. */
  private onResized_() {
    this.photosPerRow_ = getNumberOfGridItemsPerRow();
  }

  /** Invoked to compute |photosByRow_|. */
  private computePhotosByRow_(): PhotosRow[]|null {
    if (this.photosLoading_ || !this.photosPerRow_) {
      return null;
    }
    if (!isNonEmptyArray(this.photos_)) {
      return null;
    }
    return Array.from(
        {length: Math.ceil(this.photos_.length / this.photosPerRow_)},
        (_, i) => {
          i *= this.photosPerRow_;
          const row: PhotosRow = this.photos_!.slice(i, i + this.photosPerRow_);
          while (row.length < this.photosPerRow_) {
            row.push(undefined);
          }
          return row;
        });
  }
}

customElements.define(GooglePhotosPhotos.is, GooglePhotosPhotos);
