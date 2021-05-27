// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_context.h"

#include <map>
#include <utility>

#include "libcef/browser/context.h"
#include "libcef/browser/media_router/media_router_manager.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/features/runtime.h"

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"

using content::BrowserThread;

namespace {

// Manages the global list of Impl instances.
class ImplManager {
 public:
  typedef std::vector<CefBrowserContext*> Vector;

  ImplManager() {}
  ~ImplManager() {
    DCHECK(all_.empty());
    DCHECK(map_.empty());
  }

  void AddImpl(CefBrowserContext* impl) {
    CEF_REQUIRE_UIT();
    DCHECK(!IsValidImpl(impl));
    all_.push_back(impl);
  }

  void RemoveImpl(CefBrowserContext* impl, const base::FilePath& path) {
    CEF_REQUIRE_UIT();

    Vector::iterator it = GetImplPos(impl);
    DCHECK(it != all_.end());
    all_.erase(it);

    if (!path.empty()) {
      PathMap::iterator it = map_.find(path);
      DCHECK(it != map_.end());
      if (it != map_.end())
        map_.erase(it);
    }
  }

  bool IsValidImpl(const CefBrowserContext* impl) {
    CEF_REQUIRE_UIT();
    return GetImplPos(impl) != all_.end();
  }

  CefBrowserContext* GetImplFromIDs(int render_process_id,
                                    int render_frame_id,
                                    int frame_tree_node_id,
                                    bool require_frame_match) {
    CEF_REQUIRE_UIT();
    for (const auto& context : all_) {
      if (context->IsAssociatedContext(render_process_id, render_frame_id,
                                       frame_tree_node_id,
                                       require_frame_match)) {
        return context;
      }
    }
    return nullptr;
  }

  CefBrowserContext* GetImplFromBrowserContext(
      const content::BrowserContext* context) {
    CEF_REQUIRE_UIT();
    if (!context)
      return nullptr;

    for (const auto& bc : all_) {
      if (bc->AsBrowserContext() == context)
        return bc;
    }
    return nullptr;
  }

  void SetImplPath(CefBrowserContext* impl, const base::FilePath& path) {
    CEF_REQUIRE_UIT();
    DCHECK(!path.empty());
    DCHECK(IsValidImpl(impl));
    DCHECK(GetImplFromPath(path) == nullptr);
    map_.insert(std::make_pair(path, impl));
  }

  CefBrowserContext* GetImplFromPath(const base::FilePath& path) {
    CEF_REQUIRE_UIT();
    DCHECK(!path.empty());
    PathMap::const_iterator it = map_.find(path);
    if (it != map_.end())
      return it->second;
    return nullptr;
  }

  const Vector GetAllImpl() const { return all_; }

 private:
  Vector::iterator GetImplPos(const CefBrowserContext* impl) {
    Vector::iterator it = all_.begin();
    for (; it != all_.end(); ++it) {
      if (*it == impl)
        return it;
    }
    return all_.end();
  }

  typedef std::map<base::FilePath, CefBrowserContext*> PathMap;
  PathMap map_;

  Vector all_;

