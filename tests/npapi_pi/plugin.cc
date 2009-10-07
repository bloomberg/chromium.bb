/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "native_client/tests/npapi_pi/plugin.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <nacl/nacl_imc.h>
#include <nacl/nacl_npapi.h>
#include <nacl/npruntime.h>

NPIdentifier ScriptablePluginObject::id_paint;

std::map<NPIdentifier, ScriptablePluginObject::Method>*
    ScriptablePluginObject::method_table;

std::map<NPIdentifier, ScriptablePluginObject::Property>*
    ScriptablePluginObject::property_table;

bool ScriptablePluginObject::InitializeIdentifiers() {
  id_paint = NPN_GetStringIdentifier("paint");

  method_table =
    new(std::nothrow) std::map<NPIdentifier, Method>;
  if (method_table == NULL) {
    return false;
  }
  method_table->insert(
    std::pair<NPIdentifier, Method>(id_paint,
                                    &ScriptablePluginObject::Paint));

  property_table =
    new(std::nothrow) std::map<NPIdentifier, Property>;
  if (property_table == NULL) {
    return false;
  }

  return true;
}

bool ScriptablePluginObject::HasMethod(NPIdentifier name) {
  std::map<NPIdentifier, Method>::iterator i;
  i = method_table->find(name);
  return i != method_table->end();
}

bool ScriptablePluginObject::HasProperty(NPIdentifier name) {
  std::map<NPIdentifier, Property>::iterator i;
  i = property_table->find(name);
  return i != property_table->end();
}

bool ScriptablePluginObject::GetProperty(NPIdentifier name,
                                         NPVariant *result) {
  VOID_TO_NPVARIANT(*result);

  std::map<NPIdentifier, Property>::iterator i;
  i = property_table->find(name);
  if (i != property_table->end()) {
    return (this->*(i->second))(result);
  }
  return false;
}

bool ScriptablePluginObject::Invoke(NPIdentifier name,
                                    const NPVariant* args, uint32_t arg_count,
                                    NPVariant* result) {
  std::map<NPIdentifier, Method>::iterator i;
  i = method_table->find(name);
  if (i != method_table->end()) {
    return (this->*(i->second))(args, arg_count, result);
  }
  return false;
}

bool ScriptablePluginObject::Paint(const NPVariant* args,
                                   uint32_t arg_count,
                                   NPVariant* result) {
  Plugin* plugin = static_cast<Plugin*>(npp_->pdata);
  if (plugin) {
    DOUBLE_TO_NPVARIANT(plugin->pi(), *result);
    return plugin->Paint();
  }
  return false;
}

Plugin::Plugin(NPP npp)
    : npp_(npp),
      scriptable_object_(NULL),
      bitmap_data_(nacl::kMapFailed),
      bitmap_size_(0),
      quit_(false),
      thread_(0),
      pi_(0.0) {
  ScriptablePluginObject::InitializeIdentifiers();
}

Plugin::~Plugin() {
  quit_ = true;
  if (thread_) {
    pthread_join(thread_, NULL);
  }
  if (scriptable_object_) {
    NPN_ReleaseObject(scriptable_object_);
  }
  if (bitmap_data_ != nacl::kMapFailed) {
    nacl::Unmap(bitmap_data_, bitmap_size_);
  }
}

NPObject* Plugin::GetScriptableObject() {
  if (scriptable_object_ == NULL) {
    scriptable_object_ =
      NPN_CreateObject(npp_, &ScriptablePluginObject::np_class);
  }
  if (scriptable_object_) {
    NPN_RetainObject(scriptable_object_);
  }
  return scriptable_object_;
}

NPError Plugin::SetWindow(NPWindow* window) {
  if (bitmap_data_ == nacl::kMapFailed) {
    // We are using 32-bit ARGB format (4 bytes per pixel).
    bitmap_size_ = 4 * window->width * window->height;
    bitmap_size_ = (bitmap_size_ + nacl::kMapPageSize - 1) &
                   ~(nacl::kMapPageSize - 1);
    bitmap_data_ = nacl::Map(NULL, bitmap_size_,
                             nacl::kProtRead | nacl::kProtWrite,
                             nacl::kMapShared,
                             reinterpret_cast<nacl::Handle>(window->window),
                             0);
    memset(bitmap_data_, 0, bitmap_size_);
    pthread_create(&thread_, NULL, pi, this);
  }
  window_ = window;
  return Paint() ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
}

bool Plugin::Paint() {
  if (bitmap_data_ != nacl::kMapFailed) {
    NPRect rect = { 0, 0, window_->height, window_->width };
    NPN_InvalidateRect(npp_, &rect);
    NPN_ForceRedraw(npp_);
    return true;
  }
  return false;
}

// pi() estimates Pi using Monte Carlo method and it is executed by a separate
// thread created in SetWindow(). pi() puts kMaxPointCount points inside the
// square whose length of each side is 1.0, and calculates the ratio of the
// number of points put inside the inscribed quadrant divided by the total
// number of random points to get Pi/4.
void* Plugin::pi(void* param) {
  const int kMaxPointCount = 1000000000;  // The total number of points to put.
  const uint32_t kRedMask = 0xff0000;
  const uint32_t kBlueMask = 0xff;
  const unsigned kRedShift = 16;
  const unsigned kBlueShift = 0;
  int count = 0;  // The number of points put inside the inscribed quadrant.
  unsigned int seed = 1;
  Plugin* plugin = static_cast<Plugin*>(param);
  uint32_t* pixel_bits = static_cast<uint32_t*>(plugin->bitmap_data_);
  srand(seed);
  for (int i = 1; i <= kMaxPointCount && !plugin->quit(); ++i) {
    double x = static_cast<double>(rand_r(&seed)) / RAND_MAX;
    double y = static_cast<double>(rand_r(&seed)) / RAND_MAX;
    double distance = sqrt(x * x + y * y);
    int px = x * plugin->window_->width;
    int py = (1.0 - y) * plugin->window_->height;
    uint32_t color = pixel_bits[plugin->window_->width * py + px];
    if (distance < 1.0) {
      // Set color to blue.
      ++count;
      plugin->pi_ = 4.0 * count / i;
      color += 4 << kBlueShift;
      color &= kBlueMask;
    } else {
      // Set color to red.
      color += 4 << kRedShift;
      color &= kRedMask;
    }
    pixel_bits[plugin->window_->width * py + px] = color;
  }
  return 0;
}
