// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/share/share_submenu_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_sub_menu_model.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/renderer_context_menu/context_menu_content_type.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/browser/context_menu_params.h"
#include "extensions/buildflags/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-forward.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/vector2d.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#endif

class AccessibilityLabelsMenuObserver;
class ClickToCallContextMenuObserver;
class LinkToTextMenuObserver;
class PrintPreviewContextMenuObserver;
class Profile;
class QuickAnswersMenuObserver;
class SharedClipboardContextMenuObserver;
class SpellingMenuObserver;
class SpellingOptionsSubMenuObserver;

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace extensions {
class Extension;
class MenuItem;
}

namespace gfx {
class Point;
}

namespace blink {
namespace mojom {
class MediaPlayerAction;
}
}

namespace ui {
class DataTransferEndpoint;
}

namespace web_app {
class SystemWebAppDelegate;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace policy {
class DlpRulesManager;
}  // namespace policy
#endif

class RenderViewContextMenu
    : public RenderViewContextMenuBase,
      public custom_handlers::ProtocolHandlerRegistry::Observer {
 public:
  using ExecutePluginActionCallback =
      base::OnceCallback<void(content::RenderFrameHost*,
                              blink::mojom::PluginActionType)>;

  RenderViewContextMenu(content::RenderFrameHost& render_frame_host,
                        const content::ContextMenuParams& params);

  RenderViewContextMenu(const RenderViewContextMenu&) = delete;
  RenderViewContextMenu& operator=(const RenderViewContextMenu&) = delete;

  ~RenderViewContextMenu() override;

  // Adds the spell check service item to the context menu.
  static void AddSpellCheckServiceItem(ui::SimpleMenuModel* menu,
                                       bool is_checked);

  // RenderViewContextMenuBase:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void AddSpellCheckServiceItem(bool is_checked) override;
  void AddAccessibilityLabelsServiceItem(bool is_checked) override;

  // Registers a one-time callback that will be called the next time a context
  // menu is shown.
  static void RegisterMenuShownCallbackForTesting(
      base::OnceCallback<void(RenderViewContextMenu*)> cb);

  // Register a one-time callback that will be called the next time a plugin
  // action is executed from a given render frame.
  void RegisterExecutePluginActionCallbackForTesting(
      base::OnceCallback<void(content::RenderFrameHost*,
                              blink::mojom::PluginActionType)> cb);

 protected:
  Profile* GetProfile() const;

  // This may return nullptr (e.g. for WebUI dialogs).
  Browser* GetBrowser() const;

  // Returns a (possibly truncated) version of the current selection text
  // suitable for putting in the title of a menu item.
  std::u16string PrintableSelectionText();

  // Helper function to escape "&" as "&&".
  void EscapeAmpersands(std::u16string* text);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ContextMenuMatcher extension_items_;
#endif
  void RecordUsedItem(int id) override;

  // Returns true if the browser is in HTML fullscreen mode, initiated by the
  // page (as opposed to the user). Used to determine which shortcut to display.
  bool IsHTML5Fullscreen() const;

  // Returns true if keyboard lock is active and requires the user to press and
  // hold escape to exit exclusive access mode.
  bool IsPressAndHoldEscRequiredToExitFullscreen() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  virtual const policy::DlpRulesManager* GetDlpRulesManager() const;
#endif

 private:
  friend class RenderViewContextMenuTest;
  friend class TestRenderViewContextMenu;
  friend class FormatUrlForClipboardTest;

  static bool IsDevToolsURL(const GURL& url);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  static bool ExtensionContextAndPatternMatch(
      const content::ContextMenuParams& params,
      const extensions::MenuItem::ContextList& contexts,
      const extensions::URLPatternSet& target_url_patterns);
  static bool MenuItemMatchesParams(const content::ContextMenuParams& params,
                                    const extensions::MenuItem* item);
#endif

  // Formats a URL to be written to the clipboard and returns the formatted
  // string. Used by WriteURLToClipboard(), but kept in a separate function so
  // the formatting behavior can be tested without having to initialize the
  // clipboard. |url| must be valid and non-empty.
  static std::u16string FormatURLForClipboard(const GURL& url);

  // Writes the specified text/url to the system clipboard.
  void WriteURLToClipboard(const GURL& url);

  // RenderViewContextMenuBase:
  void InitMenu() override;
  void RecordShownItem(int id) override;
#if BUILDFLAG(ENABLE_PLUGINS)
  void HandleAuthorizeAllPlugins() override;
#endif
  void NotifyMenuShown() override;

  // Gets the extension (if any) associated with the WebContents that we're in.
  const extensions::Extension* GetExtension() const;

  // Queries the translate service to obtain the user's transate target
  // language.
  std::string GetTargetLanguage() const;

  void AppendDeveloperItems();
  void AppendDevtoolsForUnpackedExtensions();
  void AppendLinkItems();
  void AppendOpenWithLinkItems();
  void AppendSmartSelectionActionItems();
  void AppendOpenInWebAppLinkItems();
  void AppendQuickAnswersItems();
  void AppendImageItems();
  void AppendAudioItems();
  void AppendCanvasItems();
  void AppendVideoItems();
  void AppendMediaItems();
  void AppendPluginItems();
  void AppendPageItems();
  void AppendExitFullscreenItem();
  void AppendCopyItem();
  void AppendLinkToTextItems();
  void AppendPrintItem();
  void AppendMediaRouterItem();
  void AppendRotationItems();
  void AppendEditableItems();
  void AppendLanguageSettings();
  void AppendSpellingSuggestionItems();
  // Returns true if the items were appended. This might not happen in all
  // cases, e.g. these are only appended if a screen reader is enabled.
  bool AppendAccessibilityLabelsItems();
  void AppendSearchProvider();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  void AppendAllExtensionItems();
  void AppendCurrentExtensionItems();
#endif
  void AppendPrintPreviewItems();
  void AppendSearchLensForImageItems();
  void AppendSearchWebForImageItems();
  void AppendProtocolHandlerSubMenu();
  void AppendPasswordItems();
  void AppendPictureInPictureItem();
  void AppendSharingItems();
#if !defined(OS_FUCHSIA)
  void AppendClickToCallItem();
#endif
  void AppendSharedClipboardItem();
  void AppendRegionSearchItem();
  void AppendQRCodeGeneratorItem(bool for_image, bool draw_icon);

  std::unique_ptr<ui::DataTransferEndpoint> CreateDataEndpoint(
      bool notify_if_restricted) const;

  // Helper function for checking policies.
  bool IsSaveAsItemAllowedByPolicy() const;

  // Command enabled query functions.
  bool IsReloadEnabled() const;
  bool IsViewSourceEnabled() const;
  bool IsDevCommandEnabled(int id) const;
  bool IsTranslateEnabled() const;
  bool IsSaveLinkAsEnabled() const;
  bool IsSaveImageAsEnabled() const;
  bool IsSaveAsEnabled() const;
  bool IsSavePageEnabled() const;
  bool IsPasteEnabled() const;
  bool IsPasteAndMatchStyleEnabled() const;
  bool IsPrintPreviewEnabled() const;
  bool IsQRCodeGeneratorEnabled() const;
  bool IsRouteMediaEnabled() const;
  bool IsOpenLinkOTREnabled() const;
  bool IsSearchWebForEnabled() const;
  bool IsRegionSearchEnabled() const;

  // Command execution functions.
  void ExecOpenWebApp();
  void ExecProtocolHandler(int event_flags, int handler_index);
  void ExecOpenLinkInProfile(int profile_index);
  void ExecInspectElement();
  void ExecInspectBackgroundPage();
  void ExecSaveLinkAs();
  void ExecSaveAs();
  void ExecExitFullscreen();
  void ExecCopyLinkText();
  void ExecCopyImageAt();
  void ExecSearchLensForImage();
  void ExecRegionSearch(int event_flags,
                        bool is_google_default_search_provider);
  void ExecSearchWebForImage();
  void ExecLoadImage();
  void ExecPlayPause();
  void ExecMute();
  void ExecLoop();
  void ExecControls();
  void ExecRotateCW();
  void ExecRotateCCW();
  void ExecReloadPackagedApp();
  void ExecRestartPackagedApp();
  void ExecPrint();
  void ExecRouteMedia();
  void ExecTranslate();
  void ExecLanguageSettings(int event_flags);
  void ExecProtocolHandlerSettings(int event_flags);
  void ExecPictureInPicture();

  void MediaPlayerActionAt(const gfx::Point& location,
                           const blink::mojom::MediaPlayerAction& action);
  void PluginActionAt(const gfx::Point& location,
                      blink::mojom::PluginActionType plugin_action);

  // Returns a list of registered ProtocolHandlers that can handle the clicked
  // on URL.
  custom_handlers::ProtocolHandlerRegistry::ProtocolHandlerList
  GetHandlersForLinkUrl();

  // ProtocolHandlerRegistry::Observer:
  void OnProtocolHandlerRegistryChanged() override;

  // The destination URL to use if the user tries to search for or navigate to
  // a text selection.
  GURL selection_navigation_url_;

  ui::SimpleMenuModel profile_link_submenu_model_;
  std::vector<base::FilePath> profile_link_paths_;
  bool multiple_profiles_open_;

  // Protocol handling:
  // - The submenu containing the installed protocol handlers.
  ui::SimpleMenuModel protocol_handler_submenu_model_;
  // - The registry with the protocols.
  raw_ptr<custom_handlers::ProtocolHandlerRegistry> protocol_handler_registry_;
  // - The observation of the registry.
  base::ScopedObservation<custom_handlers::ProtocolHandlerRegistry,
                          custom_handlers::ProtocolHandlerRegistry::Observer>
      protocol_handler_registry_observation_{this};
  // - Whether or not the registered protocols have changed since the menu was
  //   built.
  bool is_protocol_submenu_valid_ = false;

  // An observer that handles spelling suggestions, "Add to dictionary", and
  // "Use enhanced spell check" items.
  std::unique_ptr<SpellingMenuObserver> spelling_suggestions_menu_observer_;

  // An observer that handles accessibility labels items.
  std::unique_ptr<AccessibilityLabelsMenuObserver>
      accessibility_labels_menu_observer_;
  ui::SimpleMenuModel accessibility_labels_submenu_model_;

#if !defined(OS_MAC)
  // An observer that handles the submenu for showing spelling options. This
  // submenu lets users select the spelling language, for example.
  std::unique_ptr<SpellingOptionsSubMenuObserver>
      spelling_options_submenu_observer_;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // An observer that handles "Open with <app>" items.
  std::unique_ptr<RenderViewContextMenuObserver> open_with_menu_observer_;
  // An observer that handles smart text selection action items.
  std::unique_ptr<RenderViewContextMenuObserver>
      start_smart_selection_action_menu_observer_;
  std::unique_ptr<QuickAnswersMenuObserver> quick_answers_menu_observer_;
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // An observer that disables menu items when print preview is active.
  std::unique_ptr<PrintPreviewContextMenuObserver> print_preview_menu_observer_;
#endif

  std::unique_ptr<LinkToTextMenuObserver> link_to_text_menu_observer_;

  // In the case of a MimeHandlerView this will point to the WebContents that
  // embeds the MimeHandlerViewGuest. Otherwise this will be the same as
  // |source_web_contents_|.
  const raw_ptr<content::WebContents> embedder_web_contents_;

  // Send tab to self submenu.
  std::unique_ptr<send_tab_to_self::SendTabToSelfSubMenuModel>
      send_tab_to_self_sub_menu_model_;

  // Sharing submenu, if present.
  std::unique_ptr<share::ShareSubmenuModel> share_submenu_model_;

  // Click to call menu observer.
  std::unique_ptr<ClickToCallContextMenuObserver>
      click_to_call_context_menu_observer_;

  // Shared clipboard menu observer.
  std::unique_ptr<SharedClipboardContextMenuObserver>
      shared_clipboard_context_menu_observer_;

  // The system app (if any) associated with the WebContents we're in.
  raw_ptr<const web_app::SystemWebAppDelegate> system_app_ = nullptr;

  // A one-time callback that will be called the next time a plugin action is
  // executed from a given render frame.
  ExecutePluginActionCallback execute_plugin_action_callback_;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Controller for Lens Region Search feature. This controller will be
  // destroyed as soon as the RenderViewContextMenu object is destroyed. The
  // RenderViewContextMenu is reset every time it is shown, but persists between
  // uses so that it doesn't go out of scope before finishing work. This means
  // that when another context menu opens, the Lens Region Search feature will
  // close if active.
  std::unique_ptr<lens::LensRegionSearchController>
      lens_region_search_controller_;
#endif
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_
