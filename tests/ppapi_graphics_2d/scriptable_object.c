/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppp_class_deprecated.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_url_loader.h"

#define EXPECT_TRUE(expr) do { \
    if (!(expr)) { \
      printf("%s %i\n", __FUNCTION__, __LINE__); \
      return PP_MakeBool(PP_FALSE); \
    } \
  } while (0)

/* Function prototypes */
static struct PPB_Var_Deprecated* GetPPB_Var();
static struct PPB_Core* GetPPB_Core();
static struct PPB_ImageData* GetPPB_ImageData();
/* Function prototypes of the tests */
static struct PP_Var TestCreate(struct PPB_Graphics2D* ppb_graphics_2d);
static struct PP_Var TestIsGraphics2D(struct PPB_Graphics2D* ppb_graphics_2d);
static struct PP_Var TestDescribe(struct PPB_Graphics2D* ppb_graphics_2d);
static struct PP_Var TestPaintImageData(struct PPB_Graphics2D* ppb_graphics_2d);
static struct PP_Var TestScroll(struct PPB_Graphics2D* ppb_graphics_2d);
static struct PP_Var TestReplaceContents(
    struct PPB_Graphics2D* ppb_graphics_2d);
static struct PP_Var TestFlush(struct PPB_Graphics2D* ppb_graphics_2d);

/* Global variables */
const PP_Bool kAlwaysOpaque = PP_TRUE;
const PP_Instance kInvalidInstance = 0;
const PP_Resource kInvalidResource = 0;
PPB_GetInterface get_browser_interface_func = NULL;
PP_Instance instance = 0;
PP_Module module = 0;

static bool HasProperty(void* object,
                        struct PP_Var name,
                        struct PP_Var* exception) {
  uint32_t len = 0;
  struct PPB_Var_Deprecated* ppb_var_interface = GetPPB_Var();
  const char* property_name = ppb_var_interface->VarToUtf8(name, &len);

  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(exception);

  return (0 == strncmp(property_name, "__moduleReady", len));
}

static bool HasMethod(void* object,
                      struct PP_Var name,
                      struct PP_Var* exception) {
  uint32_t len = 0;
  struct PPB_Var_Deprecated* ppb_var_interface = GetPPB_Var();
  const char* method_name = ppb_var_interface->VarToUtf8(name, &len);
  bool has_method = false;

  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(exception);

  if (0 == strncmp(method_name, "testCreate", len) ||
      0 == strncmp(method_name, "testIsGraphics2D", len) ||
      0 == strncmp(method_name, "testDescribe", len) ||
      0 == strncmp(method_name, "testPaintImageData", len) ||
      0 == strncmp(method_name, "testScroll", len) ||
      0 == strncmp(method_name, "testReplaceContents", len) ||
      0 == strncmp(method_name, "testFlush", len)) {
    has_method = true;
  }
  return has_method;
}

static struct PP_Var GetProperty(void* object,
                                 struct PP_Var name,
                                 struct PP_Var* exception) {
  struct PP_Var var = PP_MakeUndefined();
  uint32_t len = 0;
  struct PPB_Var_Deprecated* ppb_var_interface = GetPPB_Var();
  const char* property_name = ppb_var_interface->VarToUtf8(name, &len);

  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(exception);

  if (0 == strncmp(property_name, "__moduleReady", len)) {
    var = PP_MakeInt32(1);
  }
  return var;
}

static void GetAllPropertyNames(void* object,
                                uint32_t* property_count,
                                struct PP_Var** properties,
                                struct PP_Var* exception) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(properties);
  UNREFERENCED_PARAMETER(exception);

  *property_count = 0;
}

static void SetProperty(void* object,
                        struct PP_Var name,
                        struct PP_Var value,
                        struct PP_Var* exception) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(name);
  UNREFERENCED_PARAMETER(value);
  UNREFERENCED_PARAMETER(exception);
}

static void RemoveProperty(void* object,
                           struct PP_Var name,
                           struct PP_Var* exception) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(name);
  UNREFERENCED_PARAMETER(exception);
}

