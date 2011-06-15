/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPP_INSTANCE_H_
#define PPAPI_C_PPP_INSTANCE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"

struct PP_InputEvent;
struct PP_Var;

#define PPP_INSTANCE_INTERFACE_0_4 "PPP_Instance;0.4"
#define PPP_INSTANCE_INTERFACE_0_5 "PPP_Instance;0.5"
#ifdef PPAPI_INSTANCE_REMOVE_SCRIPTING
#define PPP_INSTANCE_INTERFACE PPP_INSTANCE_INTERFACE_0_5
#else
#define PPP_INSTANCE_INTERFACE PPP_INSTANCE_INTERFACE_0_4
#endif


/**
 * @file
 * This file defines the PPP_Instance structure - a series of pointers to
 * methods that you must implement in your module.
 */

/** @addtogroup Interfaces
 * @{
 */

/**
 * The PPP_Instance interface contains pointers to a series of functions that
 * you must implement in your module. These functions can be trivial (simply
 * return the default return value) unless you want your module
 * to handle events such as change of focus or input events (keyboard/mouse)
 * events.
 */

#ifdef PPAPI_INSTANCE_REMOVE_SCRIPTING
struct PPP_Instance {
#else
struct PPP_Instance_0_5 {
#endif
  /**
   * Creation handler that is called when a new instance is created. It is
   * called for each instantiation on the page, corresponding to one <embed>
   * tag on the page.
   *
   * Generally you would handle this call by initializing the information
   * your module associates with an instance and creating a mapping from the
   * given PP_Instance handle to this data. The PP_Instance handle will be used
   * in subsequent calls to identify which instance the call pertains to.
   *
   * It's possible for more than one instance to be created in a single module.
   * This means that you may get more than one OnCreate without an OnDestroy
   * in between, and should be prepared to maintain multiple states associated
   * with each instance.
   *
   * If this function reports a failure (by returning @a PP_FALSE), the
   * instance will be deleted.
   *
   * @param[in] instance A new PP_Instance indentifying one instance of a
   * module. This is an opaque handle.
   *
   * @param[in] argc The number of arguments contained in @a argn and @a argv.
   *
   * @param[in] argn An array of argument names.  These argument names are
   * supplied in the <embed> tag, for example:
   * <embed id="nacl_module" dimensions="2"> will produce two argument
   * names: "id" and "dimensions."
   *
   * @param[in] argv An array of argument values.  These are the values of the
   * arguments listed in the <embed> tag, for example
   * <embed id="nacl_module" dimensions="2"> will produce two argument
   * values: "nacl_module" and "2".  The indices of these values match the
   * indices of the corresponding names in @a argn.
   *
   * @return @a PP_TRUE on success or @a PP_FALSE on failure.
   */
  PP_Bool (*DidCreate)(PP_Instance instance,
                       uint32_t argc,
                       const char* argn[],
                       const char* argv[]);

  /**
   * A pointer to a instance destruction handler. This is called in many cases
   * (see below) when a plugin instance is destroyed. It will be called even if
   * DidCreate returned failure.
   *
   * Generally you will handle this call by deallocating the tracking
   * information and the PP_Instance mapping you created in the DidCreate call.
   * You can also free resources associated with this instance but this isn't
   * required; all resources associated with the deleted instance will be
   * automatically freed when this function returns.
   *
   * The instance identifier will still be valid during this call so the plugin
   * can perform cleanup-related tasks. Once this function returns, the
   * PP_Instance handle will be invalid. This means that you can't do any
   * asynchronous operations like network requests or file writes from this
   * function since they will be immediately canceled.
   *
   * <strong>Important note:</strong> This function may be skipped in certain
   * circumstances when Chrome does "fast shutdown". Fast shutdown will happen
   * in some cases when all plugin instances are being deleted, and no cleanup
   * functions will be called. The module will just be unloaded and the process
   * terminated.
   *
   * @param[in] instance A PP_Instance indentifying one instance of a module.
   */
  void (*DidDestroy)(PP_Instance instance);

  /**
   * A pointer to a function that is called when the position, the size, or
   * the clip rectangle of the element in the browser that corresponds to this
   * instance has changed.
   *
   * A typical implementation will check the size of the @a position argument
   * and reallocate the graphics context when a different size is received.
   * Note that this function will be called for scroll events where the size
   * doesn't change, so you should always check that the size is actually
   * different before doing any reallocations.
   *
   * @param[in] instance A PP_Instance indentifying the instance that has
   * changed.
   *
   * @param[in] position The location on the page of the instance. This is
   * relative to the top left corner of the viewport, which changes as the
   * page is scrolled. Generally the size of this value will be used to create
   * a graphics device, and the position is ignored (most things are relative
   * to the instance so the absolute position isn't useful in most cases).
   *
   * @param[in] clip The visible region of the instance. This is relative to
   * the top left of the plugin's coordinate system (not the page).  If the
   * plugin is invisible, @a clip will be (0, 0, 0, 0).
   *
   * It's recommended to check for invisible instances and to stop
   * generating graphics updates in this case to save system resources. It's
   * not usually worthwhile, however, to generate partial updates according to
   * the clip when the instance is partially visible. Instead, update the entire
   * region. The time saved doing partial paints is usually not significant and
   * it can create artifacts when scrolling (this notification is sent
   * asynchronously from scolling so there can be flashes of old content in the
   * exposed regions).
   */
  void (*DidChangeView)(PP_Instance instance,
                        const struct PP_Rect* position,
                        const struct PP_Rect* clip);