  DISALLOW_COPY_AND_ASSIGN(ImplManager);
};

#if DCHECK_IS_ON()
// Because of DCHECK()s in the object destructor.
base::LazyInstance<ImplManager>::DestructorAtExit g_manager =
    LAZY_INSTANCE_INITIALIZER;
#else
base::LazyInstance<ImplManager>::Leaky g_manager = LAZY_INSTANCE_INITIALIZER;
#endif

CefBrowserContext* GetSelf(base::WeakPtr<CefBrowserContext> self) {
  CEF_REQUIRE_UIT();
  return self.get();
}

CefBrowserContext::CookieableSchemes MakeSupportedSchemes(
    const CefString& schemes_list,
    bool include_defaults) {
  std::vector<std::string> all_schemes;
  if (!schemes_list.empty()) {
    all_schemes =
        base::SplitString(schemes_list.ToString(), std::string(","),
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }

  if (include_defaults) {
    // Add default schemes that should always support cookies.
    // This list should match CookieMonster::kDefaultCookieableSchemes.
    all_schemes.push_back("http");
    all_schemes.push_back("https");
    all_schemes.push_back("ws");
    all_schemes.push_back("wss");
  }

  return base::make_optional(all_schemes);
}

}  // namespace

CefBrowserContext::CefBrowserContext(const CefRequestContextSettings& settings)
    : settings_(settings), weak_ptr_factory_(this) {
  g_manager.Get().AddImpl(this);
  getter_ = base::BindRepeating(GetSelf, weak_ptr_factory_.GetWeakPtr());
}

CefBrowserContext::~CefBrowserContext() {
  CEF_REQUIRE_UIT();
#if DCHECK_IS_ON()
  DCHECK(is_shutdown_);
#endif
}

void CefBrowserContext::Initialize() {
  cache_path_ = base::FilePath(CefString(&settings_.cache_path));

  if (!cache_path_.empty())
    g_manager.Get().SetImplPath(this, cache_path_);

  iothread_state_ = base::MakeRefCounted<CefIOThreadState>();

  if (settings_.cookieable_schemes_list.length > 0 ||
      settings_.cookieable_schemes_exclude_defaults) {
    cookieable_schemes_ =
        MakeSupportedSchemes(CefString(&settings_.cookieable_schemes_list),
                             !settings_.cookieable_schemes_exclude_defaults);
  }
}

void CefBrowserContext::Shutdown() {
  CEF_REQUIRE_UIT();

#if DCHECK_IS_ON()
  is_shutdown_ = true;
#endif

  // No CefRequestContext should be referencing this object any longer.
  DCHECK(request_context_set_.empty());

  // Unregister the context first to avoid re-entrancy during shutdown.
  g_manager.Get().RemoveImpl(this, cache_path_);

  // Destroy objects that may hold references to the MediaRouter.
  media_router_manager_.reset();
}

void CefBrowserContext::AddCefRequestContext(CefRequestContextImpl* context) {
  CEF_REQUIRE_UIT();
  request_context_set_.insert(context);
}

void CefBrowserContext::RemoveCefRequestContext(
    CefRequestContextImpl* context) {
  CEF_REQUIRE_UIT();

  request_context_set_.erase(context);

  // Delete ourselves when the reference count reaches zero.
  if (request_context_set_.empty()) {
    Shutdown();
    delete this;
  }
}

// static
CefBrowserContext* CefBrowserContext::FromCachePath(
    const base::FilePath& cache_path) {
  return g_manager.Get().GetImplFromPath(cache_path);
}

// static
CefBrowserContext* CefBrowserContext::FromIDs(int render_process_id,
                                              int render_frame_id,
                                              int frame_tree_node_id,
                                              bool require_frame_match) {
  return g_manager.Get().GetImplFromIDs(render_process_id, render_frame_id,
                                        frame_tree_node_id,
                                        require_frame_match);
}

// static
CefBrowserContext* CefBrowserContext::FromBrowserContext(
    const content::BrowserContext* context) {
  return g_manager.Get().GetImplFromBrowserContext(context);
}

// static
CefBrowserContext* CefBrowserContext::FromProfile(const Profile* profile) {
  auto* cef_context = FromBrowserContext(profile);
  if (cef_context)
    return cef_context;

  if (cef::IsChromeRuntimeEnabled()) {
    auto* original_profile = profile->GetOriginalProfile();
    if (original_profile != profile) {
      // With the Chrome runtime if the user launches an incognito window via
      // the UI we might be associated with the original Profile instead of the
      // (current) incognito profile.
      return FromBrowserContext(original_profile);
    }
  }

  return nullptr;
}

// static
std::vector<CefBrowserContext*> CefBrowserContext::GetAll() {
  return g_manager.Get().GetAllImpl();
}

void CefBrowserContext::OnRenderFrameCreated(
    CefRequestContextImpl* request_context,
    int render_process_id,
    int render_frame_id,
    int frame_tree_node_id,
    bool is_main_frame,
    bool is_guest_view) {
  CEF_REQUIRE_UIT();
  DCHECK_GE(render_process_id, 0);
  DCHECK_GE(render_frame_id, 0);
  DCHECK_GE(frame_tree_node_id, 0);

  render_id_set_.insert(std::make_pair(render_process_id, render_frame_id));
  node_id_set_.insert(frame_tree_node_id);

  CefRefPtr<CefRequestContextHandler> handler = request_context->GetHandler();
  if (handler) {
    handler_map_.AddHandler(render_process_id, render_frame_id,
                            frame_tree_node_id, handler);

    CEF_POST_TASK(CEF_IOT,
                  base::Bind(&CefIOThreadState::AddHandler, iothread_state_,
                             render_process_id, render_frame_id,
                             frame_tree_node_id, handler));
  }
}

void CefBrowserContext::OnRenderFrameDeleted(
    CefRequestContextImpl* request_context,
    int render_process_id,
    int render_frame_id,
    int frame_tree_node_id,
    bool is_main_frame,
    bool is_guest_view) {
  CEF_REQUIRE_UIT();
  DCHECK_GE(render_process_id, 0);
  DCHECK_GE(render_frame_id, 0);
  DCHECK_GE(frame_tree_node_id, 0);

  auto it1 =
      render_id_set_.find(std::make_pair(render_process_id, render_frame_id));
  if (it1 != render_id_set_.end())
    render_id_set_.erase(it1);

  auto it2 = node_id_set_.find(frame_tree_node_id);
  if (it2 != node_id_set_.end())
    node_id_set_.erase(it2);

  CefRefPtr<CefRequestContextHandler> handler = request_context->GetHandler();
  if (handler) {
    handler_map_.RemoveHandler(render_process_id, render_frame_id,
                               frame_tree_node_id);

    CEF_POST_TASK(CEF_IOT, base::Bind(&CefIOThreadState::RemoveHandler,
                                      iothread_state_, render_process_id,
                                      render_frame_id, frame_tree_node_id));
  }

  if (is_main_frame) {
    ClearPluginLoadDecision(render_process_id);
  }
}

CefRefPtr<CefRequestContextHandler> CefBrowserContext::GetHandler(
    int render_process_id,
    int render_frame_id,
    int frame_tree_node_id,
    bool require_frame_match) const {
  CEF_REQUIRE_UIT();
  return handler_map_.GetHandler(render_process_id, render_frame_id,
                                 frame_tree_node_id, require_frame_match);
}

bool CefBrowserContext::IsAssociatedContext(int render_process_id,
                                            int render_frame_id,
                                            int frame_tree_node_id,
                                            bool require_frame_match) const {
  CEF_REQUIRE_UIT();

  if (render_process_id >= 0 && render_frame_id >= 0) {
    const auto it1 =
        render_id_set_.find(std::make_pair(render_process_id, render_frame_id));
    if (it1 != render_id_set_.end())
      return true;
  }

  if (frame_tree_node_id >= 0) {
    const auto it2 = node_id_set_.find(frame_tree_node_id);
    if (it2 != node_id_set_.end())
      return true;
  }

  if (render_process_id >= 0 && !require_frame_match) {
    // Choose an arbitrary handler for the same process.
    for (const auto& render_ids : render_id_set_) {
      if (render_ids.first == render_process_id)
        return true;
    }
  }

  return false;
}

void CefBrowserContext::AddPluginLoadDecision(
    int render_process_id,
    const base::FilePath& plugin_path,
    bool is_main_frame,
    const url::Origin& main_frame_origin,
    chrome::mojom::PluginStatus status) {
  CEF_REQUIRE_UIT();
  DCHECK_GE(render_process_id, 0);
  DCHECK(!plugin_path.empty());

  plugin_load_decision_map_.insert(std::make_pair(
      std::make_pair(std::make_pair(render_process_id, plugin_path),
                     std::make_pair(is_main_frame, main_frame_origin)),
      status));
}

bool CefBrowserContext::HasPluginLoadDecision(
    int render_process_id,
    const base::FilePath& plugin_path,
    bool is_main_frame,
    const url::Origin& main_frame_origin,
    chrome::mojom::PluginStatus* status) const {
  CEF_REQUIRE_UIT();
  DCHECK_GE(render_process_id, 0);
  DCHECK(!plugin_path.empty());

  PluginLoadDecisionMap::const_iterator it = plugin_load_decision_map_.find(
      std::make_pair(std::make_pair(render_process_id, plugin_path),
                     std::make_pair(is_main_frame, main_frame_origin)));
  if (it == plugin_load_decision_map_.end())
    return false;

  *status = it->second;
  return true;
}

void CefBrowserContext::ClearPluginLoadDecision(int render_process_id) {
  CEF_REQUIRE_UIT();

  if (render_process_id == -1) {
    plugin_load_decision_map_.clear();
  } else {
    PluginLoadDecisionMap::iterator it = plugin_load_decision_map_.begin();
    while (it != plugin_load_decision_map_.end()) {
      if (it->first.first.first == render_process_id)
        it = plugin_load_decision_map_.erase(it);
      else
        ++it;
    }
  }
}

void CefBrowserContext::RegisterSchemeHandlerFactory(
    const CefString& scheme_name,
    const CefString& domain_name,
    CefRefPtr<CefSchemeHandlerFactory> factory) {
  CEF_POST_TASK(CEF_IOT,
                base::Bind(&CefIOThreadState::RegisterSchemeHandlerFactory,
                           iothread_state_, scheme_name, domain_name, factory));
}

void CefBrowserContext::ClearSchemeHandlerFactories() {
  CEF_POST_TASK(CEF_IOT,
                base::Bind(&CefIOThreadState::ClearSchemeHandlerFactories,
                           iothread_state_));
}

void CefBrowserContext::LoadExtension(
    const CefString& root_directory,
    CefRefPtr<CefDictionaryValue> manifest,
    CefRefPtr<CefExtensionHandler> handler,
    CefRefPtr<CefRequestContext> loader_context) {
  NOTIMPLEMENTED();
  if (handler)
    handler->OnExtensionLoadFailed(ERR_ABORTED);
}

bool CefBrowserContext::GetExtensions(std::vector<CefString>& extension_ids) {
  NOTIMPLEMENTED();
  return false;
}

CefRefPtr<CefExtension> CefBrowserContext::GetExtension(
    const CefString& extension_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool CefBrowserContext::UnloadExtension(const CefString& extension_id) {
  NOTIMPLEMENTED();
  return false;
}

bool CefBrowserContext::IsPrintPreviewSupported() const {
  return true;
}

network::mojom::NetworkContext* CefBrowserContext::GetNetworkContext() {
  CEF_REQUIRE_UIT();
  auto browser_context = AsBrowserContext();
  return browser_context->GetDefaultStoragePartition(browser_context)
      ->GetNetworkContext();
}

CefMediaRouterManager* CefBrowserContext::GetMediaRouterManager() {
  CEF_REQUIRE_UIT();
  if (!media_router_manager_) {
    media_router_manager_.reset(new CefMediaRouterManager(AsBrowserContext()));
  }
  return media_router_manager_.get();
}

CefBrowserContext::CookieableSchemes CefBrowserContext::GetCookieableSchemes()
    const {
  CEF_REQUIRE_UIT();
  if (cookieable_schemes_)
    return cookieable_schemes_;

  return GetGlobalCookieableSchemes();
}

// static
CefBrowserContext::CookieableSchemes
CefBrowserContext::GetGlobalCookieableSchemes() {
  CEF_REQUIRE_UIT();

  static base::NoDestructor<CookieableSchemes> schemes(
      []() -> CookieableSchemes {
        const auto& settings = CefContext::Get()->settings();
        if (settings.cookieable_schemes_list.length > 0 ||
            settings.cookieable_schemes_exclude_defaults) {
          return MakeSupportedSchemes(
              CefString(&settings.cookieable_schemes_list),
              !settings.cookieable_schemes_exclude_defaults);
        }
        return base::nullopt;
      }());
  return *schemes;
}
