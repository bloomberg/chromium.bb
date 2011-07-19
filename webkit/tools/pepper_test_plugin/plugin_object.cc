/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "webkit/tools/pepper_test_plugin/plugin_object.h"

#include <stdio.h>
#include <cmath>
#include <limits>
#include <string>

#if defined(INDEPENDENT_PLUGIN)
#define CHECK(x)
#else
#include "base/logging.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#endif
#include "webkit/tools/pepper_test_plugin/event_handler.h"
#include "webkit/tools/pepper_test_plugin/test_object.h"

NPNetscapeFuncs* browser;

namespace {

// Properties ------------------------------------------------------------------

enum {
  ID_PROPERTY_PROPERTY = 0,
  ID_PROPERTY_TEST_OBJECT,
  NUM_PROPERTY_IDENTIFIERS
};

static NPIdentifier plugin_property_identifiers[NUM_PROPERTY_IDENTIFIERS];
static const NPUTF8*
    plugin_property_identifier_names[NUM_PROPERTY_IDENTIFIERS] = {
  "property",
  "testObject",
};

// Methods ---------------------------------------------------------------------

enum {
  ID_TEST_GET_PROPERTY = 0,
  ID_SET_TEXT_BOX,
  NUM_METHOD_IDENTIFIERS
};

static NPIdentifier plugin_method_identifiers[NUM_METHOD_IDENTIFIERS];
static const NPUTF8* plugin_method_identifier_names[NUM_METHOD_IDENTIFIERS] = {
  "testGetProperty",
  "setTextBox",
};

void EnsureIdentifiersInitialized() {
  static bool identifiers_initialized = false;
  if (identifiers_initialized)
    return;

  browser->getstringidentifiers(plugin_property_identifier_names,
                                NUM_PROPERTY_IDENTIFIERS,
                                plugin_property_identifiers);
  browser->getstringidentifiers(plugin_method_identifier_names,
                                NUM_METHOD_IDENTIFIERS,
                                plugin_method_identifiers);
  identifiers_initialized = true;
}

// Helper functions ------------------------------------------------------------

std::string CreateStringFromNPVariant(const NPVariant& variant) {
  return std::string(NPVARIANT_TO_STRING(variant).UTF8Characters,
                     NPVARIANT_TO_STRING(variant).UTF8Length);
}

bool TestGetProperty(PluginObject* obj,
                     const NPVariant* args, uint32 arg_count,
                     NPVariant* result) {
  if (arg_count == 0)
    return false;

  NPObject *object;
  browser->getvalue(obj->npp(), NPNVWindowNPObject, &object);

  for (uint32 i = 0; i < arg_count; i++) {
    CHECK(NPVARIANT_IS_STRING(args[i]));
    std::string property_string = CreateStringFromNPVariant(args[i]);
    NPIdentifier property_identifier =
        browser->getstringidentifier(property_string.c_str());

    NPVariant variant;
    bool retval = browser->getproperty(obj->npp(), object, property_identifier,
                                       &variant);
    browser->releaseobject(object);

    if (!retval)
      break;

    if (i + 1 < arg_count) {
      CHECK(NPVARIANT_IS_OBJECT(variant));
      object = NPVARIANT_TO_OBJECT(variant);
    } else {
      *result = variant;
      return true;
    }
  }

  VOID_TO_NPVARIANT(*result);
  return false;
}

// Plugin class functions ------------------------------------------------------

NPObject* PluginAllocate(NPP npp, NPClass* the_class) {
  EnsureIdentifiersInitialized();
  PluginObject* newInstance = new PluginObject(npp);
  return reinterpret_cast<NPObject*>(newInstance);
}

void PluginDeallocate(NPObject* header) {
  PluginObject* plugin = reinterpret_cast<PluginObject*>(header);
  delete plugin;
}

void PluginInvalidate(NPObject* obj) {
}

bool PluginHasMethod(NPObject* obj, NPIdentifier name) {
  for (int i = 0; i < NUM_METHOD_IDENTIFIERS; i++) {
    if (name == plugin_method_identifiers[i])
      return true;
  }
  return false;
}

bool PluginInvoke(NPObject* header,
                  NPIdentifier name,
                  const NPVariant* args, uint32 arg_count,
                  NPVariant* result) {
  PluginObject* plugin = reinterpret_cast<PluginObject*>(header);
  if (name == plugin_method_identifiers[ID_TEST_GET_PROPERTY]) {
    return TestGetProperty(plugin, args, arg_count, result);
  } else if (name == plugin_method_identifiers[ID_SET_TEXT_BOX]) {
    if (1 == arg_count && NPVARIANT_IS_OBJECT(args[0])) {
      return event_handler->set_text_box(NPVARIANT_TO_OBJECT(args[0]));
    }
  }

  return false;
}

bool PluginInvokeDefault(NPObject* obj,
                         const NPVariant* args, uint32 arg_count,
                         NPVariant* result) {
  INT32_TO_NPVARIANT(1, *result);
  return true;
}

bool PluginHasProperty(NPObject* obj, NPIdentifier name) {
  for (int i = 0; i < NUM_PROPERTY_IDENTIFIERS; i++) {
    if (name == plugin_property_identifiers[i])
      return true;
  }
  return false;
}

bool PluginGetProperty(NPObject* obj,
                       NPIdentifier name,
                       NPVariant* result) {
  PluginObject* plugin = reinterpret_cast<PluginObject*>(obj);
  return false;
}

bool PluginSetProperty(NPObject* obj,
                       NPIdentifier name,
                       const NPVariant* variant) {
  PluginObject* plugin = reinterpret_cast<PluginObject*>(obj);
  return false;
}

NPClass plugin_class = {
  NP_CLASS_STRUCT_VERSION,
  PluginAllocate,
  PluginDeallocate,
  PluginInvalidate,
  PluginHasMethod,
  PluginInvoke,
  PluginInvokeDefault,
  PluginHasProperty,
  PluginGetProperty,
  PluginSetProperty,
};

// Bitmap painting -------------------------------------------------------------

#if defined(INDEPENDENT_PLUGIN)
void DrawSampleBitmap(void *region, int width, int height) {
  int32 *bitmap = reinterpret_cast<int32 *>(region);

  for (int h = 0; h < height; ++h) {
    uint8 opacity = (h * 0xff) / height;
    for (int w = 0; w < width; ++w) {
      // kudos to apatrick for noticing we're using premultiplied alpha
      uint8 greenness = (w * opacity) / width;
      *bitmap++ = (opacity << 24) | (greenness << 8);
    }
  }
}
#else
void DrawSampleBitmap(void *region, int width, int height) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  bitmap.setPixels(region);

