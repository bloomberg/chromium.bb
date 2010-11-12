// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_pdf_proxy.h"

#include <string.h>  // For memcpy.

#include <map>

#include "base/linked_ptr.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "webkit/glue/plugins/ppb_private.h"

namespace pp {
namespace proxy {

class PrivateFontFile : public PluginResource {
 public:
  PrivateFontFile() {}
  virtual ~PrivateFontFile() {}

  // Resource overrides.
  virtual PrivateFontFile* AsPrivateFontFile() { return this; }

  // Sees if we have a cache of the font table and returns a pointer to it.
  // Returns NULL if we don't have it.
  std::string* GetFontTable(uint32_t table) const;

  std::string* AddFontTable(uint32_t table, const std::string& contents);

 private:
  typedef std::map<uint32_t, linked_ptr<std::string> > FontTableMap;
  FontTableMap font_tables_;

  DISALLOW_COPY_AND_ASSIGN(PrivateFontFile);
};

std::string* PrivateFontFile::GetFontTable(uint32_t table) const {
  FontTableMap::const_iterator found = font_tables_.find(table);
  if (found == font_tables_.end())
    return NULL;
  return found->second.get();
}

std::string* PrivateFontFile::AddFontTable(uint32_t table,
                                           const std::string& contents) {
  linked_ptr<std::string> heap_string(new std::string(contents));
  font_tables_[table] = heap_string;
  return heap_string.get();
}

namespace {

PP_Resource GetFontFileWithFallback(
    PP_Module module_id,
    const PP_FontDescription_Dev* description,
    PP_PrivateFontCharset charset) {
  SerializedFontDescription desc;
  // TODO(brettw): serialize the description!

  PP_Resource result = 0;
  PluginDispatcher::Get()->Send(
      new PpapiHostMsg_PPBPdf_GetFontFileWithFallback(
          INTERFACE_ID_PPB_PDF, module_id, desc, charset, &result));
  if (!result)
    return 0;

  linked_ptr<PrivateFontFile> object(new PrivateFontFile);
  PluginDispatcher::Get()->plugin_resource_tracker()->AddResource(
      result, object);
  return result;
}

bool GetFontTableForPrivateFontFile(PP_Resource font_file,
                                    uint32_t table,
                                    void* output,
                                    uint32_t* output_length) {
  PrivateFontFile* object = PluginResource::GetAs<PrivateFontFile>(font_file);
  if (!object)
    return false;

  std::string* contents = object->GetFontTable(table);
  if (!contents) {
    std::string deserialized;
    PluginDispatcher::Get()->Send(
        new PpapiHostMsg_PPBPdf_GetFontTableForPrivateFontFile(
            INTERFACE_ID_PPB_PDF, font_file, table, &deserialized));
    if (deserialized.empty())
      return false;
    contents = object->AddFontTable(table, deserialized);
  }

  *output_length = static_cast<uint32_t>(contents->size());
  if (output)
    memcpy(output, contents->c_str(), *output_length);
  return true;
}

const PPB_Private ppb_private = {
  NULL,  // &GetLocalizedString,
  NULL,  // &GetResourceImage,
  &GetFontFileWithFallback,
  &GetFontTableForPrivateFontFile,
};

}  // namespace

PPB_Pdf_Proxy::PPB_Pdf_Proxy(Dispatcher* dispatcher,
                             const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Pdf_Proxy::~PPB_Pdf_Proxy() {
}

const void* PPB_Pdf_Proxy::GetSourceInterface() const {
  return &ppb_private;
}

InterfaceID PPB_Pdf_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_PDF;
}

void PPB_Pdf_Proxy::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PPB_Pdf_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBPdf_GetFontFileWithFallback,
                        OnMsgGetFontFileWithFallback)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBPdf_GetFontTableForPrivateFontFile,
                        OnMsgGetFontTableForPrivateFontFile)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw): handle bad messages!
}

void PPB_Pdf_Proxy::OnMsgGetFontFileWithFallback(
    PP_Module module,
    const SerializedFontDescription& in_desc,
    int32_t charset,
    PP_Resource* result) {
  PP_FontDescription_Dev desc;
  // TODO(brettw) deserialize this value!
  *result = ppb_pdf_target()->GetFontFileWithFallback(module, &desc,
      static_cast<PP_PrivateFontCharset>(charset));
}

void PPB_Pdf_Proxy::OnMsgGetFontTableForPrivateFontFile(PP_Resource font_file,
                                                        uint32_t table,
                                                        std::string* result) {
  // TODO(brettw): It would be nice not to copy here. At least on Linux,
  // we can map the font file into shared memory and read it that way.
  uint32_t table_length = 0;
  if (!ppb_pdf_target()->GetFontTableForPrivateFontFile(
          font_file, table, NULL, &table_length))
    return;

  result->resize(table_length);
  ppb_pdf_target()->GetFontTableForPrivateFontFile(font_file, table,
      const_cast<char*>(result->c_str()), &table_length);
}

}  // namespace proxy
}  // namespace pp
