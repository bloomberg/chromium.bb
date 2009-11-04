/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//////////////////////////////////////////////////////////////
//
// Main plugin entry point implementation
//

#include "third_party/npapi/bindings/npapi.h"
#include "native_client/src/third_party/npapi/files/include/npupp.h"

char* NPP_GetMIMEDescription();

#ifdef XP_MACOSX

extern "C" {
  // Safari under OS X requires the following three entry points to be exported.
  NP_EXPORT(NPError) OSCALL NP_Initialize(NPNetscapeFuncs* pFuncs
#ifdef XP_UNIX
                                          , NPPluginFuncs* pluginFuncs
#endif  // XP_UNIX
                                          );
  NP_EXPORT(NPError) OSCALL NP_GetEntryPoints(NPPluginFuncs* pFuncs);
  NP_EXPORT(NPError) OSCALL NP_Shutdown(void);
#if defined(NACL_STANDALONE)  //  No need for this in Chrome
  // Firefox 2 requires main() to be defined.
  NP_EXPORT(int) main(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs,
                      NPP_ShutdownUPP* unloadUpp);
#endif
}

#endif  // XP_MACOSX

#ifndef HIBYTE
#define HIBYTE(x) ((((uint32_t)(x)) & 0xff00) >> 8)
#endif

NPNetscapeFuncs NPNFuncs;

#if defined(XP_WIN) || defined(XP_MACOSX)

