// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The element for displaying a list of albums.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../../common/styles.js';

import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getCountText, isSelectionEvent} from '../../common/utils.js';
import {AmbientModeAlbum, TopicSource} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isRecentHighlightsAlbum} from '../utils.js';
import {getTemplate} from './album_list_element.html.js';

export interface AlbumList {
  $: {grid: IronListElement};
}

export type AlbumSelectedChangedEvent = CustomEvent<{album: AmbientModeAlbum}>;

declare global {
  interface HTMLElementEventMap {
    'album_selected_changed': AlbumSelectedChangedEvent;
  }
}

export class AlbumList extends WithPersonalizationStore {
  static get is() {
    return 'album-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      topicSource: TopicSource,
      albums: {
        type: Array,
        value: null,
      },
    };
  }

  topicSource: TopicSource;
  albums: AmbientModeAlbum[]|null;

  override ready() {
    super.ready();
    /** When element is ready, force rendering iron-list */
    afterNextRender(this, () => this.$.grid.fire('iron-resize'));
  }

  /** Invoked on selection of an album. */
  private onAlbumSelected_(e: Event&{model: {album: AmbientModeAlbum}}) {
    if (isSelectionEvent(e)) {
      e.model.album.checked = !e.model.album.checked;
      this.dispatchEvent(new CustomEvent(
          'album_selected_changed',
          {bubbles: true, composed: true, detail: {album: e.model.album}}));
    }
  }

  private isAlbumSelected_(
      changedAlbum: AmbientModeAlbum|null,
      albums: AmbientModeAlbum[]|null): boolean {
    if (!changedAlbum) {
      return false;
    }
    const album = albums!.find(album => album.id === changedAlbum.id);
    return !!album && album.checked;
  }

  /** Returns the secondary text to display for the specified |album|. */
  private getSecondaryText_(
      album: AmbientModeAlbum|null, topicSource: TopicSource): string {
    if (!album) {
      return '';
    }
    if (topicSource === TopicSource.kGooglePhotos) {
      if (isRecentHighlightsAlbum(album)) {
        return this.i18n('ambientModeAlbumsSubpageRecentHighlightsDesc');
      }
      return getCountText(album.numberOfPhotos);
    }
    if (this.topicSource === TopicSource.kArtGallery) {
      return album.description;
    }
    return '';
  }
}

customElements.define(AlbumList.is, AlbumList);
