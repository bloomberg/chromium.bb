// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {Actions} from '../personalization_actions.js';
import {WallpaperCollection, WallpaperImage} from '../personalization_app.mojom-webui.js';
import {ReducerFunction} from '../personalization_reducers.js';
import {PersonalizationState} from '../personalization_state.js';

import {WallpaperActionName} from './wallpaper_actions.js';
import {WallpaperState} from './wallpaper_state.js';

export type DisplayableImage = FilePath|WallpaperImage;

function backdropReducer(
    state: WallpaperState['backdrop'], action: Actions,
    _: PersonalizationState): WallpaperState['backdrop'] {
  switch (action.name) {
    case WallpaperActionName.SET_COLLECTIONS:
      return {collections: action.collections, images: {}};
    case WallpaperActionName.SET_IMAGES_FOR_COLLECTION:
      if (!state.collections) {
        console.warn('Cannot set images when collections is null');
        return state;
      }
      if (!state.collections.some(({id}) => id === action.collectionId)) {
        console.warn(
            'Cannot store images for unknown collection', action.collectionId);
        return state;
      }
      return {
        ...state,
        images: {...state.images, [action.collectionId]: action.images}
      };
    default:
      return state;
  }
}

function loadingReducer(
    state: WallpaperState['loading'], action: Actions,
    _: PersonalizationState): WallpaperState['loading'] {
  switch (action.name) {
    case WallpaperActionName.BEGIN_LOAD_IMAGES_FOR_COLLECTIONS:
      return {
        ...state,
        images: action.collections.reduce(
            (result, {id}) => {
              result[id] = true;
              return result;
            },
            {} as Record<WallpaperCollection['id'], boolean>)
      };
    case WallpaperActionName.BEGIN_LOAD_LOCAL_IMAGE_DATA:
      return {
        ...state,
        local: {...state.local, data: {...state.local.data, [action.id]: true}}
      };
    case WallpaperActionName.BEGIN_LOAD_SELECTED_IMAGE:
      return {...state, selected: true};
    case WallpaperActionName.BEGIN_SELECT_IMAGE:
      return {...state, setImage: state.setImage + 1};
    case WallpaperActionName.END_SELECT_IMAGE:
      if (state.setImage <= 0) {
        console.error('Impossible state for loading.setImage');
        // Reset to 0.
        return {...state, setImage: 0};
      }
      return {...state, setImage: state.setImage - 1};
    case WallpaperActionName.SET_COLLECTIONS:
      return {...state, collections: false};
    case WallpaperActionName.SET_IMAGES_FOR_COLLECTION:
      return {
        ...state,
        images: {...state.images, [action.collectionId]: false},
      };
    case WallpaperActionName.BEGIN_LOAD_LOCAL_IMAGES:
      return {
        ...state,
        local: {
          ...state.local,
          images: true,
        },
      };
    case WallpaperActionName.SET_LOCAL_IMAGES:
      return {
        ...state,
        local: {
          // Only keep loading state for most recent local images.
          data: (action.images || [])
                    .reduce(
                        (result, {path}) => {
                          if (state.local.data.hasOwnProperty(path)) {
                            result[path] = state.local.data[path];
                          }
                          return result;
                        },
                        {} as Record<FilePath['path'], boolean>),
          // Image list is done loading.
          images: false,
        },
      };
    case WallpaperActionName.SET_LOCAL_IMAGE_DATA:
      return {
        ...state,
        local: {
          ...state.local,
          data: {
            ...state.local.data,
            [action.id]: false,
          },
        },
      };
    case WallpaperActionName.SET_SELECTED_IMAGE:
      if (state.setImage === 0) {
        return {...state, selected: false};
      }
      return state;
    case WallpaperActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE:
      return {...state, refreshWallpaper: true};
    case WallpaperActionName.SET_UPDATED_DAILY_REFRESH_IMAGE:
      return {...state, refreshWallpaper: false};
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM:
      assert(state.googlePhotos.photosByAlbumId[action.albumId] === undefined);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          photosByAlbumId: {
            ...state.googlePhotos.photosByAlbumId,
            [action.albumId]: true,
          },
        },
      };
    case WallpaperActionName.SET_GOOGLE_PHOTOS_ALBUM:
      assert(state.googlePhotos.photosByAlbumId[action.albumId] === true);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          photosByAlbumId: {
            ...state.googlePhotos.photosByAlbumId,
            [action.albumId]: false,
          },
        },
      };
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS:
      assert(state.googlePhotos.albums === false);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          albums: true,
        },
      };
    case WallpaperActionName.SET_GOOGLE_PHOTOS_ALBUMS:
      assert(state.googlePhotos.albums === true);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          albums: false,
        },
      };
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_COUNT:
      assert(state.googlePhotos.count === false);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          count: true,
        },
      };
    case WallpaperActionName.SET_GOOGLE_PHOTOS_COUNT:
      assert(state.googlePhotos.count === true);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          count: false,
        },
      };
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS:
      assert(state.googlePhotos.photos === false);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          photos: true,
        },
      };
    case WallpaperActionName.SET_GOOGLE_PHOTOS_PHOTOS:
      assert(state.googlePhotos.photos === true);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          photos: false,
        },
      };
    default:
      return state;
  }
}

