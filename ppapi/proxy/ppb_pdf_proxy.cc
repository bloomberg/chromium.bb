// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_pdf_proxy.h"

#include <string.h>  // For memcpy.

#include <map>

#include "base/linked_ptr.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

class PrivateFontFile : public PluginResource {
 public:
  PrivateFontFile(const HostResource& resource) : PluginResource(resource) {
  }
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
    PP_Instance instance,
    const PP_FontDescription_Dev* description,
    PP_PrivateFontCharset charset) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  SerializedFontDescription desc;
  desc.SetFromPPFontDescription(dispatcher, *description, true);

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBPDF_GetFontFileWithFallback(
      INTERFACE_ID_PPB_PDF, instance, desc, charset, &result));
  if (result.is_null())
    return 0;

  linked_ptr<PrivateFontFile> object(new PrivateFontFile(result));
  return PluginResourceTracker::GetInstance()->AddResource(object);
}

bool GetFontTableForPrivateFontFile(PP_Resource font_file,
                                    uint32_t table,
                                    void* output,
                                    uint32_t* output_length) {
  PrivateFontFile* object = PluginResource::GetAs<PrivateFontFile>(font_file);
  if (!object)
    return false;
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
      object->instance());
  if (!dispatcher)
    return false;

  std::string* contents = object->GetFontTable(table);
  if (!contents) {
    std::string deserialized;
    dispatcher->Send(new PpapiHostMsg_PPBPDF_GetFontTableForPrivateFontFile(
        INTERFACE_ID_PPB_PDF, object->host_resource(), table, &deserialized));
    if (deserialized.empty())
      return false;
    contents = object->AddFontTable(table, deserialized);
  }

  *output_length = static_cast<uint32_t>(contents->size());
  if (output)
    memcpy(output, contents->c_str(), *output_length);
  return true;
}

const PPB_PDF ppb_pdf = {
  NULL,  // &GetLocalizedString,
  NULL,  // &GetResourceImage,
  &GetFontFileWithFallback,
  &GetFontTableForPrivateFontFile,
};

}  // namespace

PPB_PDF_Proxy::PPB_PDF_Proxy(Dispatcher* dispatcher,
                             const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_PDF_Proxy::~PPB_PDF_Proxy() {
}

const void* PPB_PDF_Proxy::GetSourceInterface() const {
  return &ppb_pdf;
}

InterfaceID PPB_PDF_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_PDF;
}

bool PPB_PDF_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_PDF_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBPDF_GetFontFileWithFallback,
                        OnMsgGetFontFileWithFallback)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBPDF_GetFontTableForPrivateFontFile,
                        OnMsgGetFontTableForPrivateFontFile)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw): handle bad messages!
  return handled;
}

void PPB_PDF_Proxy::OnMsgGetFontFileWithFallback(
    PP_Instance instance,
    const SerializedFontDescription& in_desc,
    int32_t charset,
    HostResource* result) {
  PP_FontDescription_Dev desc;
  in_desc.SetToPPFontDescription(dispatcher(), &desc, false);
  result->SetHostResource(instance,
      ppb_pdf_target()->GetFontFileWithFallback(
          instance, &desc, static_cast<PP_PrivateFontCharset>(charset)));
}

void PPB_PDF_Proxy::OnMsgGetFontTableForPrivateFontFile(
    const HostResource& font_file,
    uint32_t table,
    std::string* result) {
  // TODO(brettw): It would be nice not to copy here. At least on Linux,
  // we can map the font file into shared memory and read it that way.
  uint32_t table_length = 0;
  if (!ppb_pdf_target()->GetFontTableForPrivateFontFile(
          font_file.host_resource(), table, NULL, &table_length))
    return;

  result->resize(table_length);
  ppb_pdf_target()->GetFontTableForPrivateFontFile(font_file.host_resource(),
      table, const_cast<char*>(result->c_str()), &table_length);
}

}  // namespace proxy
}  // namespace pp
