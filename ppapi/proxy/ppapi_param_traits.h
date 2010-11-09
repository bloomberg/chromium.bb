// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPAPI_PARAM_TRAITS_H_
#define PPAPI_PROXY_PPAPI_PARAM_TRAITS_H_

#include "ipc/ipc_message_utils.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/serialized_var.h" // TODO(brettw) eraseme.

struct PP_ObjectProperty;

namespace pp {
namespace proxy {
struct PPBFont_DrawTextAt_Params;
class SerializedFontDescription;
class SerializedVar;
}
}

namespace IPC {

template<>
struct ParamTraits<PP_Bool> {
  typedef PP_Bool param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct ParamTraits<PP_InputEvent> {
  typedef PP_InputEvent param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct ParamTraits<PP_ObjectProperty> {
  typedef PP_ObjectProperty param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct ParamTraits<PP_Point> {
  typedef PP_Point param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct ParamTraits<PP_Rect> {
  typedef PP_Rect param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct ParamTraits<PP_Size> {
  typedef PP_Size param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct ParamTraits<pp::proxy::PPBFont_DrawTextAt_Params> {
  typedef pp::proxy::PPBFont_DrawTextAt_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct ParamTraits<pp::proxy::SerializedFontDescription> {
  typedef pp::proxy::SerializedFontDescription param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct ParamTraits<pp::proxy::SerializedVar> {
  typedef pp::proxy::SerializedVar param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct ParamTraits< std::vector<pp::proxy::SerializedVar> > {
  typedef std::vector<pp::proxy::SerializedVar> param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // PPAPI_PROXY_PPAPI_PARAM_TRAITS_H_
