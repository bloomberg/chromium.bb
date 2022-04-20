// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * The breadcrumb that displays the current view stack and allows users to
 * navigate.
 */

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import '../common/styles.js';
import './cros_button_style.js';

import {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {isNonEmptyArray} from '../common/utils.js';

import {GooglePhotosAlbum, TopicSource, WallpaperCollection} from './personalization_app.mojom-webui.js';
import {getTemplate} from './personalization_breadcrumb_element.html.js';
import {isPathValid, isPersonalizationHubEnabled, Paths, PersonalizationRouter} from './personalization_router_element.js';
import {WithPersonalizationStore} from './personalization_store.js';
import {inBetween, isNonEmptyString} from './utils.js';

/** Event interface for dom-repeat. */
interface RepeaterEvent extends CustomEvent {
  model: {
    index: number,
  };
}

export function stringToTopicSource(x: string): TopicSource|null {
  const num = parseInt(x, 10);
  if (!isNaN(num) &&
      inBetween(num, TopicSource.MIN_VALUE, TopicSource.MAX_VALUE)) {
    return num;
  }
  return null;
}

export interface PersonalizationBreadcrumb {
  $: {
    keys: IronA11yKeysElement,
    selector: IronSelectorElement,
  };
}

export class PersonalizationBreadcrumb extends WithPersonalizationStore {
  static get is() {
    return 'personalization-breadcrumb';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current collection id to display.
       */
      collectionId: {
        type: String,
      },

      /** The current Google Photos album id to display. */
      googlePhotosAlbumId: String,

      /** The topic source of the selected album(s) for screensaver. */
      topicSource: String,

      /**
       * The current path of the page.
       */
      path: {
        type: String,
      },

      breadcrumbs_: {
        type: Array,
        computed:
            'computeBreadcrumbs_(path, collections_, collectionId, googlePhotosAlbums_, googlePhotosAlbumId, topicSource)',
      },

      collections_: {
        type: Array,
      },

      /** The list of Google Photos albums. */
      googlePhotosAlbums_: Array,

      showBackButton_: {
        type: Boolean,
        computed: 'computeShowBackButton_(path)',
      },

      /** The breadcrumb being highlighted by keyboard navigation. */
      selectedBreadcrumb_: {
        type: Object,
        notify: true,
      },
    };
  }

  collectionId: string;
  googlePhotosAlbumId: string;
  topicSource: string;
  path: string;
  private breadcrumbs_: string[];
  private collections_: WallpaperCollection[]|null;
  private googlePhotosAlbums_: GooglePhotosAlbum[]|null;
  private showBackButton_: boolean;
  private selectedBreadcrumb_: HTMLElement;

  override ready() {
    super.ready();
    this.$.keys.target = this.$.selector;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch('collections_', state => state.wallpaper.backdrop.collections);
    this.watch(
        'googlePhotosAlbums_', state => state.wallpaper.googlePhotos.albums);
    this.updateFromStore();
  }

  /** Handle keyboard navigation. */
  private onKeysPress_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>) {
    const selector = this.$.selector;
    const prevBreadcrumb = this.selectedBreadcrumb_;
    switch (e.detail.key) {
      case 'left':
        selector.selectPrevious();
        break;
      case 'right':
        selector.selectNext();
        break;
      default:
        return;
    }
    // Remove focus state of previous breadcrumb.
    if (prevBreadcrumb) {
      prevBreadcrumb.removeAttribute('tabindex');
    }
    // Add focus state for new breadcrumb.
    if (this.selectedBreadcrumb_) {
      this.selectedBreadcrumb_.setAttribute('tabindex', '0');
      this.selectedBreadcrumb_.focus();
    }
    e.detail.keyboardEvent.preventDefault();
  }

  /**
   * Returns the class of the breadcrumb. The last breadcrumb will not be
   * selectable.
   */
  private computeBreadcrumbClass_(index: number, breadcrumbs: string[]):
      string {
    if (index < breadcrumbs.length - 1) {
      return 'breadcrumb selectable';
    }
    return 'breadcrumb';
  }

  private computeBreadcrumbs_(): string[] {
    const breadcrumbs = [];
    switch (this.path) {
      case Paths.Collections:
        breadcrumbs.push(this.i18n('wallpaperLabel'));
        break;
      case Paths.CollectionImages:
        breadcrumbs.push(this.i18n('wallpaperLabel'));
        if (isNonEmptyArray(this.collections_)) {
          const collection = this.collections_.find(
              collection => collection.id === this.collectionId);
          if (collection) {
            breadcrumbs.push(collection.name);
          }
        }
        break;
      case Paths.GooglePhotosCollection:
        breadcrumbs.push(this.i18n('wallpaperLabel'));
        breadcrumbs.push(this.i18n('googlePhotosLabel'));
        if (isNonEmptyString(this.googlePhotosAlbumId) &&
            isNonEmptyArray(this.googlePhotosAlbums_)) {
          const googlePhotosAlbum = this.googlePhotosAlbums_.find(
              googlePhotosAlbum =>
                  googlePhotosAlbum.id === this.googlePhotosAlbumId);
          if (googlePhotosAlbum) {
            breadcrumbs.push(googlePhotosAlbum.title);
          }
        }
        break;
      case Paths.LocalCollection:
        breadcrumbs.push(this.i18n('wallpaperLabel'));
        breadcrumbs.push(this.i18n('myImagesLabel'));
        break;
      case Paths.User:
        breadcrumbs.push(this.i18n('avatarLabel'));
        break;
      case Paths.Ambient:
        breadcrumbs.push(this.i18n('screensaverLabel'));
        break;
      case Paths.AmbientAlbums:
        breadcrumbs.push(this.i18n('screensaverLabel'));
        const topicSourceVal = stringToTopicSource(this.topicSource);
        if (topicSourceVal === TopicSource.kGooglePhotos) {
          breadcrumbs.push(this.i18n('ambientModeTopicSourceGooglePhotos'));
        } else if (topicSourceVal === TopicSource.kArtGallery) {
          breadcrumbs.push(this.i18n('ambientModeTopicSourceArtGallery'));
        } else {
          console.warn('Invalid TopicSource value.', topicSourceVal);
        }
        break;
    }
    return breadcrumbs;
  }

  private computeShowBackButton_(): boolean {
    // Do not show the back button if hub is enabled.
    return !isPersonalizationHubEnabled() && this.path !== Paths.Collections;
  }

  private showHomeButton_(): boolean {
    return isPersonalizationHubEnabled();
  }

  private getBackButtonAriaLabel_(): string {
    return this.i18n('back', this.i18n('wallpaperLabel'));
  }

  private getHomeButtonAriaLabel_(): string {
    return this.i18n('ariaLabelHome');
  }

  private onBackClick_() {
    window.history.back();
  }

  private onBreadcrumbClick_(e: RepeaterEvent) {
    const index = e.model.index;
    // stay in same page if the user clicks on the last breadcrumb,
    // else navigate to the corresponding page.
    if (index < this.breadcrumbs_.length - 1) {
      const pathElements = this.path.split('/');
      const newPath = pathElements.slice(0, index + 2).join('/');
      if (isPathValid(newPath)) {
        // Unfocus the breadcrumb to focus on the page
        // with new path.
        const breadcrumb = e.target as HTMLElement;
        breadcrumb.blur();
        PersonalizationRouter.instance().goToRoute(newPath as Paths);
      }
    }
  }

  private onHomeIconClick_() {
    PersonalizationRouter.instance().goToRoute(Paths.Root);
  }
}

customElements.define(PersonalizationBreadcrumb.is, PersonalizationBreadcrumb);
