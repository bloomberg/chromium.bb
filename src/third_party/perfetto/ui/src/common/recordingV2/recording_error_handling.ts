// Copyright (C) 2022 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {
  showConnectionLostError,
  showNoDeviceSelected,
  showWebUSBError,
} from '../../frontend/error_dialog';
import {OnMessageCallback} from './recording_interfaces_v2';
import {
  PARSING_UNABLE_TO_DECODE_METHOD,
  PARSING_UNKNWON_REQUEST_ID,
  PARSING_UNRECOGNIZED_MESSAGE,
  PARSING_UNRECOGNIZED_PORT,
} from './traced_tracing_session';

// The pattern for handling recording error can have the following nesting in
// case of errors:
// A. wrapRecordingError -> wraps a promise
// B. onFailure -> has user defined logic and calls showRecordingModal
// C. showRecordingModal -> shows UX for a given error; this is not called
//    directly by wrapRecordingError, because we want the caller (such as the
//    UI) to dictate the UX

// This method takes a promise and a callback to be execute in case the promise
// fails. It then awaits the promise and executes the callback in case of
// failure. In the recording code it is used to wrap:
// 1. Acessing the WebUSB API.
// 2. Methods returning promises which can be rejected. For instance:
// a) When the user clicks 'Add a new device' but then doesn't select a valid
//    device.
// b) When the user starts a tracing session, but cancels it before they
//    authorize the session on the device.
export async function wrapRecordingError<T>(
    promise: Promise<T>, onFailure: OnMessageCallback): Promise<T|undefined> {
  try {
    return await promise;
  } catch (e) {
    // Sometimes the message is wrapped in an Error object, sometimes not, so
    // we make sure we transform it into a string.
    const errorMessage = getErrorMessage(e);
    onFailure(errorMessage);
    return undefined;
  }
}

// Shows a modal for every known type of error which can arise during recording.
// In this way, errors occuring at different levels of the recording process
// can be handled in a central location.
export function showRecordingModal(message: string): void {
  if ([
        'Unable to claim interface.',
        'The specified endpoint is not part of a claimed and selected ' +
            'alternate interface.',
      ].includes(message)) {
    showWebUSBError();
  } else if (
      [
        'A transfer error has occurred.',
        'The device was disconnected.',
        'The transfer was cancelled.',
      ].includes(message) ||
      isDeviceDisconnectedError(message)) {
    showConnectionLostError();
  } else if (message === 'No device selected.') {
    showNoDeviceSelected();
  } else if (isParsingError(message)) {
  } else {
    throw new Error(`${message}`);
  }
}

interface ErrorLikeObject {
  message?: unknown;
  error?: {message?: unknown};
  stack?: unknown;
  code?: unknown;
}

// Attempt to coerce an error object into a string message.
// Sometimes an error message is wrapped in an Error object, sometimes not.
function getErrorMessage(e: unknown|undefined|null) {
  if (e && typeof e === 'object') {
    const errorObject = e as ErrorLikeObject;
    if (errorObject.message) {  // regular Error Object
      return String(errorObject.message);
    } else if (errorObject.error?.message) {  // API result
      return String(errorObject.error.message);
    }
  }
  const asString = String(e);
  if (asString === '[object Object]') {
    try {
      return JSON.stringify(e);
    } catch (stringifyError) {
      // ignore failures and just fall through
    }
  }
  return asString;
}

function isDeviceDisconnectedError(message: string) {
  return message.includes('Device with serial') &&
      message.includes('was disconnected.');
}

function isParsingError(message: string) {
  for (const parsingIssue
           of [PARSING_UNKNWON_REQUEST_ID,
               PARSING_UNABLE_TO_DECODE_METHOD,
               PARSING_UNRECOGNIZED_PORT,
               PARSING_UNRECOGNIZED_MESSAGE]) {
    if (message.includes(parsingIssue)) {
      return true;
    }
  }
  return false;
}
