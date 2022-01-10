// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {Action} from 'chrome://resources/js/cr/ui/store.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {CurrentWallpaper, WallpaperCollection, WallpaperImage} from './personalization_app.mojom-webui.js';
import {DisplayableImage} from './personalization_reducers.js';

/**
 * @fileoverview Defines the actions to change state.
 */

export enum ActionName {
  BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM = 'begin_load_google_photos_album',
  BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS = 'begin_load_google_photos_albums',
  BEGIN_LOAD_GOOGLE_PHOTOS_COUNT = 'begin_load_google_photos_count',
  BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS = 'begin_load_google_photos_photos',
  BEGIN_LOAD_IMAGES_FOR_COLLECTIONS = 'begin_load_images_for_collections',
  BEGIN_LOAD_LOCAL_IMAGES = 'begin_load_local_images',
  BEGIN_LOAD_LOCAL_IMAGE_DATA = 'begin_load_local_image_data',
  BEGIN_LOAD_SELECTED_IMAGE = 'begin_load_selected_image',
  BEGIN_SELECT_IMAGE = 'begin_select_image',
  BEGIN_UPDATE_DAILY_REFRESH_IMAGE = 'begin_update_daily_refresh_image',
  END_SELECT_IMAGE = 'end_select_image',
  SET_COLLECTIONS = 'set_collections',
  SET_DAILY_REFRESH_COLLECTION_ID = 'set_daily_refresh_collection_id',
  SET_GOOGLE_PHOTOS_ALBUM = 'set_google_photos_album',
  SET_GOOGLE_PHOTOS_ALBUMS = 'set_google_photos_albums',
  SET_GOOGLE_PHOTOS_COUNT = 'set_google_photos_count',
  SET_GOOGLE_PHOTOS_PHOTOS = 'set_google_photos_photos',
  SET_IMAGES_FOR_COLLECTION = 'set_images_for_collection',
  SET_LOCAL_IMAGES = 'set_local_images',
  SET_LOCAL_IMAGE_DATA = 'set_local_image_data',
  SET_SELECTED_IMAGE = 'set_selected_image',
  SET_UPDATED_DAILY_REFRESH_IMAGE = 'set_updated_daily_refreshed_image',
  DISMISS_ERROR = 'dismiss_error',
  SET_FULLSCREEN_ENABLED = 'set_fullscreen_enabled',
}

export type Actions =
    BeginLoadGooglePhotosAlbumAction|BeginLoadGooglePhotosAlbumsAction|
    BeginLoadGooglePhotosCountAction|BeginLoadGooglePhotosPhotosAction|
    BeginLoadImagesForCollectionsAction|BeginLoadLocalImagesAction|
    BeginLoadLocalImageDataAction|BeginUpdateDailyRefreshImageAction|
    BeginLoadSelectedImageAction|BeginSelectImageAction|EndSelectImageAction|
    SetCollectionsAction|SetDailyRefreshCollectionIdAction|
    SetGooglePhotosAlbumAction|SetGooglePhotosAlbumsAction|
    SetGooglePhotosCountAction|SetGooglePhotosPhotosAction|
    SetImagesForCollectionAction|SetLocalImageDataAction|SetLocalImagesAction|
    SetUpdatedDailyRefreshImageAction|SetSelectedImageAction|DismissErrorAction|
    SetFullscreenEnabledAction;

export type BeginLoadGooglePhotosAlbumAction = Action&{
  name: ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM;
  albumId: string;
};

/**
 * Notify that the app is loading the list of Google Photos photos for the album
 * associated with the specified id.
 */
export function beginLoadGooglePhotosAlbumAction(albumId: string):
    BeginLoadGooglePhotosAlbumAction {
  return {albumId, name: ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM};
}

export type BeginLoadGooglePhotosAlbumsAction = Action&{
  name: ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS;
};

/**
 * Notify that the app is loading the list of Google Photos albums.
 */
export function beginLoadGooglePhotosAlbumsAction():
    BeginLoadGooglePhotosAlbumsAction {
  return {name: ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS};
}

export type BeginLoadGooglePhotosCountAction = Action&{
  name: ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_COUNT;
};

/**
 * Notify that the app is loading the count of Google Photos photos.
 */
export function beginLoadGooglePhotosCountAction():
    BeginLoadGooglePhotosCountAction {
  return {name: ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_COUNT};
}

export type BeginLoadGooglePhotosPhotosAction = Action&{
  name: ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS;
};
/**
 * Notify that the app is loading the list of Google Photos photos.
 */
