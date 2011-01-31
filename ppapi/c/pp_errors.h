/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PP_ERRORS_H_
#define PPAPI_C_PP_ERRORS_H_

/**
 * @file
 * Defines the API ...

 */

/**
 *
 * @addtogroup Enums
 * @{
 */

/** Errors are negative valued. */
enum {
  PP_OK = 0,

  /**
   * Returned by a function, taking a PP_CompletionCallback, that cannot
   * complete synchronously.  This return value indicates that the given
   * callback will be asynchronously notified of the final result once it is
   * available.
   */
  PP_ERROR_WOULDBLOCK = -1,

  /** Indicates failure for unspecified reasons. */
  PP_ERROR_FAILED = -2,

  /**
   * Indicates failure due to an asynchronous operation being interrupted,
   * typically as a result of user action.
   */
  PP_ERROR_ABORTED = -3,

  /** Indicates failure due to an invalid argument. */
  PP_ERROR_BADARGUMENT = -4,

  /** Indicates failure due to an invalid PP_Resource. */
  PP_ERROR_BADRESOURCE = -5,

  /** Indicates failure due to an unavailable PPAPI interface. */
  PP_ERROR_NOINTERFACE = -6,

  /** Indicates failure due to insufficient privileges. */
  PP_ERROR_NOACCESS = -7,

  /** Indicates failure due to insufficient memory. */
  PP_ERROR_NOMEMORY = -8,

  /** Indicates failure due to insufficient storage space. */
  PP_ERROR_NOSPACE = -9,

  /** Indicates failure due to insufficient storage quota. */
  PP_ERROR_NOQUOTA = -10,

  /** Indicates failure due to an action already being in progress. */
  PP_ERROR_INPROGRESS = -11,

  /** Indicates failure due to a file that does not exist. */
  PP_ERROR_FILENOTFOUND = -20,

  /** Indicates failure due to a file that already exists. */
  PP_ERROR_FILEEXISTS = -21,

  /** Indicates failure due to a file that is too big. */
  PP_ERROR_FILETOOBIG = -22,

  /** Indicates failure due to a file having been modified unexpectedly. */
  PP_ERROR_FILECHANGED = -23,

  /** Indicates failure due to a time limit being exceeded. */
  PP_ERROR_TIMEDOUT = -30,

  /** Indicates that the user cancelled rather providing expected input. */
  PP_ERROR_USERCANCEL = -40
};

/**
 * @}
 * End of addtogroup Enums
 */

#endif  /* PPAPI_C_PP_ERRORS_H_ */
