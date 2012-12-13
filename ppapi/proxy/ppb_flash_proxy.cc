// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_proxy.h"

#include <math.h>

#include <limits>

#include "base/containers/mru_cache.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "build/build_config.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_flash_print.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/proxy_module.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_url_request_info_api.h"
#include "ppapi/thunk/resource_creation_api.h"

using ppapi::thunk::EnterInstanceNoLock;
using ppapi::thunk::EnterResourceNoLock;

namespace ppapi {
namespace proxy {

namespace {

struct LocalTimeZoneOffsetEntry {
  base::TimeTicks expiration;
  double offset;
};

class LocalTimeZoneOffsetCache
    : public base::MRUCache<PP_Time, LocalTimeZoneOffsetEntry> {
 public:
  LocalTimeZoneOffsetCache()
      : base::MRUCache<PP_Time, LocalTimeZoneOffsetEntry>(kCacheSize) {}
 private:
  static const size_t kCacheSize = 100;
};

base::LazyInstance<LocalTimeZoneOffsetCache>::Leaky
    g_local_time_zone_offset_cache = LAZY_INSTANCE_INITIALIZER;

void InvokePrinting(PP_Instance instance) {
  ProxyAutoLock lock;

  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (dispatcher) {
    dispatcher->Send(new PpapiHostMsg_PPBFlash_InvokePrinting(
        API_ID_PPB_FLASH, instance));
  }
}

const PPB_Flash_Print_1_0 g_flash_print_interface = {
  &InvokePrinting
};

}  // namespace

// -----------------------------------------------------------------------------

PPB_Flash_Proxy::PPB_Flash_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_Flash_Proxy::~PPB_Flash_Proxy() {
}

// static
const PPB_Flash_Print_1_0* PPB_Flash_Proxy::GetFlashPrintInterface() {
  return &g_flash_print_interface;
}

bool PPB_Flash_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_FLASH))
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_GetLocalTimeZoneOffset,
                        OnHostMsgGetLocalTimeZoneOffset)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_InvokePrinting,
                        OnHostMsgInvokePrinting)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_GetSetting,
                        OnHostMsgGetSetting)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

double PPB_Flash_Proxy::GetLocalTimeZoneOffset(PP_Instance instance,
                                               PP_Time t) {
  LocalTimeZoneOffsetCache& cache = g_local_time_zone_offset_cache.Get();

  // Get the minimum PP_Time value that shares the same minute as |t|.
  // Use cached offset if cache hasn't expired and |t| is in the same minute as
  // the time for the cached offset (assume offsets change on minute
  // boundaries).
  PP_Time t_minute_base = floor(t / 60.0) * 60.0;
  LocalTimeZoneOffsetCache::iterator iter = cache.Get(t_minute_base);
  base::TimeTicks now = base::TimeTicks::Now();
  if (iter != cache.end() && now < iter->second.expiration)
    return iter->second.offset;

  // Cache the local offset for ten seconds, since it's slow on XP and Linux.
  // Note that TimeTicks does not continue counting across sleep/resume on all
  // platforms. This may be acceptable for 10 seconds, but if in the future this
  // is changed to one minute or more, then we should consider using base::Time.
  const int64 kMaxCachedLocalOffsetAgeInSeconds = 10;
  base::TimeDelta expiration_delta =
      base::TimeDelta::FromSeconds(kMaxCachedLocalOffsetAgeInSeconds);

  LocalTimeZoneOffsetEntry cache_entry;
  cache_entry.expiration = now + expiration_delta;
  cache_entry.offset = 0.0;

  // TODO(shess): Figure out why OSX needs the access, the sandbox
  // warmup should handle it.  http://crbug.com/149006
#if defined(OS_LINUX) || defined(OS_MACOSX)
  // On Linux localtime needs access to the filesystem, which is prohibited
  // by the sandbox. It would be better to go directly to the browser process
  // for this message rather than proxy it through some instance in a renderer.
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_GetLocalTimeZoneOffset(
      API_ID_PPB_FLASH, instance, t, &cache_entry.offset));
#else
  base::Time cur = PPTimeToTime(t);
  base::Time::Exploded exploded = { 0 };
  base::Time::Exploded utc_exploded = { 0 };
  cur.LocalExplode(&exploded);
  cur.UTCExplode(&utc_exploded);
  if (exploded.HasValidValues() && utc_exploded.HasValidValues()) {
    base::Time adj_time = base::Time::FromUTCExploded(exploded);
    base::Time cur = base::Time::FromUTCExploded(utc_exploded);
    cache_entry.offset = (adj_time - cur).InSecondsF();
  }
#endif

  cache.Put(t_minute_base, cache_entry);
  return cache_entry.offset;
}

PP_Var PPB_Flash_Proxy::GetSetting(PP_Instance instance,
                                   PP_FlashSetting setting) {
  PluginDispatcher* plugin_dispatcher =
      static_cast<PluginDispatcher*>(dispatcher());
  switch (setting) {
    case PP_FLASHSETTING_3DENABLED:
      return PP_MakeBool(PP_FromBool(
          plugin_dispatcher->preferences().is_3d_supported));
    case PP_FLASHSETTING_INCOGNITO:
      return PP_MakeBool(PP_FromBool(plugin_dispatcher->incognito()));
    case PP_FLASHSETTING_STAGE3DENABLED:
      return PP_MakeBool(PP_FromBool(
          plugin_dispatcher->preferences().is_stage3d_supported));
    case PP_FLASHSETTING_LANGUAGE:
      return StringVar::StringToPPVar(
          PluginGlobals::Get()->GetUILanguage());
    case PP_FLASHSETTING_NUMCORES:
      return PP_MakeInt32(plugin_dispatcher->preferences().number_of_cpu_cores);
    case PP_FLASHSETTING_LSORESTRICTIONS: {
      ReceiveSerializedVarReturnValue result;
      dispatcher()->Send(new PpapiHostMsg_PPBFlash_GetSetting(
          API_ID_PPB_FLASH, instance, setting, &result));
      return result.Return(dispatcher());
    }
  }
  return PP_MakeUndefined();
}

void PPB_Flash_Proxy::OnHostMsgGetLocalTimeZoneOffset(PP_Instance instance,
                                                      PP_Time t,
                                                      double* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    *result = enter.functions()->GetFlashAPI()->GetLocalTimeZoneOffset(
        instance, t);
  } else {
    *result = 0.0;
  }
}

void PPB_Flash_Proxy::OnHostMsgGetSetting(PP_Instance instance,
                                          PP_FlashSetting setting,
                                          SerializedVarReturnValue id) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    id.Return(dispatcher(),
              enter.functions()->GetFlashAPI()->GetSetting(
                  instance, setting));
  } else {
    id.Return(dispatcher(), PP_MakeUndefined());
  }
}

void PPB_Flash_Proxy::OnHostMsgInvokePrinting(PP_Instance instance) {
  // This function is actually implemented in the PPB_Flash_Print interface.
  // It's rarely used enough that we just request this interface when needed.
  const PPB_Flash_Print_1_0* print_interface =
      static_cast<const PPB_Flash_Print_1_0*>(
          dispatcher()->local_get_interface()(PPB_FLASH_PRINT_INTERFACE_1_0));
  if (print_interface)
    print_interface->InvokePrinting(instance);
}

}  // namespace proxy
}  // namespace ppapi