  SkCanvas canvas(bitmap);
  SkRect rect;
  rect.set(SkIntToScalar(0), SkIntToScalar(0),
           SkIntToScalar(width), SkIntToScalar(height));
  SkPath path;
  path.addOval(rect);

  // Create a gradient shader going from opaque green to transparent.
  SkPaint paint;
  paint.setAntiAlias(true);

  SkColor grad_colors[2];
  grad_colors[0] = SkColorSetARGB(0xFF, 0x00, 0xFF, 0x00);
  grad_colors[1] = SkColorSetARGB(0x00, 0x00, 0xFF, 0x00);
  SkPoint grad_points[2];
  grad_points[0].set(SkIntToScalar(0), SkIntToScalar(0));
  grad_points[1].set(SkIntToScalar(0), SkIntToScalar(height));
  SkSafeUnref(paint.setShader(SkGradientShader::CreateLinear(
      grad_points, grad_colors, NULL, 2, SkShader::kRepeat_TileMode)));

  canvas.drawPath(path, paint);
}
#endif
void FlushCallback(NPP instance, NPDeviceContext* context,
                   NPError err, void* user_data) {
}

NPExtensions* extensions = NULL;


// Sine wave similar to one from simple_sources.cc
// F is the desired frequency (e.g. 200), T is sample type (e.g. int16)
template <int F, typename T> void SineWaveCallback(
    NPDeviceContextAudio *context) {

  const size_t n_samples  = context->config.sampleFrameCount;
  const size_t n_channels = context->config.outputChannelMap;
  const double th = 2 * 3.141592653589 * F / context->config.sampleRate;
  // store time value to avoid clicks on buffer boundries
  intptr_t t = (intptr_t)context->config.userData;

  T* buf = reinterpret_cast<T*>(context->outBuffer);
  for (size_t sample = 0; sample < n_samples; ++sample) {
    T value = static_cast<T>(sin(th * t++) * std::numeric_limits<T>::max());
    for (size_t channel = 0; channel < n_channels; ++channel) {
      *buf++ = value;
    }
  }
  context->config.userData = reinterpret_cast<void *>(t);
}

const int32 kCommandBufferSize = 1024 * 1024;

}  // namespace


// PluginObject ----------------------------------------------------------------

PluginObject::PluginObject(NPP npp)
    : npp_(npp),
      test_object_(browser->createobject(npp, GetTestClass())),
      device2d_(NULL),
      device3d_(NULL),
#if !defined(INDEPENDENT_PLUGIN)
      pgl_context_(NULL),
#endif
      deviceaudio_(NULL) {
  memset(&context_audio_, 0, sizeof(context_audio_));
}

PluginObject::~PluginObject() {
#if !defined(INDEPENDENT_PLUGIN)
  if (pgl_context_)
    Destroy3D();
#endif

  // TODO(kbr): add audio portion of test
#if !defined(OS_MACOSX)
  deviceaudio_->destroyContext(npp_, &context_audio_);
#endif
  // FIXME(brettw) destroy the context.
  browser->releaseobject(test_object_);
}

// static
NPClass* PluginObject::GetPluginClass() {
  return &plugin_class;
}

namespace {
void Draw3DCallback(NPP npp, NPDeviceContext3D* /* context */) {
  static_cast<PluginObject*>(npp->pdata)->Draw3D();
}
}

