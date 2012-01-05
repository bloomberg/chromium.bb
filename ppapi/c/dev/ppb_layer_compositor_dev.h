/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPB_LAYER_COMPOSITOR_DEV_H_
#define PPAPI_C_DEV_PPB_LAYER_COMPOSITOR_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"

#define PPB_LAYER_COMPOSITOR_DEV_INTERFACE_0_2 "PPB_LayerCompositor(Dev);0.2"
#define PPB_LAYER_COMPOSITOR_DEV_INTERFACE \
    PPB_LAYER_COMPOSITOR_DEV_INTERFACE_0_2

// PPB_LayerCompositor allows multiple layers of PPB_Surface3D and
// PPB_VideoLayer be bound to a plugin instance.
//
// Use PPB_Instance's BindGraphics() to bind a PPB_LayerCompositor to the
// instance.
//
// This also allows each layer to be updated seperately to avoid excessive
// compositing.
struct PPB_LayerCompositor_Dev_0_2 {
  // Creates a video layer.
  PP_Resource (*Create)(PP_Instance instance);

  // Returns PP_TRUE if the input parameter is a PPB_LayerCompositor_Dev object.
  PP_Bool (*IsLayerCompositor)(PP_Resource resource);

  // Add a layer to be composited. Return PP_TRUE if |layer| is accepted.
  //
  // Note that after a PPB_Surface3D is added to this compositor, calling to
  // the layer's SwapBuffer() method will have no effect.
  PP_Bool (*AddLayer)(PP_Resource compositor, PP_Resource layer);

  // Remove a layer from the compositor.
  void (*RemoveLayer)(PP_Resource compositor, PP_Resource layer);

  // Assign the z-index for |layer| for z-ordering. A layer with greater
  // number of z-index will be placed on top.
  void (*SetZIndex)(PP_Resource compositor, PP_Resource layer, int32_t index);

  // Set the target rectangle fo the layer relative to the plugin rectangle.
  // The layer must have already been added to the compositor.
  //
  // The target rectangle will be effective after SwapBuffers() is called.
  void (*SetRect)(PP_Resource compositor, PP_Resource layer,
                  const struct PP_Rect* rect);

  // Set if this video layer is displayed. By default each layer is displayed.
  //
  // This setting will be effective after SwapBuffers() is called.
  void (*SetDisplay)(PP_Resource compositor, PP_Resource layer,
                     PP_Bool is_displayed);

  // If the content of |layer| has been updated this method must be called so
  // that the compositor is aware of the change.
  //
  // SwapBuffers() must be called afterwards to make changes available for
  // display.
  void (*MarkAsDirty)(PP_Resource compositor, PP_Resource layer);

  // Changes to z-index, rectangles will be submitted for display. Also for
  // each PPB_Surface3D and PPB_VideoLayer marked as dirty will have the
  // updates made available for compositing.
  //
  // Since this is an asynchronous operation, |callback| will be called when
  // this operation is done.
  //
  // Returns an error code from pp_errors.h.
  int32_t (*SwapBuffers)(PP_Resource compositor,
                         struct PP_CompletionCallback callback);
};

typedef struct PPB_LayerCompositor_Dev_0_2 PPB_LayerCompositor_Dev;

#endif  /* PPAPI_C_DEV_PPB_LAYER_COMPOSITOR_DEV_H_ */
