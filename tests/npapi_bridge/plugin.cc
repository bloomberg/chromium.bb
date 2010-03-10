/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/tests/npapi_bridge/plugin.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <cstring>

NPIdentifier ScriptablePluginObject::id_append_child;
NPIdentifier ScriptablePluginObject::id_bar;
NPIdentifier ScriptablePluginObject::id_body;
NPIdentifier ScriptablePluginObject::id_create_element;
NPIdentifier ScriptablePluginObject::id_create_text_node;
NPIdentifier ScriptablePluginObject::id_cycling;
NPIdentifier ScriptablePluginObject::id_document;
NPIdentifier ScriptablePluginObject::id_fill_rect;
NPIdentifier ScriptablePluginObject::id_fill_style;
NPIdentifier ScriptablePluginObject::id_get_context;
NPIdentifier ScriptablePluginObject::id_get_element_by_id;
NPIdentifier ScriptablePluginObject::id_inner_text;
NPIdentifier ScriptablePluginObject::id_null_method;
NPIdentifier ScriptablePluginObject::id_paint;
NPIdentifier ScriptablePluginObject::id_proxy;
NPIdentifier ScriptablePluginObject::id_set_proxy;
NPIdentifier ScriptablePluginObject::id_text_content;
NPIdentifier ScriptablePluginObject::id_use_proxy;

NPObject* ScriptablePluginObject::window_object;
NPVariant ScriptablePluginObject::canvas_name;
NPVariant ScriptablePluginObject::proxy_canvas_name;

std::map<NPIdentifier, ScriptablePluginObject::Method>*
    ScriptablePluginObject::method_table;

std::map<NPIdentifier, ScriptablePluginObject::Property>*
    ScriptablePluginObject::property_table;

namespace {

static pthread_mutex_t cycling_mu;
static bool cycling_on = false;
static pthread_t cycling_thread;

static void ChangeColor(void* arg) {
  ScriptablePluginObject* obj = reinterpret_cast<ScriptablePluginObject*>(arg);
  NPVariant ret;
  NULL_TO_NPVARIANT(ret);
  if (obj->Invoke(ScriptablePluginObject::id_fill_rect, NULL, 0, &ret)) {
    NPN_ReleaseVariantValue(&ret);
  }
}

static void* ColorCycler(void* arg) {
  ScriptablePluginObject* obj = reinterpret_cast<ScriptablePluginObject*>(arg);
  if (NULL == obj) {
    return NULL;
  }
  NPP instance = obj->npp();

  while (true) {
    bool cycling;

    // Wait
    sleep(1);
    // Check if we are cycling the colors.
    pthread_mutex_lock(&cycling_mu);
    cycling = cycling_on;
    pthread_mutex_unlock(&cycling_mu);
    // Post the callback via NPN_PluginThreadAsyncCall.
    if (cycling) {
      NPN_PluginThreadAsyncCall(instance, ChangeColor, obj);
    }
  }
}

}  // namespace


