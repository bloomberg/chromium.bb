/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppp_instance.idl modified Tue Aug 23 11:29:06 2011. */

#ifndef PPAPI_C_PPP_INSTANCE_H_
#define PPAPI_C_PPP_INSTANCE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPP_INSTANCE_INTERFACE_1_0 "PPP_Instance;1.0"
#define PPP_INSTANCE_INTERFACE PPP_INSTANCE_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPP_Instance</code> structure - a series of
 * pointers to methods that you must implement in your module.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPP_Instance</code> interface contains pointers to a series of
 * functions that you must implement in your module. These functions can be
 * trivial (simply return the default return value) unless you want your module
 * to handle events such as change of focus or input events (keyboard/mouse)
 * events.
 */
struct PPP_Instance {
  /**
   * DidCreate() is a creation handler that is called when a new instance is
   * created. This function is called for each instantiation on the page,
   * corresponding to one \<embed\> tag on the page.
   *
   * Generally you would handle this call by initializing the information
   * your module associates with an instance and creating a mapping from the
   * given <code>PP_Instance</code> handle to this data. The
   * <code>PP_Instance</code> handle will be used in subsequent calls to
   * identify which instance the call pertains to.
   *
   * It's possible for more than one instance to be created in a single module.
   * This means that you may get more than one <code>OnCreate</code> without an
   * <code>OnDestroy</code> in between, and should be prepared to maintain
   * multiple states associated with each instance.
   *
   * If this function reports a failure (by returning <code>PP_FALSE</code>),
   * the instance will be deleted.
   *
   * @param[in] instance A new <code>PP_Instance</code> identifying one
   * instance of a module. This is an opaque handle.
   *
   * @param[in] argc The number of arguments contained in <code>argn</code>
   * and <code>argv</code>.
   *
   * @param[in] argn An array of argument names.  These argument names are
   * supplied in the \<embed\> tag, for example:
   * <code>\<embed id="nacl_module" dimensions="2"\></code> will produce two
   * argument names: "id" and "dimensions."
   *
   * @param[in] argv An array of argument values.  These are the values of the
   * arguments listed in the \<embed\> tag, for example
   * <code>\<embed id="nacl_module" dimensions="2"\></code> will produce two
   * argument values: "nacl_module" and "2".  The indices of these values match
   * the indices of the corresponding names in <code>argn</code>.
   *
   * @return <code>PP_TRUE</code> on success or <code>PP_FALSE</code> on
   * failure.
   */
  PP_Bool (*DidCreate)(PP_Instance instance,
                       uint32_t argc,
                       const char* argn[],
                       const char* argv[]);
  /**
   * DidDestroy() is an instance destruction handler. This function is called
   * in many cases (see below) when a module instance is destroyed. It will be
   * called even if DidCreate() returned failure.
   *
   * Generally you will handle this call by deallocating the tracking
   * information and the <code>PP_Instance</code> mapping you created in the
   * DidCreate() call. You can also free resources associated with this
   * instance but this isn't required; all resources associated with the deleted
   * instance will be automatically freed when this function returns.
   *
   * The instance identifier will still be valid during this call, so the module
   * can perform cleanup-related tasks. Once this function returns, the
   * <code>PP_Instance</code> handle will be invalid. This means that you can't
   * do any asynchronous operations like network requests, file writes or
   * messaging from this function since they will be immediately canceled.
   *
   * <strong>Note:</strong> This function will always be skipped on untrusted
   * (Native Client) implementations. This function may be skipped on trusted
   * implementations in certain circumstances when Chrome does "fast shutdown"
   * of a web page. Fast shutdown will happen in some cases when all module
   * instances are being deleted, and no cleanup functions will be called.
   * The module will just be unloaded and the process terminated.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   */
  void (*DidDestroy)(PP_Instance instance);
  /**
   * DidChangeView() is called when the position, the size, of the clip
   * rectangle of the element in the browser that corresponds to this
   * instance has changed.
   *
   * A typical implementation will check the size of the <code>position</code>
   * argument and reallocate the graphics context when a different size is
   * received. Note that this function will be called for scroll events where
   * the size doesn't change, so you should always check that the size is
   * actually different before doing any reallocations.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying the instance
   * that has changed.
   *
   * @param[in] position The location on the page of the instance. This is
   * relative to the top left corner of the viewport, which changes as the
   * page is scrolled. Generally the size of this value will be used to create
   * a graphics device, and the position is ignored (most things are relative
   * to the instance so the absolute position isn't useful in most cases).
   *
   * @param[in] clip The visible region of the instance. This is relative to
   * the top left of the module's coordinate system (not the page).  If the
   * module is invisible, <code>clip</code> will be (0, 0, 0, 0).
   *
   * It's recommended to check for invisible instances and to stop
   * generating graphics updates in this case to save system resources. It's
   * not usually worthwhile, however, to generate partial updates according to
   * the clip when the instance is partially visible. Instead, update the entire
   * region. The time saved doing partial paints is usually not significant and
   * it can create artifacts when scrolling (this notification is sent
   * asynchronously from scrolling so there can be flashes of old content in the
   * exposed regions).
   */
  void (*DidChangeView)(PP_Instance instance,
                        const struct PP_Rect* position,
                        const struct PP_Rect* clip);
  /**
   * DidChangeFocus() is called when an instance has gained or lost focus.
   * Having focus means that keyboard events will be sent to the instance.
   * An instance's default condition is that it will not have focus.
   *
   * <strong>Note:</strong>Clicks on instances will give focus only if you
   * handle the click event. Return <code>true</code> from
   * <code>HandleInputEvent</code> in <code>PPP_InputEvent</code> (or use
   * unfiltered events) to signal that the click event was handled. Otherwise,
   * the browser will bubble the event and give focus to the element on the page
   * that actually did end up consuming it. If you're not getting focus, check
   * to make sure you're returning true from the mouse click in
   * <code>HandleInputEvent</code>.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying the instance
   * receiving the input event.
   *
   * @param[in] has_focus Indicates the new focused state of the instance.
   */
  void (*DidChangeFocus)(PP_Instance instance, PP_Bool has_focus);
  /**
   * HandleDocumentLoad() is called after initialize for a full-frame
   * module that was instantiated based on the MIME type of a DOMWindow
   * navigation. This situation only applies to modules that are pre-registered
   * to handle certain MIME types. If you haven't specifically registered to
   * handle a MIME type or aren't positive this applies to you, your
   * implementation of this function can just return <code>PP_FALSE</code>.
   *
   * The given <code>url_loader</code> corresponds to a
   * <code>PPB_URLLoader</code> instance that is already opened. Its response
   * headers may be queried using <code>PPB_URLLoader::GetResponseInfo</code>.
   * The reference count for the URL loader is not incremented automatically on
   * behalf of the module. You need to increment the reference count yourself
   * if you are going to keep a reference to it.
   *
   * This method returns <code>PP_FALSE</code> if the module cannot handle the
   * data. In response to this method, the module should call
   * ReadResponseBody() to read the incoming data.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying the instance
   * that should do the load.
   *
   * @param[in] url_loader An open <code>PPB_URLLoader</code> instance.
   *
   * @return <code>PP_TRUE</code> if the data was handled,
   * <code>PP_FALSE</code> otherwise.
   */
  PP_Bool (*HandleDocumentLoad)(PP_Instance instance, PP_Resource url_loader);
};
/**
 * @}
 */


typedef struct PPP_Instance PPP_Instance_1_0;

#endif  /* PPAPI_C_PPP_INSTANCE_H_ */

