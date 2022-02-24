// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays the wallpaper section of the
 * personalization SWA.
 */

import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Paths} from '../personalization_router_element.js';

export class WallpaperSubpage extends PolymerElement {
  static get is() {
    return 'wallpaper-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {path: String, queryParams: Object};
  }

  path: string;
  queryParams: Record<string, string>;

  private shouldShowCollections_(path: string): boolean {
    return path === Paths.Collections;
  }

  private shouldShowCollectionImages_(path: string): boolean {
    return path === Paths.CollectionImages;
  }

  private shouldShowGooglePhotosCollection_(path: string): boolean {
    return this.isGooglePhotosIntegrationEnabled_() &&
        path === Paths.GooglePhotosCollection;
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
}

customElements.define(WallpaperSubpage.is, WallpaperSubpage);
