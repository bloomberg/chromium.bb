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
 * Each property of MetadataItem has error property also.
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
   * @public {Error|undefined}
   */
  this.sizeError;

  /**
   * @public {!Date|undefined}
   */
  this.modificationTime;

  /**
   * @public {Error|undefined}
   */
  this.modificationTimeError;

  /**
   * Thumbnail URL obtained from external provider.
   * @public {string|undefined}
   */
  this.thumbnailUrl;

  /**
   * @public {Error|undefined}
   */
  this.thumbnailUrlError;

  /**
   * @public {number|undefined}
   */
  this.imageWidth;

  /**
   * @public {Error|undefined}
   */
  this.imageWidthError;

  /**
   * @public {number|undefined}
   */
  this.imageHeight;

  /**
   * @public {Error|undefined}
   */
  this.imageHeightError;

  /**
   * @public {number|undefined}
   */
  this.imageRotation;

  /**
   * @public {Error|undefined}
   */
  this.imageRotationError;

  /**
   * Thumbnail obtained from content provider.
   * @public {string|undefined}
   */
  this.contentThumbnailUrl;

  /**
   * @public {Error|undefined}
   */
  this.contentThumbnailUrlError;

  /**
   * Thumbnail transformation obtained from content provider.
   * @public {!ImageTransformation|undefined}
   */
  this.contentThumbnailTransform;

  /**
   * @public {Error|undefined}
   */
  this.contentThumbnailTransformError;

  /**
   * Image transformation obtained from content provider.
   * @public {!ImageTransformation|undefined}
   */
  this.contentImageTransform;

  /**
   * @public {Error|undefined}
   */
  this.contentImageTransformError;

  /**
   * Whether the entry is pinned for ensuring it is available offline.
   * @public {boolean|undefined}
   */
  this.pinned;

  /**
   * @public {Error|undefined}
   */
  this.pinnedError;

  /**
   * Whether the entry is cached locally.
   * @public {boolean|undefined}
   */
  this.present;

  /**
   * @public {Error|undefined}
   */
  this.presentError;

  /**
   * Whether the entry is hosted document of google drive.
   * @public {boolean|undefined}
   */
  this.hosted;

  /**
   * @public {Error|undefined}
   */
  this.hostedError;

  /**
   * Whether the entry is modified locally and not synched yet.
   * @public {boolean|undefined}
   */
  this.dirty;

  /**
   * @public {Error|undefined}
   */
  this.dirtyError;

  /**
   * Whether the entry is present or hosted;
   * @public {boolean|undefined}
   */
  this.availableOffline;

  /**
   * @public {Error|undefined}
   */
  this.availableOfflineError;

  /**
   * @public {boolean|undefined}
   */
  this.availableWhenMetered;

  /**
   * @public {Error|undefined}
   */
  this.availableWhenMeteredError;

  /**
   * @public {string|undefined}
   */
  this.customIconUrl;

  /**
   * @public {Error|undefined}
   */
  this.customIconUrlError;

  /**
   * @public {string|undefined}
   */
  this.contentMimeType;

  /**
   * @public {Error|undefined}
   */
  this.contentMimeTypeError;

  /**
   * Whether the entry is shared explicitly with me.
   * @public {boolean|undefined}
   */
  this.sharedWithMe;

  /**
   * @public {Error|undefined}
   */
  this.sharedWithMeError;

  /**
   * Whether the entry is shared publicly.
   * @public {boolean|undefined}
   */
  this.shared;

  /**
   * @public {Error|undefined}
   */
  this.sharedError;

  /**
   * URL for open a file in browser tab.
   * @public {string|undefined}
   */
  this.externalFileUrl;

  /**
   * @public {Error|undefined}
   */
  this.externalFileUrlError;

  /**
   * @public {string|undefined}
   */
  this.mediaTitle;

  /**
   * @public {Error|undefined}
   */
  this.mediaTitleError;

  /**
   * @public {string|undefined}
   */
  this.mediaArtist;

  /**
   * @public {Error|undefined}
   */
  this.mediaArtistError;

  /**
   * Mime type obtained by content provider based on URL.
   * TODO(hirono): Remove the mediaMimeType.
   * @public {string|undefined}
   */
  this.mediaMimeType;

  /**
   * @public {Error|undefined}
   */
  this.mediaMimeTypeError;

  /**
   * "Image File Directory" obtained from EXIF header.
   * @public {!Object|undefined}
   */
  this.ifd;

  /**
   * @public {Error|undefined}
   */
  this.ifdError;

  /**
   * @public {boolean|undefined}
   */
  this.exifLittleEndian;

  /**
   * @public {Error|undefined}
   */
  this.exifLittleEndianError;
}
