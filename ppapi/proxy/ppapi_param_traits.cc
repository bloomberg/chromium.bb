// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_param_traits.h"

#include <string.h>  // For memcpy

#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"

namespace IPC {

// PP_Bool ---------------------------------------------------------------------

// static
void ParamTraits<PP_Bool>::Write(Message* m, const param_type& p) {
  ParamTraits<bool>::Write(m, pp::proxy::PPBoolToBool(p));
}

// static
bool ParamTraits<PP_Bool>::Read(const Message* m, void** iter, param_type* r) {
  // We specifically want to be strict here about what types of input we accept,
  // which ParamTraits<bool> does for us. We don't want to deserialize "2" into
  // a PP_Bool, for example.
  bool result = false;
  if (!ParamTraits<bool>::Read(m, iter, &result))
    return false;
  *r = pp::proxy::BoolToPPBool(result);
  return true;
}

// static
void ParamTraits<PP_Bool>::Log(const param_type& p, std::string* l) {
}

// PP_InputEvent ---------------------------------------------------------------

// static
void ParamTraits<PP_InputEvent>::Write(Message* m, const param_type& p) {
  // PP_InputEvent is just POD so we can just memcpy it.
  m->WriteData(reinterpret_cast<const char*>(&p), sizeof(PP_InputEvent));
}

// static
bool ParamTraits<PP_InputEvent>::Read(const Message* m,
                                      void** iter,
                                      param_type* r) {
  const char* data;
  int data_size;
  if (!m->ReadData(iter, &data, &data_size))
    return false;
  memcpy(r, data, sizeof(PP_InputEvent));
  return true;
}

// static
void ParamTraits<PP_InputEvent>::Log(const param_type& p, std::string* l) {
}

// PP_ObjectProperty -----------------------------------------------------------

// static
void ParamTraits<PP_ObjectProperty>::Write(Message* m, const param_type& p) {
  // FIXME(brettw);
}

// static
bool ParamTraits<PP_ObjectProperty>::Read(const Message* m,
                                          void** iter,
                                          param_type* r) {
  // FIXME(brettw);
  return true;
}

// static
void ParamTraits<PP_ObjectProperty>::Log(const param_type& p, std::string* l) {
}

// PP_Point --------------------------------------------------------------------

// static
void ParamTraits<PP_Point>::Write(Message* m, const param_type& p) {
  m->WriteInt(p.x);
  m->WriteInt(p.y);
}

// static
bool ParamTraits<PP_Point>::Read(const Message* m, void** iter, param_type* r) {
  return m->ReadInt(iter, &r->x) && !m->ReadInt(iter, &r->y);
}

// static
void ParamTraits<PP_Point>::Log(const param_type& p, std::string* l) {
}

// PP_Rect ---------------------------------------------------------------------

// static
void ParamTraits<PP_Rect>::Write(Message* m, const param_type& p) {
  m->WriteInt(p.point.x);
  m->WriteInt(p.point.y);
  m->WriteInt(p.size.width);
  m->WriteInt(p.size.height);
}

// static
bool ParamTraits<PP_Rect>::Read(const Message* m,
                                void** iter,
                                param_type* r) {
  if (!m->ReadInt(iter, &r->point.x) ||
      !m->ReadInt(iter, &r->point.y) ||
      !m->ReadInt(iter, &r->size.width) ||
      !m->ReadInt(iter, &r->size.height))
    return false;
  return true;
}

// static
void ParamTraits<PP_Rect>::Log(const param_type& p, std::string* l) {
}

// PP_Size ---------------------------------------------------------------------

// static
void ParamTraits<PP_Size>::Write(Message* m, const param_type& p) {
  m->WriteInt(p.width);
  m->WriteInt(p.height);
}

// static
bool ParamTraits<PP_Size>::Read(const Message* m, void** iter, param_type* r) {
  return m->ReadInt(iter, &r->width) && m->ReadInt(iter, &r->height);
}

// static
void ParamTraits<PP_Size>::Log(const param_type& p, std::string* l) {
}

// PPBFont_DrawTextAt_Params ---------------------------------------------------

// static
void ParamTraits<pp::proxy::PPBFont_DrawTextAt_Params>::Write(
    Message* m,
    const param_type& p) {
  ParamTraits<PP_Resource>::Write(m, p.font);
  ParamTraits<PP_Resource>::Write(m, p.image_data);
  ParamTraits<PP_Bool>::Write(m, p.text_is_rtl);
  ParamTraits<PP_Bool>::Write(m, p.override_direction);
  ParamTraits<PP_Point>::Write(m, p.position);
  ParamTraits<uint32_t>::Write(m, p.color);
  ParamTraits<PP_Rect>::Write(m, p.clip);
  ParamTraits<bool>::Write(m, p.clip_is_null);
  ParamTraits<PP_Bool>::Write(m, p.image_data_is_opaque);
}

// static
bool ParamTraits<pp::proxy::PPBFont_DrawTextAt_Params>::Read(
    const Message* m,
    void** iter,
    param_type* r) {
  return
      ParamTraits<PP_Resource>::Read(m, iter, &r->font) &&
      ParamTraits<PP_Resource>::Read(m, iter, &r->image_data) &&
      ParamTraits<PP_Bool>::Read(m, iter, &r->text_is_rtl) &&
      ParamTraits<PP_Bool>::Read(m, iter, &r->override_direction) &&
      ParamTraits<PP_Point>::Read(m, iter, &r->position) &&
      ParamTraits<uint32_t>::Read(m, iter, &r->color) &&
      ParamTraits<PP_Rect>::Read(m, iter, &r->clip) &&
      ParamTraits<bool>::Read(m, iter, &r->clip_is_null) &&
      ParamTraits<PP_Bool>::Read(m, iter, &r->image_data_is_opaque);
}

// static
void ParamTraits<pp::proxy::PPBFont_DrawTextAt_Params>::Log(
    const param_type& p,
    std::string* l) {
}

// SerializedVar ---------------------------------------------------------------

// static
void ParamTraits<pp::proxy::SerializedVar>::Write(Message* m,
                                                  const param_type& p) {
  p.WriteToMessage(m);
}

// static
bool ParamTraits<pp::proxy::SerializedVar>::Read(const Message* m,
                                                 void** iter,
                                                 param_type* r) {
  return r->ReadFromMessage(m, iter);
}

// static
void ParamTraits<pp::proxy::SerializedVar>::Log(const param_type& p,
                                                std::string* l) {
}

// pp::proxy::SerializedFontDescription ----------------------------------------

// static
void ParamTraits<pp::proxy::SerializedFontDescription>::Write(
    Message* m,
    const param_type& p) {
  ParamTraits<pp::proxy::SerializedVar>::Write(m, p.face);
  ParamTraits<int32_t>::Write(m, p.family);
  ParamTraits<uint32_t>::Write(m, p.size);
  ParamTraits<int32_t>::Write(m, p.weight);
  ParamTraits<PP_Bool>::Write(m, p.italic);
  ParamTraits<PP_Bool>::Write(m, p.small_caps);
  ParamTraits<int32_t>::Write(m, p.letter_spacing);
  ParamTraits<int32_t>::Write(m, p.word_spacing);
}

// static
bool ParamTraits<pp::proxy::SerializedFontDescription>::Read(
    const Message* m,
    void** iter,
    param_type* r) {
  return
      ParamTraits<pp::proxy::SerializedVar>::Read(m, iter, &r->face) &&
      ParamTraits<int32_t>::Read(m, iter, &r->family) &&
      ParamTraits<uint32_t>::Read(m, iter, &r->size) &&
      ParamTraits<int32_t>::Read(m, iter, &r->weight) &&
      ParamTraits<PP_Bool>::Read(m, iter, &r->italic) &&
      ParamTraits<PP_Bool>::Read(m, iter, &r->small_caps) &&
      ParamTraits<int32_t>::Read(m, iter, &r->letter_spacing) &&
      ParamTraits<int32_t>::Read(m, iter, &r->word_spacing);
}

// static
void ParamTraits<pp::proxy::SerializedFontDescription>::Log(
    const param_type& p,
    std::string* l) {
}

// std::vector<SerializedVar> --------------------------------------------------

void ParamTraits< std::vector<pp::proxy::SerializedVar> >::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, static_cast<int>(p.size()));
  for (size_t i = 0; i < p.size(); i++)
    WriteParam(m, p[i]);
}

