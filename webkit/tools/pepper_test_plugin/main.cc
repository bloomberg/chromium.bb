/*
 IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. ("Apple") in
 consideration of your agreement to the following terms, and your use, installation,
 modification or redistribution of this Apple software constitutes acceptance of these
 terms.  If you do not agree with these terms, please do not use, install, modify or
 redistribute this Apple software.

 In consideration of your agreement to abide by the following terms, and subject to these
 terms, Apple grants you a personal, non-exclusive license, under Apple’s copyrights in
 this original Apple software (the "Apple Software"), to use, reproduce, modify and
 redistribute the Apple Software, with or without modifications, in source and/or binary
 forms; provided that if you redistribute the Apple Software in its entirety and without
 modifications, you must retain this notice and the following text and disclaimers in all
 such redistributions of the Apple Software.  Neither the name, trademarks, service marks
 or logos of Apple Computer, Inc. may be used to endorse or promote products derived from
 the Apple Software without specific prior written permission from Apple. Except as expressly
 stated in this notice, no other rights or licenses, express or implied, are granted by Apple
 herein, including but not limited to any patent rights that may be infringed by your
 derivative works or by other works in which the Apple Software may be incorporated.

 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES,
 EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS
 USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.

 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
          OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
 REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND
 WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR
 OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "webkit/tools/pepper_test_plugin/plugin_object.h"
#include "webkit/tools/pepper_test_plugin/event_handler.h"

#ifdef WIN32
#define NPAPI WINAPI
#else
#define NPAPI
#endif

namespace {

void Log(NPP instance, const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::string message("PLUGIN: ");
  StringAppendV(&message, format, args);
  va_end(args);

  NPObject* window_object = 0;
  NPError error = browser->getvalue(instance, NPNVWindowNPObject,
                                    &window_object);
  if (error != NPERR_NO_ERROR) {
    LOG(ERROR) << "Failed to retrieve window object while logging: "
               << message;
    return;
  }

  NPVariant console_variant;
  if (!browser->getproperty(instance, window_object,
                            browser->getstringidentifier("console"),
                            &console_variant)) {
    LOG(ERROR) << "Failed to retrieve console object while logging: "
               << message;
    browser->releaseobject(window_object);
    return;
  }

  NPObject* console_object = NPVARIANT_TO_OBJECT(console_variant);

  NPVariant message_variant;
  STRINGZ_TO_NPVARIANT(message.c_str(), message_variant);

  NPVariant result;
  if (!browser->invoke(instance, console_object,
                       browser->getstringidentifier("log"),
                       &message_variant, 1, &result)) {
    fprintf(stderr, "Failed to invoke console.log while logging: %s\n",
            message.c_str());
    browser->releaseobject(console_object);
    browser->releaseobject(window_object);
    return;
  }

  browser->releasevariantvalue(&result);
  browser->releaseobject(console_object);
  browser->releaseobject(window_object);
}

}  // namespace

// Plugin entry points
extern "C" {

#if defined(OS_WIN)
//__declspec(dllexport)
#endif
NPError NPAPI NP_Initialize(NPNetscapeFuncs* browser_funcs
#if defined(OS_LINUX)
                            , NPPluginFuncs* plugin_funcs
#endif
                            );
#if defined(OS_WIN)
//__declspec(dllexport)
#endif
NPError NPAPI NP_GetEntryPoints(NPPluginFuncs* plugin_funcs);

#if defined(OS_WIN)
//__declspec(dllexport)
#endif
void NPAPI NP_Shutdown() {
}

#if defined(OS_LINUX)
NPError NP_GetValue(NPP instance, NPPVariable variable, void* value);
const char* NP_GetMIMEDescription();
#endif

}  // extern "C"

// Plugin entry points
NPError NPAPI NP_Initialize(NPNetscapeFuncs* browser_funcs
#if defined(OS_LINUX)
                            , NPPluginFuncs* plugin_funcs
#endif
                            ) {
  browser = browser_funcs;
#if defined(OS_LINUX)
  return NP_GetEntryPoints(plugin_funcs);
#else
  return NPERR_NO_ERROR;
#endif
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
  plugin_funcs->write = (NPP_WriteProcPtr)NPP_Write;
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
    browser->releaseobject(obj->header());

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

void NPP_URLNotify(NPP instance, const char* url, NPReason reason,
                   void* notify_data) {
  PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void* value) {
  NPError err = NPERR_NO_ERROR;

  switch (variable) {
#if defined(OS_LINUX)
    case NPPVpluginNameString:
      *((const char**)value) = "Pepper Test PlugIn";
      break;
    case NPPVpluginDescriptionString:
      *((const char**)value) = "Simple Pepper plug-in for manual testing.";
      break;
    case NPPVpluginNeedsXEmbed:
      *((NPBool*)value) = TRUE;
      break;
#endif
    case NPPVpluginScriptableNPObject: {
      void** v = (void**)value;
      PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
      // Return value is expected to be retained
      browser->retainobject((NPObject*)obj);
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

#if defined(OS_LINUX)
NPError NP_GetValue(NPP instance, NPPVariable variable, void* value) {
  return NPP_GetValue(instance, variable, value);
}

const char* NP_GetMIMEDescription() {
  return "pepper-application/x-pepper-test-plugin pepper test;";
}
#endif