bool ScriptablePluginObject::InitializeIdentifiers(NPObject* object,
                                                   const char* canvas) {
  window_object = object;

  id_append_child = NPN_GetStringIdentifier("appendChild");
  id_bar = NPN_GetStringIdentifier("bar");
  id_body = NPN_GetStringIdentifier("body");
  id_create_element = NPN_GetStringIdentifier("createElement");
  id_create_text_node = NPN_GetStringIdentifier("createTextNode");
  id_cycling = NPN_GetStringIdentifier("cycling");
  id_document = NPN_GetStringIdentifier("document");
  id_fill_rect = NPN_GetStringIdentifier("fillRect");
  id_fill_style = NPN_GetStringIdentifier("fillStyle");
  id_get_context = NPN_GetStringIdentifier("getContext");
  id_get_element_by_id = NPN_GetStringIdentifier("getElementById");
  id_inner_text = NPN_GetStringIdentifier("innerText");
  id_null_method = NPN_GetStringIdentifier("nullMethod");
  id_paint = NPN_GetStringIdentifier("paint");
  id_proxy = NPN_GetStringIdentifier("proxy");
  id_set_proxy = NPN_GetStringIdentifier("setProxy");
  id_text_content = NPN_GetStringIdentifier("textContent");
  id_use_proxy = NPN_GetStringIdentifier("useProxy");

  STRINGZ_TO_NPVARIANT(canvas, canvas_name);
  STRINGZ_TO_NPVARIANT("proxy_canvas_name", proxy_canvas_name);

  method_table =
    new(std::nothrow) std::map<NPIdentifier, Method>;
  if (method_table == NULL) {
    return false;
  }
  method_table->insert(
    std::pair<NPIdentifier, Method>(id_cycling,
                                    &ScriptablePluginObject::Cycling));
  method_table->insert(
    std::pair<NPIdentifier, Method>(id_fill_rect,
                                    &ScriptablePluginObject::FillRect));
  method_table->insert(
    std::pair<NPIdentifier, Method>(id_set_proxy,
                                    &ScriptablePluginObject::SetProxy));
  method_table->insert(
    std::pair<NPIdentifier, Method>(id_use_proxy,
                                    &ScriptablePluginObject::UseProxy));
  method_table->insert(
    std::pair<NPIdentifier, Method>(id_null_method,
                                    &ScriptablePluginObject::NullMethod));
  method_table->insert(
    std::pair<NPIdentifier, Method>(id_paint,
                                    &ScriptablePluginObject::Paint));

  property_table =
    new(std::nothrow) std::map<NPIdentifier, Property>;
  if (property_table == NULL) {
    return false;
  }
  property_table->insert(
    std::pair<NPIdentifier, Property>(id_bar,
                                      &ScriptablePluginObject::GetBar));

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
                                    const NPVariant* args,
                                    uint32_t arg_count,
                                    NPVariant* result) {
  std::map<NPIdentifier, Method>::iterator i;
  i = method_table->find(name);
  if (i != method_table->end()) {
    return (this->*(i->second))(args, arg_count, result);
  }
  return false;
}

void ScriptablePluginObject::Notify(const char* url,
                                    void* notify_data,
                                    NaClSrpcImcDescType handle) {
  // TODO(sehr): reimplement a test for file download.
  NPP npp = static_cast<NPP>(notify_data);
  if (handle == kNaClSrpcInvalidImcDesc) {
    return;
  }

  NPVariant document;
  VOID_TO_NPVARIANT(document);
  if (!NPN_GetProperty(npp, window_object, id_document, &document) ||
      !NPVARIANT_IS_OBJECT(document)) {
    NPN_ReleaseVariantValue(&document);
    nacl::Close(handle);
    return;
  }

  char buffer[1024];
  int count = read(handle, buffer, sizeof buffer);
  if (0 < count) {
    printf("%.*s", count, buffer);

    NPVariant string;
    STRINGZ_TO_NPVARIANT("div", string);

    NPVariant div;
    VOID_TO_NPVARIANT(div);
    if (NPN_Invoke(npp, NPVARIANT_TO_OBJECT(document), id_create_element,
                   &string, 1, &div) &&
        NPVARIANT_IS_OBJECT(div)) {
      STRINGN_TO_NPVARIANT(buffer, count - 1, string);

      NPVariant text;
      VOID_TO_NPVARIANT(text);
      if (NPN_Invoke(npp,
                     NPVARIANT_TO_OBJECT(document),
                     id_create_text_node,
                     &string,
                     1,
                     &text) &&
          NPVARIANT_IS_OBJECT(text)) {
        NPVariant result;
        VOID_TO_NPVARIANT(result);
        NPN_Invoke(npp,
                   NPVARIANT_TO_OBJECT(div),
                   id_append_child,
                   &text,
                   1,
                   &result);
        NPN_ReleaseVariantValue(&result);
      }
      NPN_ReleaseVariantValue(&text);

      NPVariant body;
      VOID_TO_NPVARIANT(body);
      if (NPN_GetProperty(npp, NPVARIANT_TO_OBJECT(document), id_body, &body) &&
          NPVARIANT_IS_OBJECT(body)) {
        NPVariant result;
        VOID_TO_NPVARIANT(result);
        NPN_Invoke(npp,
                   NPVARIANT_TO_OBJECT(body),
                   id_append_child,
                   &div,
                   1,
                   &result);
        NPN_ReleaseVariantValue(&result);
      }
      NPN_ReleaseVariantValue(&body);
    }
    NPN_ReleaseVariantValue(&div);
  }

  NPN_ReleaseVariantValue(&document);
  nacl::Close(handle);
}