NPError OSCALL NP_GetEntryPoints(NPPluginFuncs* pFuncs)
{
  if (pFuncs == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

#ifdef XP_MACOSX
  // Webkit under OS X passes 0 in pFuncs->size, and Apple's sample code
  // (NetscapeMoviePlugIn) treats this as an output parameter.
  if (pFuncs->size == 0)
    pFuncs->size = sizeof(NPPluginFuncs);
#endif  // XP_MACOSX

  if (pFuncs->size < sizeof(NPPluginFuncs))
    return NPERR_INVALID_FUNCTABLE_ERROR;

  pFuncs->version       = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  pFuncs->newp          = NPP_New;
  pFuncs->destroy       = NPP_Destroy;
  pFuncs->setwindow     = NPP_SetWindow;
  pFuncs->newstream     = NPP_NewStream;
  pFuncs->destroystream = NPP_DestroyStream;
  pFuncs->asfile        = NPP_StreamAsFile;
  pFuncs->writeready    = NPP_WriteReady;
  pFuncs->write         = NPP_Write;
  pFuncs->print         = NPP_Print;
  pFuncs->event         = NPP_HandleEvent;
  pFuncs->urlnotify     = NPP_URLNotify;
  pFuncs->getvalue      = NPP_GetValue;
  pFuncs->setvalue      = NPP_SetValue;
  pFuncs->javaClass     = NULL;

  return NPERR_NO_ERROR;
}

#endif  // defined(XP_WIN) || defined(XP_MACOSX)

char*
NP_GetMIMEDescription()
{
  return NPP_GetMIMEDescription();
}

NPError
NP_GetValue(void* future, NPPVariable variable, void *value)
{
  return NPP_GetValue((NPP_t *)future, variable, value);
}

// Under OS X, we rely on NP_GetEntryPoints() to be called to set up
// the NPPluginFuncs structure, and we do not use the second parameter
// `pluginFuncs` here for the compatibility with Safari under OS X.
NPError OSCALL
NP_Initialize(NPNetscapeFuncs* pFuncs
#ifdef XP_UNIX
              , NPPluginFuncs* pluginFuncs
#endif  // XP_UNIX
              )
{
// TODO(sehr): This code could stand some ifdef refactoring.
#if defined(XP_UNIX) && defined(XP_MACOSX)
#  if !defined(UNREFERENCED_PARAMETER)
#    define UNREFERENCED_PARAMETER(P) do { (void) P; } while (0)
#  endif  // UNREFERENCED_PARAMETER
  UNREFERENCED_PARAMETER(pluginFuncs);
#endif  // defined(XP_UNIX) && defined(XP_MACOSX)
  if(pFuncs == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  if(HIBYTE(pFuncs->version) > NP_VERSION_MAJOR)
    return NPERR_INCOMPATIBLE_VERSION_ERROR;

  NPNFuncs.size                    = pFuncs->size;
  NPNFuncs.version                 = pFuncs->version;
  NPNFuncs.geturlnotify            = pFuncs->geturlnotify;
  NPNFuncs.geturl                  = pFuncs->geturl;
  NPNFuncs.posturlnotify           = pFuncs->posturlnotify;
  NPNFuncs.posturl                 = pFuncs->posturl;
  NPNFuncs.requestread             = pFuncs->requestread;
  NPNFuncs.newstream               = pFuncs->newstream;
  NPNFuncs.write                   = pFuncs->write;
  NPNFuncs.destroystream           = pFuncs->destroystream;
  NPNFuncs.status                  = pFuncs->status;
  NPNFuncs.uagent                  = pFuncs->uagent;
  NPNFuncs.memalloc                = pFuncs->memalloc;
  NPNFuncs.memfree                 = pFuncs->memfree;
  NPNFuncs.memflush                = pFuncs->memflush;
  NPNFuncs.reloadplugins           = pFuncs->reloadplugins;
  NPNFuncs.getJavaEnv              = pFuncs->getJavaEnv;
  NPNFuncs.getJavaPeer             = pFuncs->getJavaPeer;
  NPNFuncs.getvalue                = pFuncs->getvalue;
  NPNFuncs.setvalue                = pFuncs->setvalue;
  NPNFuncs.invalidaterect          = pFuncs->invalidaterect;
  NPNFuncs.invalidateregion        = pFuncs->invalidateregion;
  NPNFuncs.forceredraw             = pFuncs->forceredraw;
  NPNFuncs.getstringidentifier     = pFuncs->getstringidentifier;
  NPNFuncs.getstringidentifiers    = pFuncs->getstringidentifiers;
  NPNFuncs.getintidentifier        = pFuncs->getintidentifier;
  NPNFuncs.identifierisstring      = pFuncs->identifierisstring;
  NPNFuncs.utf8fromidentifier      = pFuncs->utf8fromidentifier;
  NPNFuncs.intfromidentifier       = pFuncs->intfromidentifier;
  NPNFuncs.createobject            = pFuncs->createobject;
  NPNFuncs.retainobject            = pFuncs->retainobject;
  NPNFuncs.releaseobject           = pFuncs->releaseobject;
  NPNFuncs.invoke                  = pFuncs->invoke;
  NPNFuncs.invokeDefault           = pFuncs->invokeDefault;
  NPNFuncs.evaluate                = pFuncs->evaluate;
  NPNFuncs.getproperty             = pFuncs->getproperty;
  NPNFuncs.setproperty             = pFuncs->setproperty;
  NPNFuncs.removeproperty          = pFuncs->removeproperty;
  NPNFuncs.hasproperty             = pFuncs->hasproperty;
  NPNFuncs.hasmethod               = pFuncs->hasmethod;
  NPNFuncs.releasevariantvalue     = pFuncs->releasevariantvalue;
  NPNFuncs.setexception            = pFuncs->setexception;
  // NPNFuncs.pluginthreadasynccall   = pFuncs->pluginthreadasynccall;

#if defined(XP_UNIX) && !defined(XP_MACOSX)
  /*
   * Set up the plugin function table that Netscape will use to
   * call us.  Netscape needs to know about our version and size
   * and have a UniversalProcPointer for every function we
   * implement.
   */
  pluginFuncs->version    = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
  pluginFuncs->size       = sizeof(NPPluginFuncs);
  pluginFuncs->newp       = NewNPP_NewProc(NPP_New);
  pluginFuncs->destroy    = NewNPP_DestroyProc(NPP_Destroy);
  pluginFuncs->setwindow  = NewNPP_SetWindowProc(NPP_SetWindow);
  pluginFuncs->newstream  = NewNPP_NewStreamProc(NPP_NewStream);
  pluginFuncs->destroystream = NewNPP_DestroyStreamProc(NPP_DestroyStream);
  pluginFuncs->asfile     = NewNPP_StreamAsFileProc(NPP_StreamAsFile);
  pluginFuncs->writeready = NewNPP_WriteReadyProc(NPP_WriteReady);
  pluginFuncs->write      = NewNPP_WriteProc(NPP_Write);
  pluginFuncs->print      = NewNPP_PrintProc(NPP_Print);
  pluginFuncs->urlnotify  = NewNPP_URLNotifyProc(NPP_URLNotify);
  pluginFuncs->event      = NewNPP_HandleEventProc(NPP_HandleEvent);
  pluginFuncs->getvalue   = NewNPP_GetValueProc(NPP_GetValue);
#ifdef OJI
  pluginFuncs->javaClass  = NPP_GetJavaClass();
#endif  // OJI
#endif  // defined(XP_UNIX) && !defined(XP_MACOSX)

  return NPP_Initialize();
}

NPError OSCALL NP_Shutdown()
{
  NPP_Shutdown();
  return NPERR_NO_ERROR;
}

#if defined(XP_MACOSX) && defined(NACL_STANDALONE)  // Not required for Chrome

int main(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs, NPP_ShutdownUPP* unloadUpp)
{
  NPError err = NPERR_NO_ERROR;

  //
  // Ensure that everything Netscape passed us is valid!
  //
  if ((nsTable == NULL) || (pluginFuncs == NULL) || (unloadUpp == NULL))
    err = NPERR_INVALID_FUNCTABLE_ERROR;

  //
  // Check the "major" version passed in Netscape's function table.
  // We won't load if the major version is newer than what we expect.
  // Also check that the function tables passed in are big enough for
  // all the functions we need (they could be bigger, if Netscape added
  // new APIs, but that's OK with us -- we'll just ignore them).
  //
  if (err == NPERR_NO_ERROR)
  {
    if (HIBYTE(nsTable->version) > NP_VERSION_MAJOR)
      err = NPERR_INCOMPATIBLE_VERSION_ERROR;
  }

  if (err == NPERR_NO_ERROR)
  {
    err = NP_Initialize(nsTable, NULL);
  }

  if (err == NPERR_NO_ERROR)
  {
    err = NP_GetEntryPoints(pluginFuncs);
    *unloadUpp = NewNPP_ShutdownProc(NP_Shutdown);
  }

  return err;
}

#endif  // XP_MACOSX