static struct PP_Var Call(void* object,
                          struct PP_Var method_name,
                          uint32_t argc,
                          struct PP_Var* argv,
                          struct PP_Var* exception) {
  struct PPB_Var_Deprecated* ppb_var_interface = GetPPB_Var();
  uint32_t len;
  const char* method = ppb_var_interface->VarToUtf8(method_name, &len);
  static struct PPB_Graphics2D* ppb_graphics_2d = NULL;

  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(argc);
  UNREFERENCED_PARAMETER(argv);
  UNREFERENCED_PARAMETER(exception);

  if (NULL == ppb_graphics_2d) {
    ppb_graphics_2d = (struct PPB_Graphics2D*)(*get_browser_interface_func)(
        PPB_GRAPHICS_2D_INTERFACE);
  }
  if (NULL == ppb_graphics_2d) {
    printf("%s ppb_graphics_2d is NULL.\n", __FUNCTION__);
    return PP_MakeNull();
  }
  if (0 == strncmp(method, "testCreate", len)) {
    return TestCreate(ppb_graphics_2d);
  } else if (0 == strncmp(method, "testIsGraphics2D", len)) {
    return TestIsGraphics2D(ppb_graphics_2d);
  } else if (0 == strncmp(method, "testDescribe", len)) {
    return TestDescribe(ppb_graphics_2d);
  } else if (0 == strncmp(method, "testPaintImageData", len)) {
    return TestPaintImageData(ppb_graphics_2d);
  } else if (0 == strncmp(method, "testScroll", len)) {
    return TestScroll(ppb_graphics_2d);
  } else if (0 == strncmp(method, "testReplaceContents", len)) {
    return TestReplaceContents(ppb_graphics_2d);
  } else if (0 == strncmp(method, "testFlush", len)) {
    return TestFlush(ppb_graphics_2d);
  }
  return PP_MakeNull();
}

static struct PP_Var Construct(void* object,
                               uint32_t argc,
                               struct PP_Var* argv,
                               struct PP_Var* exception) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(argc);
  UNREFERENCED_PARAMETER(argv);
  UNREFERENCED_PARAMETER(exception);
  return PP_MakeUndefined();
}

static void Deallocate(void* object) {
  UNREFERENCED_PARAMETER(object);
}

struct PP_Var GetScriptableObject(PP_Instance plugin_instance) {
  if (NULL != get_browser_interface_func) {
    struct PPB_Var_Deprecated* ppb_var_interface = GetPPB_Var();
    static struct PPP_Class_Deprecated ppp_class = {
      HasProperty,
      HasMethod,
      GetProperty,
      GetAllPropertyNames,
      SetProperty,
      RemoveProperty,
      Call,
      Construct,
      Deallocate
    };
    instance = plugin_instance;

    return ppb_var_interface->CreateObject(instance, &ppp_class, NULL);
  }
  return PP_MakeNull();
}

static struct PPB_Core* GetPPB_Core() {
  return (struct PPB_Core*)(*get_browser_interface_func)(
      PPB_CORE_INTERFACE);
}

static struct PPB_ImageData* GetPPB_ImageData() {
  return (struct PPB_ImageData*)(*get_browser_interface_func)(
      PPB_IMAGEDATA_INTERFACE);
}

static struct PPB_Var_Deprecated* GetPPB_Var() {
  return (struct PPB_Var_Deprecated*)(*get_browser_interface_func)(
      PPB_VAR_DEPRECATED_INTERFACE);
}

static struct PP_Var TestCreate(struct PPB_Graphics2D* ppb_graphics_2d) {
  struct PPB_Core* ppb_core = GetPPB_Core();
  const struct PP_Size valid_size = PP_MakeSize(100, 100);
  const struct PP_Size invalid_size = PP_MakeSize(0, 0);

  /* Test fail for invalid instance. */
  PP_Resource graphics_2d = ppb_graphics_2d->Create(kInvalidInstance,
                                                    &valid_size, kAlwaysOpaque);
  EXPECT_TRUE(graphics_2d == kInvalidResource);

  graphics_2d = ppb_graphics_2d->Create(kInvalidInstance, &valid_size,
                                        !kAlwaysOpaque);
  EXPECT_TRUE(graphics_2d == kInvalidResource);

  graphics_2d = ppb_graphics_2d->Create(kInvalidInstance, &invalid_size,
                                        kAlwaysOpaque);
  EXPECT_TRUE(graphics_2d == kInvalidResource);