  /**
   * A pointer to a function that is called when an instance has gained or
   * lost focus. Having focus means that keyboard events will be
   * sent to the instance. An instance's default condition is that it will
   * not have focus.
   *
   * Note: clicks on instances will give focus only if you handle the
   * click event. Return @a true from HandleInputEvent to signal that the click
   * event was handled. Otherwise the browser will bubble the event and give
   * focus to the element on the page that actually did end up consuming it.
   * If you're not getting focus, check to make sure you're returning true from
   * the mouse click in HandleInputEvent.
   *
   * @param[in] instance A PP_Instance indentifying the instance receiving the
   * input event.
   *
   * @param[in] has_focus Indicates the new focused state of the instance.
   */
  void (*DidChangeFocus)(PP_Instance instance, PP_Bool has_focus);

  /**
   * A pointer to a function to handle input events.  Returns PP_TRUE if the
   * event was handled or PP_FALSE if it was not.
   *
   * If the event was handled, it will not be forwarded to the web page or
   * browser. If it was not handled, it will bubble according to the normal
   * rules. So it is important that a module respond accurately with whether
   * event propagation should continue.
   *
   * Event propagation also controls focus. If you handle an event like a mouse
   * event, typically the instance will be given focus. Returning false means
   * that the click will be given to a lower part of the page and your module
   * will not receive focus. This allows an instance to be partially
   * transparent, where clicks on the transparent areas will behave like clicks
   * to the underlying page.
   *
   * @param[in] instance A PP_Instance indentifying one instance of a module.
   *
   * @param[in] event The input event.
   *
   * @return PP_TRUE if @a event was handled, PP_FALSE otherwise.
   */
  PP_Bool (*HandleInputEvent)(PP_Instance instance,
                              const struct PP_InputEvent* event);

  /**
   * A pointer to a function that is called after initialize for a full-frame
   * plugin that was instantiated based on the MIME type of a DOMWindow
   * navigation. This only applies to modules that are pre-registered to handle
   * certain MIME types. If you haven't specifically registered to handle a
   * MIME type or aren't positive this applies to you, your implementation of
   * this function can just return PP_FALSE.
   *
   * The given url_loader corresponds to a PPB_URLLoader instance that is
   * already opened. Its response headers may be queried using
   * PPB_URLLoader::GetResponseInfo. The url loader is not addrefed on behalf
   * of the module, if you're going to keep a reference to it, you need to
   * addref it yourself.
   *
   * This method returns PP_FALSE if the module cannot handle the data. In
   * response to this method, the module should call ReadResponseBody to read
   * the incoming data.
   *
   * @param[in] instance A PP_Instance indentifying the instance that should
   * do the load.
   *
   * @param[in] url_loader A PP_Resource an open PPB_URLLoader instance.
   *
   * @return PP_TRUE if the data was handled, PP_FALSE otherwise.
   */
  PP_Bool (*HandleDocumentLoad)(PP_Instance instance, PP_Resource url_loader);
};

#ifdef PPAPI_INSTANCE_REMOVE_SCRIPTING
struct PPP_Instance_0_4 {
#else
struct PPP_Instance {
#endif
  PP_Bool (*DidCreate)(PP_Instance instance,
                       uint32_t argc,
                       const char* argn[],
                       const char* argv[]);
  void (*DidDestroy)(PP_Instance instance);
  void (*DidChangeView)(PP_Instance instance,
                        const struct PP_Rect* position,
                        const struct PP_Rect* clip);
  void (*DidChangeFocus)(PP_Instance instance, PP_Bool has_focus);
  PP_Bool (*HandleInputEvent)(PP_Instance instance,
                              const struct PP_InputEvent* event);
  PP_Bool (*HandleDocumentLoad)(PP_Instance instance, PP_Resource url_loader);
  struct PP_Var (*GetInstanceObject)(PP_Instance instance);
};

/**
 * @}
 */

#ifdef PPAPI_INSTANCE_REMOVE_SCRIPTING
typedef struct PPP_Instance PPP_Instance_0_5;
#else
typedef struct PPP_Instance PPP_Instance_0_4;
#endif

#endif  /* PPAPI_C_PPP_INSTANCE_H_ */

