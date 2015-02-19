// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *  scaleX: number,
 *  scaleY: number,
 *  rotate90: number
 * }}
 */
var ImageTransformation;

/**
 * @constructor
 * @struct
 */
function MetadataItem() {
  /**
   * Size of the file. -1 for directory.
   * @public {number|undefined}
   */
  this.size;

  /**
   * @public {!Date|undefined}
   */
  this.modificationTime;

  /**
   * Thumbnail URL obtained from external provider.
   * @public {string|undefined}
   */
  this.thumbnailUrl;

  /**
   * @public {number|undefined}
   */
  this.imageWidth;

  /**
   * @public {number|undefined}
   */
  this.imageHeight;

  /**
   * @public {number|undefined}
   */
  this.imageRotation;

  /**
   * Thumbnail obtained from content provider.
   * @public {string|undefined}
   */
  this.contentThumbnailUrl;

  /**
   * Thumbnail transformation obtained from content provider.
   * @public {!ImageTransformation|undefined}
   */
  this.contentThumbnailTransform;

  /**
   * Image transformation obtained from content provider.
   * @public {!ImageTransformation|undefined}
   */
  this.contentImageTransform;

  /**
   * Whether the entry is pinned for ensuring it is available offline.
   * @public {boolean|undefined}
   */
  this.pinned;

  /**
   * Whether the entry is cached locally.
   * @public {boolean|undefined}
   */
  this.present;

  /**
   * Whether the entry is hosted document of google drive.
   * @public {boolean|undefined}
   */
  this.hosted;

  /**
   * Whether the entry is modified locally and not synched yet.
   * @public {boolean|undefined}
   */
  this.dirty;

  /**
   * Whether the entry is present or hosted;
   * @public {boolean|undefined}
   */
  this.availableOffline;

  /**
   * @public {boolean|undefined}
   */
  this.availableWhenMetered;

  /**
   * @public {string|undefined}
   */
  this.customIconUrl;

  /**
   * @public {string|undefined}
   */
  this.contentMimeType;

  /**
   * Whether the entry is shared explicitly with me.
   * @public {boolean|undefined}
   */
  this.sharedWithMe;

  /**
   * Whether the entry is shared publicly.
   * @public {boolean|undefined}
   */
  this.shared;

  /**
   * URL for open a file in browser tab.
   * @public {string|undefined}
   */
  this.externalFileUrl;

  /**
   * @public {string|undefined}
   */
  this.mediaTitle;

  /**
   * @public {string|undefined}
   */
  this.mediaArtist;
}