bool ScriptablePluginObject::Cycling(const NPVariant* args,
                                     uint32_t arg_count,
                                     NPVariant* result) {
  if (!NPVARIANT_IS_INT32(args[0])) {
    return false;
  }
  bool cycle = (NPVARIANT_TO_INT32(args[0]) != 0);
  pthread_mutex_lock(&cycling_mu);
  cycling_on = cycle;
  pthread_mutex_unlock(&cycling_mu);
  return true;
}

bool ScriptablePluginObject::GetBar(NPVariant* result) {
  NPObject* object = NaClNPN_CreateArray(npp_);
  if (object) {
    NPVariant value;
    INT32_TO_NPVARIANT(12, value);
    NPN_SetProperty(npp_, object, NPN_GetIntIdentifier(0), &value);
    INT32_TO_NPVARIANT(34, value);
    NPN_SetProperty(npp_, object, NPN_GetIntIdentifier(1), &value);
    OBJECT_TO_NPVARIANT(object, *result);
  } else {
    NULL_TO_NPVARIANT(*result);
  }

  // TODO(sehr): This still needs to be implemented.
  // NaClNPN_OpenURL(npp_, "hello_npapi.txt", npp_, Notify);

  return true;
}

bool ScriptablePluginObject::FillRect(const NPVariant* args,
                                      uint32_t arg_count,
                                      NPVariant* result) {
  NPVariant document;
  VOID_TO_NPVARIANT(document);
  if (!NPN_GetProperty(npp_, window_object, id_document, &document) ||
      !NPVARIANT_IS_OBJECT(document)) {
    NPN_ReleaseVariantValue(&document);
    return false;
  }

  NPVariant canvas;
  VOID_TO_NPVARIANT(canvas);
  if (!NPN_Invoke(npp_,
                  NPVARIANT_TO_OBJECT(document),
                  id_get_element_by_id,
                  &canvas_name,
                  1,
                  &canvas) ||
      !NPVARIANT_IS_OBJECT(canvas)) {
    NPN_ReleaseVariantValue(&canvas);
    NPN_ReleaseVariantValue(&document);
    return false;
  }

  NPVariant string;
  STRINGZ_TO_NPVARIANT("2d", string);

  NPVariant context;
  VOID_TO_NPVARIANT(context);
  if (!NPN_Invoke(npp_,
                  NPVARIANT_TO_OBJECT(canvas),
                  id_get_context,
                  &string,
                  1,
                  &context) ||
      !NPVARIANT_IS_OBJECT(context)) {
    NPN_ReleaseVariantValue(&context);
    NPN_ReleaseVariantValue(&canvas);
    NPN_ReleaseVariantValue(&document);
    return false;
  }

  static int count;
  count = (count + 1) % 3;
  switch (count) {
  case 0:
    STRINGZ_TO_NPVARIANT("rgb(255, 0, 0)", string);
    NPN_Status(npp_, "rgb(255, 0, 0)");
    break;
  case 1:
    STRINGZ_TO_NPVARIANT("rgb(0, 255, 0)", string);
    NPN_Status(npp_, "rgb(0, 255, 0)");
    break;
  case 2:
    STRINGZ_TO_NPVARIANT("rgb(0, 0, 255)", string);
    NPN_Status(npp_, "rgb(0, 0, 255)");
    break;
  }
  NPN_SetProperty(npp_, NPVARIANT_TO_OBJECT(context), id_fill_style, &string);

  NPVariant rect[4];
  DOUBLE_TO_NPVARIANT(0.0, rect[0]);
  DOUBLE_TO_NPVARIANT(0.0, rect[1]);
  DOUBLE_TO_NPVARIANT(150.0, rect[2]);
  DOUBLE_TO_NPVARIANT(150.0, rect[3]);
  if (NPN_Invoke(npp_,
                 NPVARIANT_TO_OBJECT(context),
                 id_fill_rect,
                 rect,
                 4,
                 result)) {
    NPN_ReleaseVariantValue(result);
  }

  NPN_ReleaseVariantValue(&context);
  NPN_ReleaseVariantValue(&canvas);
  NPN_ReleaseVariantValue(&document);

  VOID_TO_NPVARIANT(*result);
  return true;
}

