/*
 IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
 ("Apple") in consideration of your agreement to the following terms, and
 your use, installation, modification or redistribution of this Apple software
 constitutes acceptance of these terms.  If you do not agree with these terms,
 please do not use, install, modify or redistribute this Apple software.

 In consideration of your agreement to abide by the following terms, and
 subject to these terms, Apple grants you a personal, non-exclusive license,
 under Apple's copyrights in this original Apple software
 (the "Apple Software"), to use, reproduce, modify and redistribute the Apple
 Software, with or without modifications, in source and/or binary forms;
 provided that if you redistribute the Apple Software in its entirety and
 without modifications, you must retain this notice and the following text and
 disclaimers in all such redistributions of the Apple Software.  Neither the
 name, trademarks, service marks or logos of Apple Computer, Inc. may be used
 to endorse or promote products derived from the Apple Software without
 specific prior written permission from Apple. Except as expressly stated in
 this notice, no other rights or licenses, express or implied, are granted by
 Apple herein, including but not limited to any patent rights that may be
 infringed by your derivative works or by other works in which the Apple
 Software may be incorporated.

 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
 WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
 COMBINATION WITH YOUR PRODUCTS.

 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION
 AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER
 THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR
 OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <nacl/nacl_npapi.h>
#include <pgl/pgl.h>

#include "native_client/tests/pepper_plugin/plugin_object.h"
#include "native_client/tests/pepper_plugin/event_handler.h"

#ifdef WIN32
#define NPAPI WINAPI
#else
#define NPAPI
#endif

// Plugin entry points
extern "C" {
NPError NPAPI NP_Initialize(NPNetscapeFuncs* browser_funcs,
                            NPPluginFuncs* plugin_funcs);
NPError NPAPI NP_GetEntryPoints(NPPluginFuncs* plugin_funcs);

NPError NPAPI NP_Shutdown() {
  pglTerminate();
  return NPERR_NO_ERROR;
}

NPError NP_GetValue(void* instance, NPPVariable variable, void* value);
char* NP_GetMIMEDescription();
}  // extern "C"

// Plugin entry points

NPError NPAPI NP_Initialize(NPNetscapeFuncs* browser_funcs,
                            NPPluginFuncs* plugin_funcs) {
  browser = browser_funcs;
  pglInitialize();
  return NP_GetEntryPoints(plugin_funcs);
}

// Entrypoints -----------------------------------------------------------------

NPError NPAPI NP_GetEntryPoints(NPPluginFuncs* plugin_funcs) {
  plugin_funcs->version = 11;
  plugin_funcs->size = sizeof(plugin_funcs);
  plugin_funcs->newp = NPP_New;
  plugin_funcs->destroy = NPP_Destroy;
  plugin_funcs->setwindow = NPP_SetWindow;
  plugin_funcs->newstream = NPP_NewStream;
  plugin_funcs->destroystream = NPP_DestroyStream;
  plugin_funcs->asfile = NPP_StreamAsFile;
  plugin_funcs->writeready = NPP_WriteReady;
  plugin_funcs->write = (NPP_WriteUPP)NPP_Write;
  plugin_funcs->print = NPP_Print;
  plugin_funcs->event = NPP_HandleEvent;
  plugin_funcs->urlnotify = NPP_URLNotify;
  plugin_funcs->getvalue = NPP_GetValue;
  plugin_funcs->setvalue = NPP_SetValue;

  return NPERR_NO_ERROR;
}

NPError NPP_New(NPMIMEType pluginType,
                NPP instance,
                uint16 mode,
                int16 argc, char* argn[], char* argv[],
                NPSavedData* saved) {
  if (browser->version >= 14) {
    PluginObject* obj = reinterpret_cast<PluginObject*>(
        browser->createobject(instance, PluginObject::GetPluginClass()));
    instance->pdata = obj;
    event_handler = new EventHandler(instance);
    obj->New(pluginType, argc, argn, argv);
  }

  return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
  if (obj)
    NPN_ReleaseObject(obj->header());

  fflush(stdout);
  return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
  if (obj)
    obj->SetWindow(*window);
  return NPERR_NO_ERROR;
}

NPError NPP_NewStream(NPP instance,
                      NPMIMEType type,
                      NPStream* stream,
                      NPBool seekable,
                      uint16* stype) {
  *stype = NP_ASFILEONLY;
  return NPERR_NO_ERROR;
}

NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason) {
  return NPERR_NO_ERROR;
}

void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname) {
}

int32 NPP_Write(NPP instance,
                NPStream* stream,
                int32 offset,
                int32 len,
                void* buffer) {
  return 0;
}

int32 NPP_WriteReady(NPP instance, NPStream* stream) {
  return 0;
}

void NPP_Print(NPP instance, NPPrint* platformPrint) {
}

int16 NPP_HandleEvent(NPP instance, void* event) {
  return event_handler->handle(event);
}

void NPP_URLNotify(NPP instance,
                   const char* url,
                   NPReason reason,
                   void* notify_data) {
  // PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void* value) {
  NPError err = NPERR_NO_ERROR;

  switch (variable) {
    case NPPVpluginNameString:
      *(reinterpret_cast<const char**>(value)) = "Pepper Test PlugIn";
      break;
    case NPPVpluginDescriptionString:
      *(reinterpret_cast<const char**>(value)) =
          "Simple Pepper plug-in for manual testing.";
      break;
    case NPPVpluginNeedsXEmbed:
      *(reinterpret_cast<NPBool*>(value)) = TRUE;
      break;
    case NPPVpluginScriptableNPObject: {
      void** v = reinterpret_cast<void**>(value);
      PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
      if (NULL == obj) {
        obj = reinterpret_cast<PluginObject*>(
            browser->createobject(instance, PluginObject::GetPluginClass()));
        instance->pdata = reinterpret_cast<void*>(obj);
      } else {
        // Return value is expected to be retained.
        browser->retainobject(reinterpret_cast<NPObject*>(obj));
      }
      *v = obj;
      break;
    }
    default:
      fprintf(stderr, "Unhandled variable to NPP_GetValue\n");
      err = NPERR_GENERIC_ERROR;
      break;
  }

  return err;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void* value) {
  return NPERR_GENERIC_ERROR;
}

NPError NP_GetValue(void* instance, NPPVariable variable, void* value) {
  return NPP_GetValue(reinterpret_cast<NPP>(instance), variable, value);
}

char* NP_GetMIMEDescription() {
  return const_cast<char*>("pepper-application/x-pepper-test-plugin;");
}