  graphics_2d = ppb_graphics_2d->Create(kInvalidInstance, &invalid_size,
                                        !kAlwaysOpaque);
  EXPECT_TRUE(graphics_2d == kInvalidResource);

  /* Test fail for invalid size. */
  graphics_2d = ppb_graphics_2d->Create(instance, &invalid_size, kAlwaysOpaque);
  EXPECT_TRUE(graphics_2d == kInvalidResource);
  graphics_2d = ppb_graphics_2d->Create(instance, &invalid_size,
                                        !kAlwaysOpaque);
  EXPECT_TRUE(graphics_2d == kInvalidResource);

  /* Test success with instance and valid size */
  graphics_2d = ppb_graphics_2d->Create(instance, &valid_size, kAlwaysOpaque);
  EXPECT_TRUE(graphics_2d != kInvalidResource);
  ppb_core->ReleaseResource(graphics_2d);

  graphics_2d = ppb_graphics_2d->Create(instance, &valid_size, !kAlwaysOpaque);
  EXPECT_TRUE(graphics_2d != kInvalidResource);
  ppb_core->ReleaseResource(graphics_2d);

  return PP_MakeBool(PP_TRUE);
}

struct Graphics2DInfo {
  const struct PP_Size size;
  const PP_Bool always_opaque;
};

static struct PP_Var TestIsGraphics2D(struct PPB_Graphics2D* ppb_graphics_2d) {
  struct PPB_Core* ppb_core = GetPPB_Core();
  const struct PP_Size dummy_size = PP_MakeSize(100, 100);
  struct PPB_URLLoader* ppb_url_loader = NULL;
  PP_Resource url_loader = 0;
  PP_Bool is_graphics_2d = PP_FALSE;

  PP_Resource graphics_2d = ppb_graphics_2d->Create(instance, &dummy_size,
                                                    kAlwaysOpaque);

  /* Test pass with graphics 2d resource. */
  is_graphics_2d = ppb_graphics_2d->IsGraphics2D(graphics_2d);
  ppb_core->ReleaseResource(graphics_2d);
  EXPECT_TRUE(is_graphics_2d == PP_TRUE);
  /* Test fails with released graphics 2d resource. */
  is_graphics_2d = ppb_graphics_2d->IsGraphics2D(graphics_2d);
  EXPECT_TRUE(is_graphics_2d == PP_FALSE);

  /* Test fail with non-graphics 2d resource. */
  ppb_url_loader = (struct PPB_URLLoader*)(*get_browser_interface_func)(
      PPB_URLLOADER_INTERFACE);
  url_loader = ppb_url_loader->Create(instance);
  CHECK(url_loader != kInvalidResource);
  is_graphics_2d = ppb_graphics_2d->IsGraphics2D(url_loader);
  ppb_core->ReleaseResource(url_loader);
  EXPECT_TRUE(is_graphics_2d == PP_FALSE);

  return PP_MakeBool(PP_TRUE);
}

static struct PP_Var TestDescribe(struct PPB_Graphics2D* ppb_graphics_2d) {
  struct PPB_Core* ppb_core = GetPPB_Core();
  PP_Bool is_always_opaque;
  struct PP_Size size;
  const struct PP_Size kSmallSize = PP_MakeSize(100, 100);  /* dummmy values */
  const struct PP_Size kLargerSize = PP_MakeSize(500, 300);  /* dummmy values */
  struct Graphics2DInfo graphics_info[4] = {
    { kSmallSize, kAlwaysOpaque},
    { kSmallSize, !kAlwaysOpaque},
    { kLargerSize, kAlwaysOpaque},
    { kLargerSize, !kAlwaysOpaque},
  };
  const size_t num_graphics_info =
      sizeof(graphics_info) / sizeof(graphics_info[0]);
  size_t j = 0;

  /* Test fail with invalid resource. */
  is_always_opaque = PP_TRUE;
  size = PP_MakeSize(1, 1);  /* set to non-zero value */
  EXPECT_TRUE(ppb_graphics_2d->Describe(0, &size, &is_always_opaque)
              == PP_FALSE);

  /* Test the output parameters are set to 0 */
  EXPECT_TRUE(is_always_opaque == PP_FALSE &&
              size.width == 0 &&
              size.height == 0);