bool ScriptablePluginObject::SetProxy(const NPVariant* args,
                                      uint32_t arg_count,
                                      NPVariant* result) {
  // Update the span that shows the canvas setting for the proxy.
  NPVariant document;
  VOID_TO_NPVARIANT(document);
  if (!NPN_GetProperty(npp_, window_object, id_document, &document) ||
      !NPVARIANT_IS_OBJECT(document)) {
    NPN_ReleaseVariantValue(&document);
    return false;
  }
  NPVariant span;
  VOID_TO_NPVARIANT(span);
  if (!NPN_Invoke(npp_,
                  NPVARIANT_TO_OBJECT(document),
                  id_get_element_by_id,
                  &proxy_canvas_name,
                  1,
                  &span) ||
      !NPVARIANT_IS_OBJECT(span)) {
    NPN_ReleaseVariantValue(&span);
    NPN_ReleaseVariantValue(&document);
    return false;
  }
  if (NPN_HasProperty(npp_, NPVARIANT_TO_OBJECT(span), id_text_content)) {
    NPN_SetProperty(npp_,
                    NPVARIANT_TO_OBJECT(span),
                    id_text_content,
                    &canvas_name);
  } else if (NPN_HasProperty(npp_, NPVARIANT_TO_OBJECT(span), id_inner_text)) {
    NPN_SetProperty(npp_,
                    NPVARIANT_TO_OBJECT(span),
                    id_inner_text,
                    &canvas_name);
  }
  NPN_ReleaseVariantValue(&span);
  NPN_ReleaseVariantValue(&document);

  // set the proxy object.
  NPVariant var;
  OBJECT_TO_NPVARIANT(this, var);
  VOID_TO_NPVARIANT(*result);
  return NPN_SetProperty(npp_, window_object, id_proxy, &var);
}

bool ScriptablePluginObject::UseProxy(const NPVariant* args,
                                      uint32_t arg_count,
                                      NPVariant* result) {
  NPVariant proxy;
  VOID_TO_NPVARIANT(proxy);
  if (NPN_GetProperty(npp_, window_object, id_proxy, &proxy) &&
      NPVARIANT_IS_OBJECT(proxy)) {
    NPVariant result;
    if (NPN_Invoke(npp_,
                   NPVARIANT_TO_OBJECT(proxy),
                   id_fill_rect,
                   NULL,
                   0,
                   &result)) {
      NPN_ReleaseVariantValue(&result);
    }
  }
  NPN_ReleaseVariantValue(&proxy);

  VOID_TO_NPVARIANT(*result);
  return true;
}

bool ScriptablePluginObject::Paint(const NPVariant* args,
                                   uint32_t arg_count,
                                   NPVariant* result) {
  printf("paint called!\n");
  Plugin* plugin = static_cast<Plugin*>(npp_->pdata);
  if (NULL != plugin) {
    VOID_TO_NPVARIANT(*result);
    return plugin->Paint();
  }
  return false;
}

bool ScriptablePluginObject::NullMethod(const NPVariant* args,
                                        uint32_t arg_count,
                                        NPVariant* result) {
  NULL_TO_NPVARIANT(*result);
  return true;
}