void PluginObject::New(NPMIMEType pluginType,
                       int16 argc,
                       char* argn[],
                       char* argv[]) {
  // Default to 2D rendering.
  dimensions_ = 2;

  for (int i = 0; i < argc; ++i) {
    if (strcmp(argn[i], "dimensions") == 0)
      dimensions_ = atoi(argv[i]);
  }

  if (!extensions) {
    browser->getvalue(npp_, NPNVPepperExtensions,
                      reinterpret_cast<void*>(&extensions));
    CHECK(extensions);
  }
  device2d_ = extensions->acquireDevice(npp_, NPPepper2DDevice);
  CHECK(device2d_);

  device3d_ = extensions->acquireDevice(npp_, NPPepper3DDevice);
  CHECK(device3d_);

  // TODO(kbr): add audio portion of test
#if !defined(OS_MACOSX)
  deviceaudio_ =  extensions->acquireDevice(npp_, NPPepperAudioDevice);
  CHECK(deviceaudio_);
#endif
}

void PluginObject::SetWindow(const NPWindow& window) {
  width_ = window.width;
  height_ = window.height;

  if (dimensions_ == 2) {
    NPDeviceContext2DConfig config;
    NPDeviceContext2D context;
    device2d_->initializeContext(npp_, &config, &context);

    DrawSampleBitmap(context.region, width_, height_);

    // TODO(brettw) figure out why this cast is necessary, the functions seem to
    // match. Could be a calling convention mismatch?
    NPDeviceFlushContextCallbackPtr callback =
        reinterpret_cast<NPDeviceFlushContextCallbackPtr>(&FlushCallback);
    device2d_->flushContext(npp_, &context, callback, NULL);
  } else {
#if !defined(INDEPENDENT_PLUGIN)
    if (!pgl_context_)
      Initialize3D();
    demo_3d_.SetWindowSize(width_, height_);
#endif
  }

  // TODO(kbr): put back in audio portion of test
#if !defined(OS_MACOSX)
  // testing any field would do
  if (!context_audio_.config.callback) {
    NPDeviceContextAudioConfig cfg;
    cfg.sampleRate       = 44100;
    cfg.sampleType       = NPAudioSampleTypeInt16;
    cfg.outputChannelMap = NPAudioChannelStereo;
    cfg.inputChannelMap  = NPAudioChannelNone;
    cfg.sampleFrameCount = 2048;
    cfg.startThread      = 1;  // Start a thread for the audio producer.
    cfg.flags            = 0;
    cfg.callback         = &SineWaveCallback<200, int16>;
    deviceaudio_->initializeContext(npp_, &cfg, &context_audio_);
  }
#endif
}

void PluginObject::Initialize3D() {
#if !defined(INDEPENDENT_PLUGIN)
  DCHECK(!pgl_context_);

  // Initialize a 3D context.
  NPDeviceContext3DConfig config;
  config.commandBufferSize = kCommandBufferSize;
  device3d_->initializeContext(npp_, &config, &context3d_);
  context3d_.repaintCallback = Draw3DCallback;

  // Create a PGL context.
  pgl_context_ = pglCreateContext(npp_, device3d_, &context3d_);

  // Initialize the demo GL state.
  pglMakeCurrent(pgl_context_);
  demo_3d_.SetWindowSize(width_, height_);
  CHECK(demo_3d_.InitGL());

  pglMakeCurrent(PGL_NO_CONTEXT);
#endif  // INDEPENDENT_PLUGIN
}

void PluginObject::Destroy3D() {
#if !defined(INDEPENDENT_PLUGIN)
  DCHECK(pgl_context_);

  demo_3d_.ReleaseGL();

  // Destroy the PGL context.
  pglDestroyContext(pgl_context_);
  pgl_context_ = NULL;

  // Destroy the Device3D context.
  device3d_->destroyContext(npp_, &context3d_);
#endif  // INDEPENDENT_PLUGIN
}

namespace {
void AsyncFlushCallback(NPP npp,
                        NPDeviceContext* context,
                        NPError error,
                        void* user_data) {
}
}

void PluginObject::Draw3D() {
#if !defined(INDEPENDENT_PLUGIN)
  if (!pglMakeCurrent(pgl_context_) && pglGetError() == PGL_CONTEXT_LOST) {
    Destroy3D();
    Initialize3D();
    pglMakeCurrent(pgl_context_);
  }

  demo_3d_.Draw();
  pglSwapBuffers();
  pglMakeCurrent(PGL_NO_CONTEXT);

  // Async flushes just to see them working.
  context3d_.waitForProgress = true;
  device3d_->flushContext(npp_,
                          &context3d_,
                          AsyncFlushCallback,
                          reinterpret_cast<void*>(0x3D1));
  context3d_.waitForProgress = false;
  device3d_->flushContext(npp_,
                          &context3d_,
                          AsyncFlushCallback,
                          reinterpret_cast<void*>(0x3D2));
#endif  // INDEPENDENT_PLUGIN
}