  /* Test success */
  for (j = 0; j < num_graphics_info; ++j) {
    struct Graphics2DInfo* info = &graphics_info[j];
    PP_Resource graphics_2d = ppb_graphics_2d->Create(
        instance,
        &info->size,
        info->always_opaque);
    size = PP_MakeSize(0, 0);
    EXPECT_TRUE(ppb_graphics_2d->Describe(graphics_2d, &size, &is_always_opaque)
                == PP_TRUE);
    EXPECT_TRUE(size.width == info->size.width &&
                size.height == info->size.height &&
                is_always_opaque == info->always_opaque);
    ppb_core->ReleaseResource(graphics_2d);
  }

  return PP_MakeBool(PP_TRUE);
}

void DummyFlushCallback(void* data, int32_t result) {
  UNREFERENCED_PARAMETER(data);
  UNREFERENCED_PARAMETER(result);
}

/*
 * Flush is called in the tests even though there is no way to test for
 * correctness of the PaintImageData call from the untrusted side.  This is done
 * to test for not crashing.
 */
static struct PP_Var TestPaintImageData(
    struct PPB_Graphics2D* ppb_graphics_2d) {
  struct PPB_Core* ppb_core = GetPPB_Core();
  struct PPB_ImageData* ppb_image_data = GetPPB_ImageData();
  const size_t context_width = 30;
  const size_t context_height = 22;
  uint32_t* image_bits = NULL;
  const struct PP_Size graphics_context_size = PP_MakeSize(context_width,
                                                           context_height);
  PP_Resource graphics_2d = ppb_graphics_2d->Create(instance,
                                                    &graphics_context_size,
                                                    false);
  PP_Resource blank_image = 0;
  PP_Resource square_image = 0;
  const struct PP_Size square_image_size =
      PP_MakeSize(10, 8);  /* dummy values */
  const size_t square_dimension = 4;
  const struct PP_Point square_image_offset = PP_MakePoint(1, 2);
  const struct PP_Rect square_image_rect = {
    square_image_offset,
    PP_MakeSize(square_dimension, square_dimension)
  };
  PP_ImageDataFormat image_format;

  const struct PP_Point origin = PP_MakePoint(0, 0);
  const struct PP_Rect full_rect = { origin, graphics_context_size };

  CHECK(ppb_core != NULL);
  CHECK(ppb_image_data != NULL);

  image_format = PP_IMAGEDATAFORMAT_BGRA_PREMUL;
  CHECK(ppb_image_data->IsImageDataFormatSupported(image_format) == PP_TRUE);
  /* Create a blank image to clear the graphics 2d context */
  blank_image = ppb_image_data->Create(instance, image_format,
                                       &graphics_context_size,
                                       PP_TRUE);  /* Initialize bits to zero */

  /* Initialize the square image with a grey square*/
  square_image = ppb_image_data->Create(instance, image_format,
                                        &square_image_size,
                                        PP_TRUE);  /* Initialize bits to zero */
  image_bits = (uint32_t*)ppb_image_data->Map(square_image);
  for (int32_t j = square_image_offset.y; j < square_image_size.height; ++j) {
    int32_t current_row_index =
        square_image_size.width * j + square_image_offset.x;
    uint32_t* current_row_pixels = &image_bits[current_row_index];
    memset(current_row_pixels, 0x8f, square_dimension * sizeof(uint32_t));
  }
  ppb_image_data->Unmap(square_image);

  /* Clear the context */
  ppb_graphics_2d->PaintImageData(graphics_2d, blank_image, &origin,
                                  &full_rect);
  ppb_graphics_2d->Flush(graphics_2d,
                         PP_MakeCompletionCallback(DummyFlushCallback, NULL));

  /* Paint with square image */
  ppb_graphics_2d->PaintImageData(graphics_2d, square_image,
                                  &square_image_offset, &square_image_rect);
  ppb_graphics_2d->Flush(graphics_2d,
                         PP_MakeCompletionCallback(DummyFlushCallback, NULL));

  ppb_core->ReleaseResource(blank_image);
  ppb_core->ReleaseResource(square_image);
  ppb_core->ReleaseResource(graphics_2d);

  return PP_MakeBool(PP_TRUE);
}

/*
 * Flush is called in the tests even though there is no way to test for
 * correctness of the Scroll call from the untrusted side.  This is done
 * to test for not crashing.
 */
