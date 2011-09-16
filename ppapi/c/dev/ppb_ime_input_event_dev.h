/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_ime_input_event_dev.idl modified Thu Sep 15 17:06:09 2011. */

#ifndef PPAPI_C_DEV_PPB_IME_INPUT_EVENT_DEV_H_
#define PPAPI_C_DEV_PPB_IME_INPUT_EVENT_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_IME_INPUT_EVENT_DEV_INTERFACE_0_1 "PPB_IMEInputEvent(Dev);0.1"
#define PPB_IME_INPUT_EVENT_DEV_INTERFACE PPB_IME_INPUT_EVENT_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_IMEInputEvent_Dev</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_IMEInputEvent_Dev {
  /**
   * IsIMEInputEvent() determines if a resource is an IME event.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to an event.
   *
   * @return <code>PP_TRUE</code> if the given resource is a valid input event.
   */
  PP_Bool (*IsIMEInputEvent)(PP_Resource resource);
  /**
   * GetText() returns the composition text as a UTF-8 string for the given IME
   * event.
   *
   * @param[in] ime_event A <code>PP_Resource</code> corresponding to an IME
   * event.
   *
   * @return A string var representing the composition text. For non-IME input
   * events the return value will be an undefined var.
   */
  struct PP_Var (*GetText)(PP_Resource ime_event);
  /**
   * GetSegmentNumber() returns the number of segments in the composition text.
   *
   * @param[in] ime_event A <code>PP_Resource</code> corresponding to an IME
   * event.
   *
   * @return The number of segments. For events other than COMPOSITION_UPDATE,
   * returns 0.
   */
  uint32_t (*GetSegmentNumber)(PP_Resource ime_event);
  /**
   * GetSegmentAt() returns the start and the end position of the index-th
   * segment in the composition text. The positions are given by byte-offsets
   * (not character-offsets) of the string returned by GetText(). The range of
   * the segment extends from start (inclusive) to end (exclusive). They satisfy
   * 0 <= start < end <= (byte-length of GetText()). When the event is not
   * COMPOSITION_UPDATE or index >= GetSegmentNumber(), the function returns
   * PP_FALSE and nothing else happens.
   *
   * @param[in] ime_event A <code>PP_Resource</code> corresponding to an IME
   * event.
   *
   * @param[in] index An integer indicating a segment.
   *
   * @param[out] start The start position of the index-th segment.
   *
   * @param[out] end The end position of the index-th segment.
   *
   * @return PP_TRUE when the start and the end position is successfully
   * obtained, and PP_FALSE otherwise.
   */
  PP_Bool (*GetSegmentAt)(PP_Resource ime_event,
                          uint32_t index,
                          uint32_t* start,
                          uint32_t* end);
  /**
   * GetTargetSegment() returns the index of the current target segment of
   * composition.
   *
   * @param[in] ime_event A <code>PP_Resource</code> corresponding to an IME
   * event.
   *
   * @return An integer indicating the index of the target segment. When there
   * is no active target segment, or the event is not COMPOSITION_UPDATE,
   * returns -1.
   */
  int32_t (*GetTargetSegment)(PP_Resource ime_event);
  /**
   * GetSelection() returns the range selected by caret in the composition text.
   *
   * @param[in] ime_event A <code>PP_Resource</code> corresponding to an IME
   * event.
   *
   * @param[out] start The start position of the current selection.
   *
   * @param[out] end The end position of the current selection.
   */
  void (*GetSelection)(PP_Resource ime_event, uint32_t* start, uint32_t* end);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_IME_INPUT_EVENT_DEV_H_ */

