// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This router component hooks into the current url path and query
 * parameters to display sections of the personalization SWA.
 */

import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-query-params.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GooglePhotosAlbum, WallpaperCollection} from './personalization_app.mojom-webui.js';

export enum Paths {
  Ambient = '/ambient',
  CollectionImages = '/wallpaper/collection',
  Collections = '/wallpaper',
  GooglePhotosCollection = '/wallpaper/google-photos',
  LocalCollection = '/wallpaper/local',
  Root = '/',
  User = '/user',
}

export function isPersonalizationHubEnabled(): boolean {
  return loadTimeData.getBoolean('isPersonalizationHubEnabled');
}

export class PersonalizationRouter extends PolymerElement {
  static get is() {
    return 'personalization-router';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      path_: {
        type: String,
      },

      query_: {
        type: String,
      },

      queryParams_: {
        type: Object,
      },
    };
  }
  private path_: string;
  private query_: string;
  private queryParams_: {id?: string; googlePhotosAlbumId?: string;};

  static instance(): PersonalizationRouter {
    return document.querySelector(PersonalizationRouter.is) as
        PersonalizationRouter;
  }

  /**
   * Reload the application at the collections page.
   */
  static reloadAtWallpaper() {
    window.location.replace(Paths.Collections);
  }

  connectedCallback() {
    super.connectedCallback();
    // Force the user onto the wallpaper subpage if personalization hub feature
    // is not enabled, and the user is not already on a wallpaper page.
    if (!loadTimeData.getBoolean('isPersonalizationHubEnabled') &&
        !this.shouldShowWallpaperSubpage_(this.path_)) {
      PersonalizationRouter.reloadAtWallpaper();
    }
  }

  get collectionId() {
    if (this.path_ !== Paths.CollectionImages) {
      return null;
    }
    return this.queryParams_.id;
  }

  /**
   * Navigate to the selected collection id. Assumes validation of the
   * collection has already happened.
   */
  selectCollection(collection: WallpaperCollection) {
    document.title = collection.name;
    this.goToRoute(Paths.CollectionImages, {id: collection.id});
  }

  /** Navigate to a specific album in the Google Photos collection page. */
  selectGooglePhotosAlbum(album: GooglePhotosAlbum) {
    this.goToRoute(
        Paths.GooglePhotosCollection, {googlePhotosAlbumId: album.id});
  }

  goToRoute(path: Paths, queryParams: Object = {}) {
    this.setProperties({path_: path, queryParams_: queryParams});
  }

  private shouldShowCollections_(path: string): boolean {
    return path === Paths.Collections;
  }

  private shouldShowCollectionImages_(path: string): boolean {
    return path === Paths.CollectionImages;
  }

  private shouldShowGooglePhotosCollection_(path: string): boolean {
    return path === Paths.GooglePhotosCollection;
  }

  private shouldShowLocalCollection_(path: string): boolean {
    return path === Paths.LocalCollection;
  }

  /**
   * Whether Google Photos integration is enabled.
   */
  private isGooglePhotosIntegrationEnabled_(): boolean {
    return loadTimeData.getBoolean('isGooglePhotosIntegrationEnabled');
  }

  private shouldShowRootPage_(path: string|null): boolean {
    return isPersonalizationHubEnabled() && path === Paths.Root;
  }

  private shouldShowAmbientSubpage_(path: string|null): boolean {
    return !!path?.startsWith(Paths.Ambient);
  }

  private shouldShowUserSubpage_(path: string|null): boolean {
    return !!path?.startsWith(Paths.User);
  }

  private shouldShowWallpaperSubpage_(path: string|null): boolean {
    return !!path?.startsWith(Paths.Collections);
  }

  private shouldShowBreadcrumb_(path: string|null): boolean {
    return path !== Paths.Root;
  }
}

customElements.define(PersonalizationRouter.is, PersonalizationRouter);