Plugin::Plugin(NPP npp, const char* canvas, bool start_cycle_thread)
    : npp_(npp),
      scriptable_object_(NULL),
      window_object_(NULL),
      bitmap_data_(nacl::kMapFailed),
      bitmap_size_(0) {
  NPError error = NPN_GetValue(npp_, NPNVWindowNPObject, &window_object_);
  if (error != NPERR_NO_ERROR) {
    return;
  }

  if (!ScriptablePluginObject::InitializeIdentifiers(window_object_, canvas)) {
    return;
  }

  // Test DOM access.
  NPIdentifier id = NPN_GetStringIdentifier("tar");
  NPVariant var;
  NPVariant ret;
  INT32_TO_NPVARIANT(1, var);
  INT32_TO_NPVARIANT(2, ret);
  if (!NPN_SetProperty(npp_, window_object_, id, &var) ||
      !NPN_GetProperty(npp_, window_object_, id, &ret) ||
      !NPVARIANT_IS_INT32(ret) ||
      NPVARIANT_TO_INT32(ret) != 1) {
    return;
  }

  // Print the document title.
  NPIdentifier id_document = NPN_GetStringIdentifier("document");
  NPVariant document;
  VOID_TO_NPVARIANT(document);
  if (NPN_GetProperty(npp_, window_object_, id_document, &document) &&
      NPVARIANT_IS_OBJECT(document)) {
    NPIdentifier id_title = NPN_GetStringIdentifier("title");
    NPVariant title;
    VOID_TO_NPVARIANT(title);
    if (NPN_GetProperty(npp_, NPVARIANT_TO_OBJECT(document), id_title,
                        &title) &&
        NPVARIANT_IS_STRING(title)) {
      printf("title = '%.*s'\n",
             static_cast<int>(NPVARIANT_TO_STRING(title).UTF8Length),
             NPVARIANT_TO_STRING(title).UTF8Characters);
    }
    NPN_ReleaseVariantValue(&title);
  }
  NPN_ReleaseVariantValue(&document);

  // Color cycling
  // start_cycle_thread = false;
  pthread_mutex_init(&cycling_mu, NULL);
  cycling_on = false;
  if (start_cycle_thread) {
    // IMPORTANT: static_cast actually changes the this pointer here, because
    // ScriptablePluginObject is derived from NPObject but has a vtbl.
    // Hence, a reinterpret_cast here is incorrect.
    ScriptablePluginObject* obj =
        static_cast<ScriptablePluginObject*>(GetScriptableObject());
    pthread_create(&cycling_thread, NULL, ColorCycler, obj);
  }
}

Plugin::~Plugin() {
  if (scriptable_object_) {
    NPN_ReleaseObject(scriptable_object_);
  }
  if (bitmap_data_ != nacl::kMapFailed) {
    nacl::Unmap(bitmap_data_, bitmap_size_);
  }
  if (window_object_) {
    NPN_ReleaseObject(window_object_);
  }
  pthread_mutex_destroy(&cycling_mu);
}

NPObject* Plugin::GetScriptableObject() {
  if (NULL == scriptable_object_) {
    scriptable_object_ =
      NPN_CreateObject(npp_, &ScriptablePluginObject::np_class);
  }

  if (NULL != scriptable_object_) {
    NPN_RetainObject(scriptable_object_);
  }

  return scriptable_object_;
}

NPError Plugin::SetWindow(NPWindow* window) {
  printf("SetWindow %d, %d\n",
         static_cast<int>(window->width),
         static_cast<int>(window->height));
  if (bitmap_data_ == nacl::kMapFailed) {
    // We are using 32-bit ARGB format (4 bytes per pixel).
    bitmap_size_ = 4 * window->width * window->height;
    bitmap_size_ = (bitmap_size_ + nacl::kMapPageSize - 1) &
                   ~(nacl::kMapPageSize - 1);
    bitmap_data_ = nacl::Map(NULL, bitmap_size_,
                             nacl::kProtRead | nacl::kProtWrite,
                             nacl::kMapShared,
                             reinterpret_cast<nacl::HtpHandle>(window->window),
                             0);
  }
  window_ = window;
  return Paint() ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
}

bool Plugin::Paint() {
  static int count = 0;

  if (bitmap_data_ != nacl::kMapFailed) {
    uint32_t* pixel_bits = static_cast<uint32_t*>(bitmap_data_);
    for (unsigned y = 0; y < window_->height; ++y) {
      // Fill the rows with a gradient.
      uint32_t color = ((y + count) % 256);  // Set to blue.
      if (320 <= window_->width) {
        color <<= 8;  // Set to green.
      }
      for (unsigned x = 0; x < window_->width; ++x) {
        pixel_bits[window_->width * y + x] = color;
      }
    }
    count += 1;
    return true;
  }
  return false;
}
