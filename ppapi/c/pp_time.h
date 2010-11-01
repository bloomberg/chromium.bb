// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PP_TIME_H_
#define PPAPI_C_PP_TIME_H_

/**
 * @file
 * Defines the API ...
 *
 * @addtogroup PP
 * @{
 */

/**
 * PP_Time represents the "wall clock time" according to the browser and is
 * defined as the number of seconds since the Epoch (00:00:00 UTC, January 1,
 * 1970).
 */
typedef double PP_Time;

/**
 * Represents time ticks which is measured in seconds and is used for indicating
 * the time that certain messages were received. In contrast to PP_Time, it
 * does not correspond to any actual wall clock time and will not change
 * discontinuously if the user changes their computer clock.
 *
 * The units are in seconds, but are not measured relative to any particular
 * epoch, so the most you can do is compare two values.
 */
typedef double PP_TimeTicks;

/**
 * @}
 * End addtogroup PP
 */
#endif  // PPAPI_C_PP_TIME_H_
