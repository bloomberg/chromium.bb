// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   name: string,
 *   size: number,
 *   readable: boolean,
 * }}
 */
export let Attachment;

/** @enum {string} */
export const DisplayAnnotationsAction = {
  DISPLAY_ANNOTATIONS: 'display-annotations',
  HIDE_ANNOTATIONS: 'hide-annotations',
};

/**
 * @typedef {{
 *   author: string,
 *   canSerializeDocument: boolean,
 *   creationDate: string,
 *   creator: string,
 *   fileSize: string,
 *   keywords: string,
 *   linearized: boolean,
 *   modDate: string,
 *   pageSize: string,
 *   producer: string,
 *   subject: string,
 *   title: string,
 *   version: string,
 * }}
 */
export let DocumentMetadata;

/**
 * Enumeration of page fitting types.
 * @enum {string}
 */
export const FittingType = {
  NONE: 'none',
  FIT_TO_PAGE: 'fit-to-page',
  FIT_TO_WIDTH: 'fit-to-width',
  FIT_TO_HEIGHT: 'fit-to-height',
};

/**
 * @typedef {{
 *   messageId: string,
 *   namedDestinationView: (string|undefined),
 *   pageNumber: number,
 * }}
 */
export let NamedDestinationMessageData;

/**
 * Enumeration of save message request types. Must match `SaveRequestType` in
 * pdf/pdf_view_plugin_base.h.
 * @enum {number}
 */
export const SaveRequestType = {
  ANNOTATION: 0,
  ORIGINAL: 1,
  EDITED: 2,
};

/** @typedef {{x: number, y: number}} */
export let Point;
