// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions to be used in trusted code.
 */

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {AmbientModeAlbum, GooglePhotosPhoto, TopicSource, WallpaperImage, WallpaperLayout} from '../trusted/personalization_app.mojom-webui.js';


export function isWallpaperImage(obj: any): obj is WallpaperImage {
  return !!obj && typeof obj.assetId === 'bigint';
}

export function isFilePath(obj: any): obj is FilePath {
  return !!obj && typeof obj.path === 'string' && obj.path;
}

/** Checks whether |obj| is an instance of |GooglePhotosPhoto|. */
export function isGooglePhotosPhoto(obj: any): obj is GooglePhotosPhoto {
  return !!obj && typeof obj.id === 'string';
}

/**
 * Convert a string layout value to the corresponding enum.
 */
export function getWallpaperLayoutEnum(layout: string): WallpaperLayout {
  switch (layout) {
    case 'FILL':
      return WallpaperLayout.kCenterCropped;
    case 'CENTER':  // fall through
    default:
      return WallpaperLayout.kCenter;
  }
}

/**
 * Checks if argument is a string with non-zero length.
 */
export function isNonEmptyString(maybeString: unknown): maybeString is string {
  return typeof maybeString === 'string' && maybeString.length > 0;
}

/**
 * Checks if a number is within a range.
 */
export function inBetween(
    num: number, minVal: number, maxVal: number): boolean {
  return minVal <= num && num <= maxVal;
}

/**
 * Appends a suffix to request wallpaper images with the longest of width or
 * height being 512 pixels. This should ensure that the wallpaper image is
 * large enough to cover a grid item but not significantly more so.
 */
export function appendMaxResolutionSuffix(value: Url): Url {
  return {...value, url: value.url + '=s512'};
}

/**
 * Wallpaper images sometimes have a resolution suffix appended to the end of
 * the image. This is typically to fetch a high resolution image to show as the
 * user's wallpaper. We do not want the full resolution here, so remove the
 * suffix to get a 512x512 preview.
 * TODO(b/186807814) support different resolution parameters here.
 */
export function removeHighResolutionSuffix(url: string): string {
  return url.replace(/=w\d+$/, '');
}

/**
 * Removes the resolution suffix at the end of an image (from character '=' to
 * the end) and replace it with a new resolution suffix.
 */
export function replaceResolutionSuffix(
    url: string, resolution: string): string {
  return url.replace(/=w[\w-]+$/, resolution);
}

/**
 * Returns whether the given URL starts with http:// or https://.
 */
export function hasHttpScheme(url: string): boolean {
  return url.startsWith('http://') || url.startsWith('https://');
}

/**
 * Returns whether the given album is Recent Highlights.
 */
export function isRecentHighlightsAlbum(album: AmbientModeAlbum): boolean {
  return album.id === 'RecentHighlights';
}

/**
 * Returns photo count string.
 */
export function getPhotoCount(photoCount: number): string {
  if (photoCount <= 1) {
    return loadTimeData.getStringF(
        'ambientModeAlbumsSubpagePhotosNumSingularDesc', photoCount);
  }
  return loadTimeData.getStringF(
      'ambientModeAlbumsSubpagePhotosNumPluralDesc', photoCount);
}

/**
 * Returns the topic source name.
 */
export function getTopicSourceName(topicSource: TopicSource): string {
  switch (topicSource) {
    case TopicSource.kGooglePhotos:
      return loadTimeData.getString('ambientModeTopicSourceGooglePhotos');
    case TopicSource.kArtGallery:
      return loadTimeData.getString('ambientModeTopicSourceArtGallery');
    default:
      console.warn('Invalid TopicSource value.');
      return '';
  }
}

/**
 * Returns a x-length dummy array of zeros (0s)
 */
export function getZerosArray(x: number): number[] {
  return new Array(x).fill(0);
}

/** Converts a String16 to a JavaScript String. */
export function decodeString16(str: String16|null): string {
  return str ? str.data.map(ch => String.fromCodePoint(ch)).join('') : '';
}
