// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that previews the current selected
 * screensaver.
 */

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import '../../common/styles.js';
import '../cros_button_style.js';

import {isNonEmptyArray} from '../../common/utils.js';
import {AmbientModeAlbum, TopicSource} from '../personalization_app.mojom-webui.js';
import {Paths, PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getPhotoCount, getTopicSourceName, replaceResolutionSuffix} from '../utils.js';

import {setAmbientModeEnabled} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {AmbientObserver} from './ambient_observer.js';
import {getTemplate} from './ambient_preview_element.html.js';

export class AmbientPreview extends WithPersonalizationStore {
  static get is() {
    return 'ambient-preview';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      clickable: {
        type: Boolean,
        value: false,
      },
      ambientModeEnabled_: Boolean,
      albums_: {
        type: Array,
        value: null,
      },
      topicSource_: {
        type: Object,
        value: null,
      },
      previewAlbums_: {
        type: Array,
        computed: 'computePreviewAlbums_(albums_, topicSource_)',
      },
      firstPreviewAlbum_: {
        type: AmbientModeAlbum,
        computed: 'computeFirstPreviewAlbum_(previewAlbums_)',
      },
      loading_: {
        type: Boolean,
        computed: 'computeLoading_(ambientModeEnabled_, albums_, topicSource_)',
      }
    };
  }

  clickable: boolean;

  private ambientModeEnabled_: boolean|null;
  private albums_: AmbientModeAlbum[]|null;
  private topicSource_: TopicSource|null;
  private previewAlbums_: AmbientModeAlbum[]|null;
  private firstPreviewAlbum_: AmbientModeAlbum|null;
  private loading_: boolean;

  override ready() {
    super.ready();
    AmbientObserver.initAmbientObserverIfNeeded();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch(
        'ambientModeEnabled_', state => state.ambient.ambientModeEnabled);
    this.watch('albums_', state => state.ambient.albums);
    this.watch('topicSource_', state => state.ambient.topicSource);
    this.updateFromStore();
  }

  private computeLoading_(): boolean {
    return this.ambientModeEnabled_ === null || this.albums_ === null ||
        this.topicSource_ === null;
  }

  /** Enable ambient mode and navigates to the ambient subpage. */
  private async onClickAmbientModeButton_(event: Event) {
    event.stopPropagation();
    await setAmbientModeEnabled(
        /*ambientModeEnabled=*/ true, getAmbientProvider(), this.getStore());
    PersonalizationRouter.instance().goToRoute(Paths.Ambient);
  }

  private computePreviewAlbums_(): AmbientModeAlbum[]|null {
    return (this.albums_ || [])
        .filter(
            album => album.topicSource === this.topicSource_ && album.checked &&
                album.url);
  }

  private computeFirstPreviewAlbum_(): AmbientModeAlbum|null {
    if (isNonEmptyArray(this.previewAlbums_)) {
      return this.previewAlbums_[0];
    }
    return null;
  }

  private getPreviewContainerClass_(): string {
    return this.ambientModeEnabled_ ? 'ambient-enabled' : '';
  }

  private getContainerClass_(): string {
    return `collage-${this.getCollageItems_().length}`;
  }

  private getCollageItems_(): AmbientModeAlbum[] {
    if (!isNonEmptyArray(this.previewAlbums_)) {
      return [];
    }
    return this.previewAlbums_.length < 5 ? this.previewAlbums_ :
                                            this.previewAlbums_.slice(0, 4);
  }

  private getPreviewImage_(album: AmbientModeAlbum|null): string {
    // Replace the resolution suffix appended at the end of the images
    // with a new resolution suffix of 512px. This won't impact images
    // with no resolution suffix.
    return album && album.url ?
        replaceResolutionSuffix(album.url.url, '=s512') :
        '';
  }

  private getAlbumTitle_(): string {
    return this.firstPreviewAlbum_ ? this.firstPreviewAlbum_.title : '';
  }

  private getAlbumDescription_(): string {
    if (!isNonEmptyArray(this.previewAlbums_) || this.topicSource_ === null) {
      return '';
    }
    switch (this.previewAlbums_.length) {
      case 1:
        // For only 1 selected album, album description includes image source
        // and number of photos in the album (only applicable for Google
        // Photos).
        const topicSourceDesc = getTopicSourceName(this.topicSource_);
        // TODO(b/223834394): replace dot separator symbol • with an icon/image.
        return this.topicSource_ === TopicSource.kArtGallery ?
            topicSourceDesc :
            `${topicSourceDesc} • ${
                getPhotoCount(this.previewAlbums_[0].numberOfPhotos)}`;
      case 2:
      case 3:
        // For 2-3 selected albums, album description includes the titles of all
        // selected albums except the first one already shown in album title
        // text.
        const albumTitles =
            this.previewAlbums_.slice(1).map(album => album.title);
        return albumTitles.join(', ');
      default:
        // For more than 3 selected albums, album description includes the title
        // of the second album and the number of remaining albums.
        // For example: Sweden 2020, +2 more albums.
        return this.i18n(
            'ambientModeMultipleAlbumsDesc', this.previewAlbums_[1].title,
            this.previewAlbums_.length - 2);
    }
  }
}

customElements.define(AmbientPreview.is, AmbientPreview);
