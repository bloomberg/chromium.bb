// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-ambient-mode-photos-page' is the settings page to
 * select personal albums in Google Photos or categories in Art gallary.
 */
import './album_list.js';
import './art_album_dialog.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import '//resources/cr_components/localized_link/localized_link.js';
import '../../settings_shared_css.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from '//resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {AmbientModeBrowserProxy, AmbientModeBrowserProxyImpl} from './ambient_mode_browser_proxy.js';
import {AmbientModeAlbum, AmbientModeSettings, AmbientModeTopicSource} from './constants.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsAmbientModePhotosPageElementBase = mixinBehaviors(
    [I18nBehavior, RouteObserverBehavior, WebUIListenerBehavior],
    PolymerElement);

/** @polymer */
class SettingsAmbientModePhotosPageElement extends
    SettingsAmbientModePhotosPageElementBase {
  static get is() {
    return 'settings-ambient-mode-photos-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      photoPreviewEnabled: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isAmbientModePhotoPreviewEnabled');
        },
        readOnly: true,
      },

      /** @type {!AmbientModeTopicSource} */
      topicSource: {
        type: Number,
        value: AmbientModeTopicSource.UNKNOWN,
      },

      /** @type {?Array<!AmbientModeAlbum>} */
      albums: {
        type: Array,
        notify: true,
        // Set to null to differentiate from an empty album.
        value: null,
      },

      /** @private */
      showArtAlbumDialog_: {
        type: Boolean,
        value: false,
      }
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!AmbientModeBrowserProxy} */
    this.browserProxy_ = AmbientModeBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    this.addWebUIListener('albums-changed', this.onAlbumsChanged_.bind(this));
    this.addWebUIListener(
        'album-preview-changed', this.onAlbumPreviewChanged_.bind(this));

    this.addEventListener(
        'selected-albums-changed', this.onSelectedAlbumsChanged_);
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} currentRoute
   * @param {!Route=} prevRoute
   * @protected
   */
  currentRouteChanged(currentRoute, prevRoute) {
    if (currentRoute !== routes.AMBIENT_MODE_PHOTOS) {
      return;
    }

    const topicSourceParam =
        Router.getInstance().getQueryParameters().get('topicSource');
    const topicSourceInt = parseInt(topicSourceParam, 10);

    if (isNaN(topicSourceInt)) {
      assertNotReached();
      return;
    }

    this.topicSource = /** @type {!AmbientModeTopicSource} */ (topicSourceInt);
    if (this.topicSource === AmbientModeTopicSource.GOOGLE_PHOTOS) {
      this.parentNode.pageTitle =
          this.i18n('ambientModeTopicSourceGooglePhotos');
    } else if (this.topicSource === AmbientModeTopicSource.ART_GALLERY) {
      this.parentNode.pageTitle = this.i18n('ambientModeTopicSourceArtGallery');
    } else {
      assertNotReached();
      return;
    }

    // TODO(b/162793904): Have a better plan to cache the UI data.
    // Reset to null to distinguish empty albums fetched from server.
    this.albums = null;
    this.browserProxy_.requestAlbums(this.topicSource);
  }

  /**
   * @param {!AmbientModeSettings} settings
   * @private
   */
  onAlbumsChanged_(settings) {
    // This page has been reused by other topic source since the last time
    // requesting the albums. Do not update on this stale event.
    if (settings.topicSource !== this.topicSource) {
      return;
    }
    this.albums = settings.albums;
  }

  /**
   * @param {!AmbientModeAlbum} album
   * @private
   */
  onAlbumPreviewChanged_(album) {
    if (album.topicSource !== this.topicSource) {
      return;
    }

    for (let i = 0; i < this.albums.length; ++i) {
      if (this.albums[i].albumId === album.albumId) {
        if (album.url) {
          this.set('albums.' + i + '.url', album.url);
          continue;
        }
        this.set(
            'albums.' + i + '.recentHighlightsUrls',
            album.recentHighlightsUrls);
      }
    }
  }

  /**
   * @param {number} topicSource
   * @return {string}
   * @private
   */
  getTitleInnerHtml_(topicSource) {
    if (topicSource === AmbientModeTopicSource.GOOGLE_PHOTOS) {
      return this.i18nAdvanced('ambientModeAlbumsSubpageGooglePhotosTitle');
    } else {
      return this.i18n('ambientModeTopicSourceArtGalleryDescription');
    }
  }

  /**
   * @param {!Event} event
   * @private
   */
  onSelectedAlbumsChanged_(
      /** @type {CustomEvent<{item: !AmbientModeAlbum}>} */ event) {
    const albums = [];
    let eventAlbumIndex = -1;
    for (let i = 0; i < this.albums.length; ++i) {
      const album = this.albums[i];
      if (album.checked) {
        albums.push({albumId: album.albumId});
      }

      if (album.albumId === event.detail.albumId) {
        eventAlbumIndex = i;
      }
    }

    assert(eventAlbumIndex >= 0, 'Wrong album index.');

    // For art gallery, cannot deselect the last album. Show a dialog to users
    // and select the album automatically.
    if (this.topicSource === AmbientModeTopicSource.ART_GALLERY &&
        albums.length === 0) {
      this.showArtAlbumDialog_ = true;
      this.set('albums.' + eventAlbumIndex + '.checked', true);
      return;
    }

    this.browserProxy_.setSelectedAlbums(
        {topicSource: this.topicSource, albums: albums});
  }

  /** @private */
  onArtAlbumDialogClose_() {
    this.showArtAlbumDialog_ = false;
  }

  /** @private */
  onCheckboxChange_() {
    const checkboxes =
        this.shadowRoot.querySelector('#albums').querySelectorAll(
            'cr-checkbox');
    const albums = [];
    checkboxes.forEach((checkbox) => {
      if (checkbox.checked && !checkbox.hidden) {
        albums.push({albumId: checkbox.dataset.id});
      }
    });
    this.browserProxy_.setSelectedAlbums(
        {topicSource: this.topicSource, albums: albums});
  }

  /**
   * @return {boolean}
   * @private
   */
  hasNoAlbums_() {
    return !!this.albums && !this.albums.length;
  }
}

customElements.define(
    SettingsAmbientModePhotosPageElement.is,
    SettingsAmbientModePhotosPageElement);
