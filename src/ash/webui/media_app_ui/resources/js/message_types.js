// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Message definitions passed over the MediaApp privileged/unprivileged pipe.
 */

/**
 * Enum for message types.
 * @enum {string}
 */
export const Message = {
  DELETE_FILE: 'delete-file',
  IFRAME_READY: 'iframe-ready',
  LOAD_EXTRA_FILES: 'load-extra-files',
  LOAD_FILES: 'load-files',
  NAVIGATE: 'navigate',
  NOTIFY_CURRENT_FILE: 'notify-current-file',
  OPEN_FEEDBACK_DIALOG: 'open-feedback-dialog',
  OPEN_FILE: 'open-file',
  OPEN_FILES_WITH_PICKER: 'open-files-with-picker',
  OVERWRITE_FILE: 'overwrite-file',
  RENAME_FILE: 'rename-file',
  REQUEST_SAVE_FILE: 'request-save-file',
  SAVE_AS: 'save-as',
  OPEN_ALLOWED_FILE: 'open-allowed-file',
  TOGGLE_BROWSER_FULLSCREEN_MODE: 'toggle-browser-fullscreen-mode',
};

/**
 * Message sent by the unprivileged context to request the privileged context to
 * delete the currently writable file.
 * If the supplied file `token` is invalid the request is rejected.
 * @typedef {{token: number}}
 */
export let DeleteFileMessage;

/**
 * Representation of a file passed in on the LoadFilesMessage.
 * @typedef {{
 *    token: number,
 *    file: ?File,
 *    name: string,
 *    error: string,
 *    canDelete: boolean,
 *    canRename: boolean
 * }}
 */
export let FileContext;

/**
 * Message sent by the privileged context to the unprivileged context indicating
 * the files available to open.
 * @typedef {{
 *    currentFileIndex: number,
 *    files: !Array<!FileContext>
 * }}
 */
export let LoadFilesMessage;

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * that the currently writable file be overridden with the provided `blob`.
 * If the supplied file `token` is invalid the request is rejected.
 * @typedef {{token: number, blob: !Blob}}
 */
export let OverwriteFileMessage;

/**
 * Response message to a successful overwrite (no error thrown). If fields are
 * defined, indicates that an overwrite failed, but the user was able to select
 * a new file from a file picker. The UI should update to reflect the new name.
 * `errorName` is the error on the write attempt that triggered the picker.
 * @typedef {{renamedTo: (string|undefined), errorName: (string|undefined)}}
 */
export let OverwriteViaFilePickerResponse;

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * the app be relaunched with the next/previous file in the current directory
 * set to writable. Direction is a number specifying how many files to advance
 * by, positive integers specify files "next" in the navigation order whereas
 * negative integers specify files "back" in the navigation order.
 * The `currentFileToken` is the token of the file which is currently opened,
 * this is used to decide what `direction` is in reference to.
 * @typedef {{direction: number, currentFileToken: (number|undefined)}}
 */
export let NavigateMessage;

/**
 * Enum for results of renaming a file.
 * @enum {number}
 */
export const RenameResult = {
  FILE_NO_LONGER_IN_LAST_OPENED_DIRECTORY: -1,
  SUCCESS: 0,
  FILE_EXISTS: 1,
};

/**
 * Message sent by the unprivileged context to request the privileged context to
 * rename the currently writable file.
 * If the supplied file `token` is invalid the request is rejected.
 * @typedef {{token: number, newFilename: string}}
 */
export let RenameFileMessage;

/** @typedef {{renameResult: RenameResult!}}  */
export let RenameFileResponse;

/**
 * Message sent by the unprivileged context to notify the privileged context
 * that the current file has been updated.
 * @typedef {{
 *   name: (string|undefined),
 *   type: (string|undefined),
 * }}
 */
export let NotifyCurrentFileMessage;

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * for the user to be prompted with a save file dialog. Once the user selects a
 * location a new file handle is created and a unique token to that file will be
 * returned. The file extension on `suggestedName` and the provided `mimeType`
 * are used to inform the save as dialog what file should be created. Once the
 * native filesystem api allows, this save as dialog will additionally have the
 * filename input be pre-filled with `suggestedName`.
 * If a non-zero startInToken is provided, the corresponding file handle is used
 * to start the file picker in the same folder as that file.
 * The `accept` array contains keys of preconfigured file filters to include on
 * the file picker file type dropdown. These are keys such as "PDF", "JPG",
 * "PNG", etc. that are known on both sides of API boundary.
 * @typedef {{
 *   suggestedName: string,
 *   mimeType: string,
 *   startInToken: number,
 *   accept: !Array<string>
 * }}
 */
export let RequestSaveFileMessage;

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * to show a file picker. Picked files will be sent down the launch path.
 * The `accept` array contains keys of preconfigured file filters to include on
 * the file picker file type dropdown. These are keys such as "AUDIO", "IMAGE",
 * "PDF", etc. that are known on both sides of API boundary.
 * @typedef {{
 *   startInToken: number,
 *   accept: !Array<string>
 * }}
 */
export let OpenFilesWithPickerMessage;

/**
 * Response message sent by the privileged context with a unique identifier for
 * the new writable file created on disk by the corresponding request save
 * file message.
 * @typedef {{pickedFileContext: !FileContext}}
 */
export let RequestSaveFileResponse;

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * for the provided blob to be saved in the location specified by
 * `pickedFileToken`. Once saved the new file takes over oldFileToken if it is
 * provided, else it gives itself a fresh token, then it becomes currently
 * writable. The file specified by oldFileToken is given a new token and pushed
 * forward in the navigation order. This method can be called with any file, not
 * just the currently writable file.
 * @typedef {{blob: !Blob, oldFileToken: ?number, pickedFileToken: number}}
 */
export let SaveAsMessage;

/**
 * Response message sent by the privileged context with the name of the new
 * current file.
 * @typedef {{newFilename: string}}
 */
export let SaveAsResponse;

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * an "allowed" file to be opened.
 * @typedef {{fileToken: number}}
 */
export let OpenAllowedFileMessage;

/**
 * Response message sent by the privileged context to "open" an allowed file.
 * @typedef {{file: !File}}
 */
export let OpenAllowedFileResponse;