static struct PP_Var TestScroll(struct PPB_Graphics2D* ppb_graphics_2d) {
  struct PPB_Core* ppb_core = GetPPB_Core();
  const struct PP_Point scroll_amount = PP_MakePoint(-10, 2);
  const struct PP_Rect clip_rect = PP_MakeRectFromXYWH(2, 5, 4, 7);
  const struct PP_Size context_size = PP_MakeSize(100, 100);
  PP_Resource graphics_2d = ppb_graphics_2d->Create(instance, &context_size,
                                                    PP_TRUE);

  ppb_graphics_2d->Scroll(graphics_2d, &clip_rect, &scroll_amount);
  ppb_graphics_2d->Flush(graphics_2d,
                         PP_MakeCompletionCallback(DummyFlushCallback, NULL));

  ppb_core->ReleaseResource(graphics_2d);

  return PP_MakeBool(PP_TRUE);
}

/*
 * Flush is called in the tests even though there is no way to test for
 * correctness of the ReplaceContents call from the untrusted side.  This is
 * done to test for not crashing.
 */
static struct PP_Var TestReplaceContents(
    struct PPB_Graphics2D* ppb_graphics_2d) {
  struct PPB_Core* ppb_core = GetPPB_Core();
  struct PPB_ImageData* ppb_image_data = GetPPB_ImageData();
  /* Create the 2d graphics context */
  const struct PP_Size context_size = PP_MakeSize(100, 100);
  PP_Resource graphics_2d = ppb_graphics_2d->Create(instance, &context_size,
                                                    PP_TRUE);
  PP_Resource image_data = 0;
  uint32_t* image_bits = NULL;

  CHECK(ppb_image_data);

  /* Create an image with the same dimensions as the 2d graphics context. */
  image_data = ppb_image_data->Create(
      instance,
      PP_IMAGEDATAFORMAT_BGRA_PREMUL,
      &context_size,
      PP_FALSE);  /* Do not initalize bits to zero */
  /* Fill the image w/ non-zero data */
  image_bits = (uint32_t*)ppb_image_data->Map(image_data);
  memset(
      image_bits,
      1,
      context_size.width * context_size.height * 4);  /* 4 color channels for */
                                                      /* bgra format */
  ppb_image_data->Unmap(image_data);

  /* Replace contents and flush */
  ppb_graphics_2d->ReplaceContents(graphics_2d, image_data);
  ppb_graphics_2d->Flush(graphics_2d,
                         PP_MakeCompletionCallback(DummyFlushCallback, NULL));

  ppb_core->ReleaseResource(image_data);
  ppb_core->ReleaseResource(graphics_2d);

  return PP_MakeBool(PP_TRUE);
}

void FlushCompletionCallback(void* data, int32_t result) {
  struct PPB_Instance* ppb_instance =
      (struct PPB_Instance*)(*get_browser_interface_func)(
          PPB_INSTANCE_INTERFACE);
  struct PPB_Var_Deprecated* ppb_var = GetPPB_Var();
  struct PP_Var window = ppb_instance->GetWindowObject(instance);
  struct PP_Var flush_callback = PP_MakeUndefined();
  struct PP_Var exception = PP_MakeUndefined();

  UNREFERENCED_PARAMETER(data);
  UNREFERENCED_PARAMETER(result);

  CHECK(ppb_instance);
  CHECK(window.type == PP_VARTYPE_OBJECT);

  flush_callback = ppb_var->VarFromUtf8(module, "FlushCallback",
                                        strlen("FlushCallback"));
  ppb_var->Call(window, flush_callback, 0, NULL, &exception);
  ppb_var->Release(flush_callback);
  ppb_var->Release(window);
}

static struct PP_Var TestFlush(struct PPB_Graphics2D* ppb_graphics_2d) {
  struct PPB_Core* ppb_core = GetPPB_Core();
  const struct PP_Size context_size = PP_MakeSize(100, 100);
  PP_Resource graphics_2d = ppb_graphics_2d->Create(instance, &context_size,
                                                    PP_TRUE);
  ppb_graphics_2d->Flush(
      graphics_2d,
      PP_MakeCompletionCallback(FlushCompletionCallback, NULL));
  ppb_core->ReleaseResource(graphics_2d);

  return PP_MakeBool(PP_TRUE);
}