// static
bool ParamTraits< std::vector<pp::proxy::SerializedVar> >::Read(
    const Message* m,
    void** iter,
    param_type* r) {
  // This part is just a copy of the the default ParamTraits vector Read().
  int size;
  // ReadLength() checks for < 0 itself.
  if (!m->ReadLength(iter, &size))
    return false;
  // Resizing beforehand is not safe, see BUG 1006367 for details.
  if (INT_MAX / sizeof(pp::proxy::SerializedVar) <= static_cast<size_t>(size))
    return false;

  // The default vector deserializer does resize here and then we deserialize
  // into those allocated slots. However, the implementation of vector (at
  // least in GCC's implementation), creates a new empty object using the
  // default constructor, and then sets the rest of the items to that empty
  // one using the copy constructor.
  //
  // Since we allocate the inner class when you call the default constructor
  // and transfer the inner class when you do operator=, the entire vector
  // will end up referring to the same inner class. Deserializing into this
  // will just end up overwriting the same item over and over, since all the
  // SerializedVars will refer to the same thing.
  //
  // The solution is to make a new SerializedVar for each deserialized item,
  // and then add it to the vector one at a time. Our copies are efficient so
  // this is no big deal.
  r->reserve(size);
  for (int i = 0; i < size; i++) {
    pp::proxy::SerializedVar var;
    if (!ReadParam(m, iter, &var))
      return false;
    r->push_back(var);
  }
  return true;
}

// static
void ParamTraits< std::vector<pp::proxy::SerializedVar> >::Log(
    const param_type& p,
    std::string* l) {
}

}  // namespace IPC
