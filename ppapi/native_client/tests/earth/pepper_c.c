/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NaCl Earth demo */
/* Pepper code in C */

#include "native_client/tests/earth/earth.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

/* Pepper includes */
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"

#define NUMBER_OF_IMAGES 2

PPB_GetInterface g_get_browser_interface = NULL;

/* NOTE on PP_Instance: In general Pepper is designed such that a
 * single plugin process can implement multiple plugin instances.
 * This might occur, for example, if a plugin were instantiated by
 * multiple <embed ...> tags in a single page.
 *
 * This implementation assumes at most one instance per plugin,
 * consistent with limitations of the current implementation of
 * Native Client.
 */
struct PepperState {
  const struct PPB_Core* core_interface;
  const struct PPB_Graphics2D* graphics_2d_interface;
  const struct PPB_ImageData* image_data_interface;
  const struct PPB_Instance* instance_interface;
  PP_Resource device_context;
  int32_t which_image;
  PP_Resource image[NUMBER_OF_IMAGES];
  uint32_t* image_data[NUMBER_OF_IMAGES];
  PP_Instance instance;
  struct PP_Rect position;
  bool ready;
};
struct PepperState g_MyState;
bool g_MyStateIsValid = false;

static void Repaint(struct PepperState *mystate);
static void FlushCompletionCallback(void* user_data, int32_t result) {
  Repaint((struct PepperState*)user_data);
}

static void Repaint(struct PepperState *mystate) {
  struct PP_Point topleft = PP_MakePoint(0, 0);
  struct PP_Rect rect = PP_MakeRectFromXYWH(0, 0,
                                            mystate->position.size.width,
                                            mystate->position.size.height);
  int show, render;

  /* Wait for previous rendering (if applicable) to finish */
  Earth_Sync();

  /* Double buffer - show previously rendered image. */
  show = mystate->which_image;
  mystate->graphics_2d_interface->PaintImageData(mystate->device_context,
                                                 mystate->image[show],
                                                 &topleft, &rect);
  int32_t ret;
  ret = mystate->graphics_2d_interface->Flush(mystate->device_context,
      PP_MakeCompletionCallback(&FlushCompletionCallback, mystate));

  /* Start Rendering into the other image while presenting. */
  render = (mystate->which_image + 1) % NUMBER_OF_IMAGES;

  Earth_Draw(mystate->image_data[render],
             mystate->position.size.width,
             mystate->position.size.height);

  /* In next callback, show what was rendered. */
  mystate->which_image = render;
}

static PP_Bool Instance_DidCreate(PP_Instance instance,
                                  uint32_t argc,
                                  const char* argn[],
                                  const char* argv[]) {
  assert(g_MyStateIsValid == false);

  DebugPrintf("Creating instance %x\n", instance);
  g_MyState.instance = instance;
  g_MyState.ready = false;
  g_MyStateIsValid = true;

  Earth_Init(argc, argn, argv);
  return PP_TRUE;
}

static void Instance_DidDestroy(PP_Instance instance) {
  assert(g_MyState.instance == instance && g_MyStateIsValid == true);
  g_MyStateIsValid = false;
}

/* Returns a refed resource corresponding to the created device context. */
static PP_Resource MakeAndBindDeviceContext(PP_Instance instance,
                                            const struct PP_Size* size) {
  PP_Resource device_context;

  device_context =
    g_MyState.graphics_2d_interface->Create(instance, size, PP_FALSE);
  if (!device_context) return 0;

  if (!g_MyState.instance_interface->BindGraphics(instance, device_context)) {
    g_MyState.core_interface->ReleaseResource(device_context);
    return 0;
  }
  return device_context;
}

static void Instance_DidChangeView(PP_Instance pp_instance,
                                   const struct PP_Rect* position,
                                   const struct PP_Rect* clip) {
  DebugPrintf("DidChangeView(%x)\n", pp_instance);
  assert(g_MyStateIsValid == true);
  assert(g_MyState.instance == pp_instance);

  g_MyState.position = *position;
  if (g_MyState.ready == false) {
    g_MyState.device_context =
      MakeAndBindDeviceContext(pp_instance, &position->size);
    /* create device context */
    if (!g_MyState.device_context) {
      DebugPrintf("device_context is null!\n");
      return;
    }
    /*
     * Create double-buffered image data.
     * Note: This example does not use transparent pixels. All pixels are
     * written into the framebuffer with alpha set to 255 (opaque)
     * Note: Pepper uses premultiplied alpha.
     */
    g_MyState.which_image = 0;
    for (int i = 0; i < NUMBER_OF_IMAGES; ++i) {
      g_MyState.image[i] =
        g_MyState.image_data_interface->Create(pp_instance,
          PP_IMAGEDATAFORMAT_BGRA_PREMUL, &position->size, PP_TRUE);
      if (!g_MyState.image[i]) {
        DebugPrintf("image resource is invalid!\n");
        return;
      }
      g_MyState.image_data[i] =
        (uint32_t*)g_MyState.image_data_interface->Map(g_MyState.image[i]);
      if (!g_MyState.image_data[i]) {
        DebugPrintf("could not allocate image_data\n");
        return;
      }
      size_t size_in_bytes = position->size.width * position->size.height *
                             sizeof(uint32_t);
      memset(g_MyState.image_data[i], 0, size_in_bytes);
    }
    g_MyState.ready = true;
    Repaint(&g_MyState);
  }
}

static void Instance_DidChangeFocus(PP_Instance pp_instance,
                                    PP_Bool has_focus) {
}

static PP_Bool Instance_HandleDocumentLoad(PP_Instance pp_instance,
                                           PP_Resource pp_url_loader) {
  return PP_FALSE;
}

static struct PPP_Instance instance_interface = {
  &Instance_DidCreate,
  &Instance_DidDestroy,
  &Instance_DidChangeView,
  &Instance_DidChangeFocus,
  &Instance_HandleDocumentLoad
};


/* Global entrypoints --------------------------------------------------------*/

PP_EXPORT int32_t PPP_InitializeModule(PP_Module module,
                                       PPB_GetInterface get_browser_interface) {
  g_get_browser_interface = get_browser_interface;

  g_MyState.core_interface = (const struct PPB_Core*)
    get_browser_interface(PPB_CORE_INTERFACE);
  g_MyState.instance_interface = (const struct PPB_Instance*)
    get_browser_interface(PPB_INSTANCE_INTERFACE);
  g_MyState.image_data_interface = (const struct PPB_ImageData*)
    get_browser_interface(PPB_IMAGEDATA_INTERFACE);
  g_MyState.graphics_2d_interface = (const struct PPB_Graphics2D*)
    get_browser_interface(PPB_GRAPHICS_2D_INTERFACE);
  if (!g_MyState.core_interface ||
      !g_MyState.instance_interface ||
      !g_MyState.image_data_interface ||
      !g_MyState.graphics_2d_interface)
    return -1;

  return PP_OK;
}

PP_EXPORT void PPP_ShutdownModule() {
}

PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0)
    return &instance_interface;
  return NULL;
}