function localReducer(
    state: WallpaperState['local'], action: Actions,
    _: PersonalizationState): WallpaperState['local'] {
  switch (action.name) {
    case WallpaperActionName.SET_LOCAL_IMAGES:
      return {
        ...state,
        images: action.images,
        // Only keep image thumbnails if the image is still in |images|.
        data: (action.images || [])
                  .reduce(
                      (result, {path}) => {
                        if (state.data.hasOwnProperty(path)) {
                          result[path] = state.data[path];
                        }
                        return result;
                      },
                      {} as Record<FilePath['path'], string>),
      };
    case WallpaperActionName.SET_LOCAL_IMAGE_DATA:
      return {
        ...state,
        data: {
          ...state.data,
          [action.id]: action.data,
        }
      };
    default:
      return state;
  }
}

function currentSelectedReducer(
    state: WallpaperState['currentSelected'], action: Actions,
    _: PersonalizationState): WallpaperState['currentSelected'] {
  switch (action.name) {
    case WallpaperActionName.SET_SELECTED_IMAGE:
      return action.image;
    default:
      return state;
  }
}

/**
 * Reducer for the pending selected image. The pendingSelected state is set when
 * a user clicks on an image and before the client code is reached.
 *
 * Note: We allow multiple concurrent requests of selecting images while only
 * keeping the latest pending image and failing others occurred in between.
 * The pendingSelected state should not be cleared in this scenario (of multiple
 * concurrent requests). Otherwise, it results in a unwanted jumpy motion of
 * selected state.
 */
function pendingSelectedReducer(
    state: WallpaperState['pendingSelected'], action: Actions,
    globalState: PersonalizationState): WallpaperState['pendingSelected'] {
  switch (action.name) {
    case WallpaperActionName.BEGIN_SELECT_IMAGE:
      return action.image;
    case WallpaperActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE:
      return null;
    case WallpaperActionName.SET_SELECTED_IMAGE:
      const {image} = action;
      if (!image) {
        console.warn('pendingSelectedReducer: Failed to get selected image.');
        return null;
      } else if (globalState.wallpaper.loading.setImage == 0) {
        // Clear the pending state when there are no more requests.
        return null;
      }
      return state;
    case WallpaperActionName.SET_FULLSCREEN_ENABLED:
      if (!action.enabled) {
        // Clear the pending selected state after full screen is dismissed.
        return null;
      }
      return state;
    case WallpaperActionName.END_SELECT_IMAGE:
      const {success} = action;
      if (!success && globalState.wallpaper.loading.setImage <= 1) {
        // Clear the pending selected state if an error occurs and
        // there are no multiple concurrent requests of selecting images.
        return null;
      }
      return state;
    default:
      return state;
  }
}

function dailyRefreshReducer(
    state: WallpaperState['dailyRefresh'], action: Actions,
    _: PersonalizationState): WallpaperState['dailyRefresh'] {
  switch (action.name) {
    case WallpaperActionName.SET_DAILY_REFRESH_COLLECTION_ID:
      return {
        ...state,
        collectionId: action.collectionId,
      };
    default:
      return state;
  }
}


function fullscreenReducer(
    state: WallpaperState['fullscreen'], action: Actions,
    _: PersonalizationState): WallpaperState['fullscreen'] {
  switch (action.name) {
    case WallpaperActionName.SET_FULLSCREEN_ENABLED:
      return action.enabled;
    default:
      return state;
  }
}

function googlePhotosReducer(
    state: WallpaperState['googlePhotos'], action: Actions,
    _: PersonalizationState): WallpaperState['googlePhotos'] {
  switch (action.name) {
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM:
      // The list of photos for an album should be loaded only once.
      assert(state.albums?.some(album => album.id === action.albumId));
      assert(state.photosByAlbumId[action.albumId] === undefined);
      return state;
    case WallpaperActionName.SET_GOOGLE_PHOTOS_ALBUM:
      assert(state.albums?.some(album => album.id === action.albumId));
      assert(action.albumId !== undefined);
      assert(action.photos !== undefined);
      return {
        ...state,
        photosByAlbumId: {
          ...state.photosByAlbumId,
          [action.albumId]: action.photos,
        },
      };
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS:
      // The list of albums should be loaded only once.
      assert(state.albums === undefined);
      return state;
    case WallpaperActionName.SET_GOOGLE_PHOTOS_ALBUMS:
      assert(action.albums !== undefined);
      return {
        ...state,
        albums: action.albums,
      };
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_COUNT:
      // The total count of photos should be loaded only once.
      assert(state.count === undefined);
      return state;
    case WallpaperActionName.SET_GOOGLE_PHOTOS_COUNT:
      assert(action.count !== undefined);
      return {
        ...state,
        count: action.count,
      };
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS:
      // The list of photos should be loaded only once.
      assert(state.photos === undefined);
      return state;
    case WallpaperActionName.SET_GOOGLE_PHOTOS_PHOTOS:
      assert(action.photos !== undefined);
      return {
        ...state,
        photos: action.photos,
      };
    default:
      return state;
  }
}

export const wallpaperReducers:
    {[K in keyof WallpaperState]: ReducerFunction<WallpaperState[K]>} = {
      backdrop: backdropReducer,
      loading: loadingReducer,
      local: localReducer,
      currentSelected: currentSelectedReducer,
      pendingSelected: pendingSelectedReducer,
      dailyRefresh: dailyRefreshReducer,
      fullscreen: fullscreenReducer,
      googlePhotos: googlePhotosReducer,
    };
