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

#include "webkit/tools/pepper_test_plugin/plugin_object.h"

#ifdef WIN32
#define strcasecmp _stricmp
#define NPAPI WINAPI
#else
#define NPAPI
#endif

static void Log(NPP instance, const char* format, ...) {
  va_list args;
  va_start(args, format);
  char message[2048] = "PLUGIN: ";
  vsprintf(message + strlen(message), format, args);
  va_end(args);

  NPObject* window_object = 0;
  NPError error = browser->getvalue(instance, NPNVWindowNPObject,
                                    &window_object);
  if (error != NPERR_NO_ERROR) {
    fprintf(stderr, "Failed to retrieve window object while logging: %s\n",
            message);
    return;
  }

  NPVariant console_variant;
  if (!browser->getproperty(instance, window_object,
                            browser->getstringidentifier("console"),
                            &console_variant)) {
    fprintf(stderr, "Failed to retrieve console object while logging: %s\n",
            message);
    browser->releaseobject(window_object);
    return;
  }

  NPObject* console_object = NPVARIANT_TO_OBJECT(console_variant);

  NPVariant message_variant;
  STRINGZ_TO_NPVARIANT(message, message_variant);

  NPVariant result;
  if (!browser->invoke(instance, console_object,
                       browser->getstringidentifier("log"),
                       &message_variant, 1, &result)) {
    fprintf(stderr, "Failed to invoke console.log while logging: %s\n",
            message);
    browser->releaseobject(console_object);
    browser->releaseobject(window_object);
    return;
  }

  browser->releasevariantvalue(&result);
  browser->releaseobject(console_object);
  browser->releaseobject(window_object);
}

// Plugin entry points
extern "C" {

NPError NPAPI NP_Initialize(NPNetscapeFuncs* browser_funcs
#if defined(OS_LINUX)
                            , NPPluginFuncs* plugin_funcs
#endif
                            );
NPError NPAPI NP_GetEntryPoints(NPPluginFuncs* plugin_funcs);
void NPAPI NP_Shutdown();

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

void NPAPI NP_Shutdown(void) {
}

static void executeScript(const PluginObject* obj, const char* script);

NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc,
                char* argn[], char* argv[], NPSavedData* saved) {
  if (browser->version >= 14) {
    PluginObject* obj = (PluginObject*)browser->createobject(instance,
                                                             GetPluginClass());

    for (int i = 0; i < argc; i++) {
      if (strcasecmp(argn[i], "onstreamload") == 0 && !obj->onStreamLoad) {
        obj->onStreamLoad = strdup(argv[i]);
      } else if (strcasecmp(argn[i], "onStreamDestroy") == 0 &&
                 !obj->onStreamDestroy) {
        obj->onStreamDestroy = strdup(argv[i]);
      } else if (strcasecmp(argn[i], "onURLNotify") == 0 && !obj->onURLNotify) {
        obj->onURLNotify = strdup(argv[i]);
      } else if (strcasecmp(argn[i], "logfirstsetwindow") == 0) {
        obj->logSetWindow = TRUE;
      } else if (strcasecmp(argn[i], "testnpruntime") == 0) {
        TestNPRuntime(instance);
      } else if (strcasecmp(argn[i], "logSrc") == 0) {
        for (int i = 0; i < argc; i++) {
          if (strcasecmp(argn[i], "src") == 0) {
            Log(instance, "src: %s", argv[i]);
            fflush(stdout);
          }
        }
      } else if (strcasecmp(argn[i], "cleardocumentduringnew") == 0) {
        executeScript(obj, "document.body.innerHTML = ''");
      }
    }

    instance->pdata = obj;
  }

  // On Windows and Unix, plugins only get events if they are windowless.
  return browser->setvalue(instance, NPPVpluginWindowBool, NULL);
}

NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
  if (obj) {
    if (obj->onStreamLoad)
      free(obj->onStreamLoad);

    if (obj->onURLNotify)
      free(obj->onURLNotify);

    if (obj->onStreamDestroy)
      free(obj->onStreamDestroy);

    if (obj->logDestroy)
      Log(instance, "NPP_Destroy");

    browser->releaseobject(&obj->header);
  }

  fflush(stdout);
  return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  PluginObject* obj = static_cast<PluginObject*>(instance->pdata);

  if (obj) {
    if (obj->logSetWindow) {
      Log(instance, "NPP_SetWindow: %d %d", static_cast<int>(window->width),
          static_cast<int>(window->height));
      fflush(stdout);
      obj->logSetWindow = false;
    }
  }

  return NPERR_NO_ERROR;
}

static void executeScript(const PluginObject* obj, const char* script) {
  NPObject* window_script_object;
  browser->getvalue(obj->npp, NPNVWindowNPObject, &window_script_object);

  NPString np_script;
  np_script.UTF8Characters = script;
  np_script.UTF8Length = strlen(script);

  NPVariant browser_result;
  browser->evaluate(obj->npp, window_script_object, &np_script,
                    &browser_result);
  browser->releasevariantvalue(&browser_result);
}

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream,
                      NPBool seekable, uint16* stype) {
  PluginObject* obj = static_cast<PluginObject*>(instance->pdata);

  if (obj->returnErrorFromNewStream)
    return NPERR_GENERIC_ERROR;

  obj->stream = stream;
  *stype = NP_ASFILEONLY;

  if (browser->version >= NPVERS_HAS_RESPONSE_HEADERS)
    NotifyStream(obj, stream->url, stream->headers);

  if (obj->onStreamLoad)
    executeScript(obj, obj->onStreamLoad);

  return NPERR_NO_ERROR;
}

NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason) {
  PluginObject* obj = static_cast<PluginObject*>(instance->pdata);

  if (obj->onStreamDestroy)
    executeScript(obj, obj->onStreamDestroy);

  return NPERR_NO_ERROR;
}

int32 NPP_WriteReady(NPP instance, NPStream* stream) {
  return 0;
}

int32 NPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len,
                void* buffer) {
  return 0;
}

void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname) {
}

void NPP_Print(NPP instance, NPPrint* platformPrint) {
}

int16 NPP_HandleEvent(NPP instance, void* event) {
  return 0;
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason,
                   void* notify_data) {
  PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
  if (obj->onURLNotify)
    executeScript(obj, obj->onURLNotify);

  HandleCallback(obj, url, reason, notify_data);
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
