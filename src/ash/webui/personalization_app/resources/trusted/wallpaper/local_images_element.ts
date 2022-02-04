// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview WallpaperImages displays a list of wallpaper images from a
 * wallpaper collection. It requires a parameter collection-id to fetch
 * and display the images. It also caches the list of wallpaper images by
 * wallpaper collection id to avoid refetching data unnecessarily.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './styles.js';
import '../../common/icons.js';
import '../../common/styles.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getLoadingPlaceholderAnimationDelay} from '../../common/utils.js';
import {isSelectionEvent} from '../../common/utils.js';
import {CurrentWallpaper, WallpaperImage, WallpaperProviderInterface} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isFilePath} from '../utils.js';

import {fetchLocalData, selectWallpaper} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

export class LocalImages extends WithPersonalizationStore {
  static get is() {
    return 'local-images';
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

      images_: {
        type: Array,
        observer: 'onImagesChanged_',
      },

      /** Mapping of local image path to data url. */
      imageData_: Object,

      /** Mapping of local image path to boolean. */
      imageDataLoading_: Object,

      currentSelected_: Object,

      /** The pending selected image. */
      pendingSelected_: Object,

      imagesToDisplay_: {
        type: Array,
        value: [],
      },
    };
  }

  static get observers() {
    return ['onImageLoaded_(imageData_, imageDataLoading_)'];
  }

  public hidden: boolean;

  private wallpaperProvider_: WallpaperProviderInterface;
  private images_: FilePath[]|null;
  private imageData_: Record<FilePath['path'], string>;
  private imageDataLoading_: Record<FilePath['path'], boolean>;
  private currentSelected_: CurrentWallpaper|null;
  private pendingSelected_: FilePath|WallpaperImage|null;
  private imagesToDisplay_: FilePath[];

  constructor() {
    super();
    this.wallpaperProvider_ = getWallpaperProvider();
  }

  connectedCallback() {
    super.connectedCallback();
    this.watch<LocalImages['images_']>(
        'images_', state => state.wallpaper.local.images);
    this.watch<LocalImages['imageData_']>(
        'imageData_', state => state.wallpaper.local.data);
    this.watch<LocalImages['imageDataLoading_']>(
        'imageDataLoading_', state => state.wallpaper.loading.local.data);
    this.watch<LocalImages['currentSelected_']>(
        'currentSelected_', state => state.wallpaper.currentSelected);
    this.watch<LocalImages['pendingSelected_']>(
        'pendingSelected_', state => state.wallpaper.pendingSelected);
    this.updateFromStore();
    fetchLocalData(this.wallpaperProvider_, this.getStore());
    window.addEventListener('focus', () => {
      fetchLocalData(this.wallpaperProvider_, this.getStore());
    });
  }

  /**
   * When iron-list items change while parent element is hidden, iron-list will
   * render incorrectly. Force another layout to happen by calling iron-resize
   * when this element is visible again.
   */
  private onHiddenChanged_(hidden: boolean) {
    if (!hidden) {
      document.title = this.i18n('myImagesLabel');
      this.shadowRoot?.getElementById('main')?.focus();
      afterNextRender(this, () => {
        this.shadowRoot?.querySelector('iron-list')?.fire('iron-resize');
      });
    }
  }

  /** Sets |imagesToDisplay| when a new set of local images loads. */
  private onImagesChanged_(images: LocalImages['images_']) {
    this.imagesToDisplay_ = (images || []).filter(image => {
      const key = image.path;
      if (this.imageDataLoading_[key] === false) {
        return !!this.imageData_[key];
      }
      return true;
    });
  }

  /**
   * Called each time a new image thumbnail is loaded. Removes images
   * from the list of displayed images if it has failed to load.
   */
  private onImageLoaded_(
      imageData: LocalImages['imageData_'],
      imageDataLoading: LocalImages['imageDataLoading_']) {
    if (!imageData || !imageDataLoading) {
      return;
    }
    // Iterate backwards in case we need to splice to remove from
    // |imagesToDisplay| while iterating.
    for (let i = this.imagesToDisplay_.length - 1; i >= 0; i--) {
      const image = this.imagesToDisplay_[i];
      const key = image.path;
      const failed = imageDataLoading[key] === false && !imageData[key];
      if (failed) {
        this.splice('imagesToDisplay_', i, 1);
      }
    }
  }

  private getAriaSelected_(
      image: FilePath|null, currentSelected: LocalImages['currentSelected_'],
      pendingSelected: LocalImages['pendingSelected_']): 'true'|'false' {
    if (!image || (!currentSelected && !pendingSelected)) {
      return 'false';
    }
    return (isFilePath(pendingSelected) &&
                image.path === pendingSelected.path ||
            !!currentSelected && image.path.endsWith(currentSelected.key) &&
                !pendingSelected)
               .toString() as 'true' |
        'false';
  }

  private getAriaLabel_(image: FilePath|null): string {
    if (!isFilePath(image)) {
      return '';
    }
    const path = image.path;
    return path.substring(path.lastIndexOf('/') + 1);
  }

  private isImageLoading_(
      image: FilePath|null,
      imageDataLoading: LocalImages['imageDataLoading_']): boolean {
    if (!isFilePath(image) || !imageDataLoading) {
      return true;
    }
    const key = image.path;
    // If key is not present, then loading has not yet started. Still show a
    // loading tile in this case.
    return !imageDataLoading.hasOwnProperty(key) ||
        imageDataLoading[key] === true;
  }

  private getLoadingPlaceholderAnimationDelay_(index: number): string {
    return getLoadingPlaceholderAnimationDelay(index);
  }

  private isImageReady_(
      image: FilePath|null, imageData: LocalImages['imageData_'],
      imageDataLoading: LocalImages['imageDataLoading_']): boolean {
    if (!isFilePath(image) || !imageData || !imageDataLoading) {
      return false;
    }
    const key = image.path;
    return !!imageData[key] && imageDataLoading[key] === false;
  }

  private getImageData_(image: FilePath, imageData: LocalImages['imageData_']):
      string {
    return imageData[image.path];
  }

  private getImageKey_(image: FilePath): FilePath['path'] {
    return image.path;
  }

  private onImageSelected_(event: Event) {
    if (!isSelectionEvent(event)) {
      return;
    }
    const path = (event.currentTarget as HTMLElement).dataset['id'];
    const image = this.images_!.find(image => path === image.path);
    if (!image) {
      assertNotReached('Image with that path not found');
      return;
    }
    selectWallpaper(image, this.wallpaperProvider_, this.getStore());
  }

  private getAriaIndex_(i: number): number {
    return i + 1;
  }
}

customElements.define(LocalImages.is, LocalImages);