export function beginLoadGooglePhotosPhotosAction():
    BeginLoadGooglePhotosPhotosAction {
  return {name: ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS};
}

export type BeginLoadImagesForCollectionsAction = Action&{
  name: ActionName.BEGIN_LOAD_IMAGES_FOR_COLLECTIONS;
  collections: WallpaperCollection[];
};

/**
 * Notify that app is loading image list for the given collection.
 */
export function beginLoadImagesForCollectionsAction(
    collections: WallpaperCollection[]): BeginLoadImagesForCollectionsAction {
  return {
    collections,
    name: ActionName.BEGIN_LOAD_IMAGES_FOR_COLLECTIONS,
  };
}

export type BeginLoadLocalImagesAction = Action&{
  name: ActionName.BEGIN_LOAD_LOCAL_IMAGES;
};
/**
 * Notify that app is loading local image list.
 */
export function beginLoadLocalImagesAction(): BeginLoadLocalImagesAction {
  return {name: ActionName.BEGIN_LOAD_LOCAL_IMAGES};
}

export type BeginLoadLocalImageDataAction = Action&{
  name: ActionName.BEGIN_LOAD_LOCAL_IMAGE_DATA;
  id: string;
};

/**
 * Notify that app is loading thumbnail for the given local image.
 */
export function beginLoadLocalImageDataAction(image: FilePath):
    BeginLoadLocalImageDataAction {
  return {
    id: image.path,
    name: ActionName.BEGIN_LOAD_LOCAL_IMAGE_DATA,
  };
}

export type BeginUpdateDailyRefreshImageAction = Action&{
  name: ActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE;
};

/**
 * Notify that a user has clicked on the refresh button.
 */
export function beginUpdateDailyRefreshImageAction():
    BeginUpdateDailyRefreshImageAction {
  return {
    name: ActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE,
  };
}

export type BeginLoadSelectedImageAction = Action&{
  name: ActionName.BEGIN_LOAD_SELECTED_IMAGE;
};

/**
 * Notify that app is loading currently selected image information.
 */
export function beginLoadSelectedImageAction(): BeginLoadSelectedImageAction {
  return {name: ActionName.BEGIN_LOAD_SELECTED_IMAGE};
}

export type BeginSelectImageAction = Action&{
  name: ActionName.BEGIN_SELECT_IMAGE;
  image: DisplayableImage;
};

/**
 * Notify that a user has clicked on an image to set as wallpaper.
 */
export function beginSelectImageAction(image: DisplayableImage):
    BeginSelectImageAction {
  return {name: ActionName.BEGIN_SELECT_IMAGE, image};
}

export type EndSelectImageAction = Action&{
  name: ActionName.END_SELECT_IMAGE;
  image: DisplayableImage;
  success: boolean;
};

/**
 * Notify that the user-initiated action to set image has finished.
 */
export function endSelectImageAction(
    image: DisplayableImage, success: boolean): EndSelectImageAction {
  return {name: ActionName.END_SELECT_IMAGE, image, success};
}

export type SetCollectionsAction = Action&{
  name: ActionName.SET_COLLECTIONS;
  collections: WallpaperCollection[]|null;
};

/**
 * Set the collections. May be called with null if an error occurred.
 */
export function setCollectionsAction(collections: WallpaperCollection[]|
                                     null): SetCollectionsAction {
  return {
    name: ActionName.SET_COLLECTIONS,
    collections,
  };
}

export type SetDailyRefreshCollectionIdAction = Action&{
  name: ActionName.SET_DAILY_REFRESH_COLLECTION_ID;
  collectionId: string|null;
};

/**
 * Set and enable daily refresh for given collectionId.
 */
export function setDailyRefreshCollectionIdAction(collectionId: string|null):
    SetDailyRefreshCollectionIdAction {
  return {
    collectionId,
    name: ActionName.SET_DAILY_REFRESH_COLLECTION_ID,
  };
}

export type SetGooglePhotosAlbumAction = Action&{
  name: ActionName.SET_GOOGLE_PHOTOS_ALBUM;
  albumId: string;
  photos: unknown[];
};

/**
 * Sets the list of Google Photos photos for the album associated with the
 * specified id. May be called with null on error.
 */
export function setGooglePhotosAlbumAction(
    albumId: string, photos: unknown[]): SetGooglePhotosAlbumAction {
  return {albumId, photos, name: ActionName.SET_GOOGLE_PHOTOS_ALBUM};
}

