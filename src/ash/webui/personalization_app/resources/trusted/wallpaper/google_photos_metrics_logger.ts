// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
export enum WallpaperGooglePhotosSource {
  PHOTOS = 0,
  ALBUMS = 1,
  NUM_SOURCES = 2,
}

const WallpaperGooglePhotosSourceHistogramName: string =
    'Ash.Wallpaper.GooglePhotos.Source';

/**
 * Records the section of the Wallpaper app from which a new Google Photos
 * wallpaper is selected.
 */
export function recordWallpaperGooglePhotosSourceUMA(
    source: WallpaperGooglePhotosSource) {
  assert(source < WallpaperGooglePhotosSource.NUM_SOURCES);

  chrome.metricsPrivate.recordEnumerationValue(
      WallpaperGooglePhotosSourceHistogramName, source,
      WallpaperGooglePhotosSource.NUM_SOURCES);
}