export type SetGooglePhotosAlbumsAction = Action&{
  name: ActionName.SET_GOOGLE_PHOTOS_ALBUMS;
  albums: WallpaperCollection[]|null;
};

/**
 * Sets the list of Google Photos albums. May be called with null on error.
 */
export function setGooglePhotosAlbumsAction(albums: WallpaperCollection[]|
                                            null): SetGooglePhotosAlbumsAction {
  return {albums, name: ActionName.SET_GOOGLE_PHOTOS_ALBUMS};
}

export type SetGooglePhotosCountAction = Action&{
  name: ActionName.SET_GOOGLE_PHOTOS_COUNT;
  count: bigint|null;
};

/**
 * Sets the count of Google Photos photos. May be called with null on error.
 */
export function setGooglePhotosCountAction(count: bigint|
                                           null): SetGooglePhotosCountAction {
  return {count, name: ActionName.SET_GOOGLE_PHOTOS_COUNT};
}

export type SetGooglePhotosPhotosAction = Action&{
  name: ActionName.SET_GOOGLE_PHOTOS_PHOTOS;
  photos: unknown[]|null;
};

/**
 * Sets the list of Google Photos photos. May be called with null on error.
 */
export function setGooglePhotosPhotosAction(photos: unknown[]|
                                            null): SetGooglePhotosPhotosAction {
  return {photos, name: ActionName.SET_GOOGLE_PHOTOS_PHOTOS};
}

export type SetImagesForCollectionAction = Action&{
  name: ActionName.SET_IMAGES_FOR_COLLECTION;
  collectionId: string;
  images: WallpaperImage[]|null;
};

/**
 * Set the images for a given collection. May be called with null if an error
 * occurred.
 */
export function setImagesForCollectionAction(
    collectionId: string,
    images: WallpaperImage[]|null): SetImagesForCollectionAction {
  return {
    collectionId,
    images,
    name: ActionName.SET_IMAGES_FOR_COLLECTION,
  };
}

export type SetLocalImageDataAction = Action&{
  name: ActionName.SET_LOCAL_IMAGE_DATA;
  id: string;
  data: string;
};

/**
 * Set the thumbnail data for a local image.
 */
export function setLocalImageDataAction(
    filePath: FilePath, data: string): SetLocalImageDataAction {
  return {
    id: filePath.path,
    data,
    name: ActionName.SET_LOCAL_IMAGE_DATA,
  };
}

export type SetLocalImagesAction = Action&{
  name: ActionName.SET_LOCAL_IMAGES;
  images: FilePath[]|null;
};

/** Set the list of local images. */
export function setLocalImagesAction(images: FilePath[]|
                                     null): SetLocalImagesAction {
  return {
    images,
    name: ActionName.SET_LOCAL_IMAGES,
  };
}

export type SetUpdatedDailyRefreshImageAction = Action&{
  name: ActionName.SET_UPDATED_DAILY_REFRESH_IMAGE;
};

/**
 * Notify that a image has been refreshed.
 */
export function setUpdatedDailyRefreshImageAction():
    SetUpdatedDailyRefreshImageAction {
  return {
    name: ActionName.SET_UPDATED_DAILY_REFRESH_IMAGE,
  };
}

export type SetSelectedImageAction = Action&{
  name: ActionName.SET_SELECTED_IMAGE;
  image: CurrentWallpaper|null;
};

/**
 * Returns an action to set the current image as currently selected across the
 * app. Can be called with null to represent no image currently selected or that
 * an error occurred.
 */
export function setSelectedImageAction(image: CurrentWallpaper|
                                       null): SetSelectedImageAction {
  return {
    image,
    name: ActionName.SET_SELECTED_IMAGE,
  };
}

export type DismissErrorAction = Action&{
  name: ActionName.DISMISS_ERROR;
};

/**
 * Dismiss the current error if there is any.
 */
export function dismissErrorAction(): DismissErrorAction {
  return {name: ActionName.DISMISS_ERROR};
}

export type SetFullscreenEnabledAction = Action&{
  name: ActionName.SET_FULLSCREEN_ENABLED;
  enabled: boolean;
};

/**
 * Enable/disable the fullscreen preview mode for wallpaper.
 */
export function setFullscreenEnabledAction(enabled: boolean):
    SetFullscreenEnabledAction {
  assert(typeof enabled === 'boolean');
  return {name: ActionName.SET_FULLSCREEN_ENABLED, enabled};
}
