/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_webcontentsviewdelegateimpl.h>

#include <blpwtk2_contextmenuparams.h>
#include <blpwtk2_contextmenuitem.h>
#include <blpwtk2_webviewimpl.h>
#include <blpwtk2_textdirection.h>
#include <blpwtk2/private/blpwtk2_webview.mojom.h>

#include <base/strings/utf_string_conversions.h>
#include <content/public/browser/web_contents.h>
#include <content/public/common/context_menu_params.h>
#include <content/public/common/menu_item.h>
#include <third_party/blink/public/web/web_context_menu_data.h>
#include <ui/aura/window_tree_host.h>
#include <ui/aura/window.h>

namespace {

void convertItem(const content::MenuItem&         item1,
                 blpwtk2::mojom::ContextMenuItem *item2Impl);

void convertSubmenus(const content::MenuItem&         item1,
                     blpwtk2::mojom::ContextMenuItem *item2Impl)
{
    item2Impl->subMenu.resize(item1.submenu.size());
    for (size_t i = 0; i < item1.submenu.size(); ++i) {
        convertItem(item1.submenu[i], item2Impl->subMenu[i].get());
    }
}

void convertCustomItems(const content::ContextMenuParams&  params,
                        blpwtk2::mojom::ContextMenuParams *params2Impl)
{
    params2Impl->customItems.resize(params.custom_items.size());
    for (size_t i = 0; i <params.custom_items.size(); ++i) {
        convertItem(params.custom_items[i],
                    params2Impl->customItems[i].get());
    }
}

void convertItem(const content::MenuItem&         item1,
                 blpwtk2::mojom::ContextMenuItem *item2Impl)
{
    item2Impl->label = base::UTF16ToUTF8(item1.label);
    item2Impl->tooltip = base::UTF16ToUTF8(item1.tool_tip);
    switch (item1.type) {
    case content::MenuItem::OPTION:
        item2Impl->type = blpwtk2::mojom::ContextMenuItemType::OPTION;
        break;
    case content::MenuItem::CHECKABLE_OPTION:
        item2Impl->type = blpwtk2::mojom::ContextMenuItemType::CHECKABLE_OPTION;
        break;
    case content::MenuItem::GROUP:
        item2Impl->type = blpwtk2::mojom::ContextMenuItemType::GROUP;
        break;
    case content::MenuItem::SEPARATOR:
        item2Impl->type = blpwtk2::mojom::ContextMenuItemType::SEPARATOR;
        break;
    case content::MenuItem::SUBMENU:
        item2Impl->type = blpwtk2::mojom::ContextMenuItemType::SUBMENU;
        break;
    }
    item2Impl->action = item1.action;
    item2Impl->textDirection =
        item1.rtl ? blpwtk2::mojom::TextDirection::RIGHT_TO_LEFT :
                    blpwtk2::mojom::TextDirection::LEFT_TO_RIGHT;
    item2Impl->hasDirectionalOverride = item1.has_directional_override;
    item2Impl->enabled = item1.enabled;
    item2Impl->checked = item1.checked;
    convertSubmenus(item1, item2Impl);
}

void convertSpellcheck(const content::ContextMenuParams&  params,
                       blpwtk2::mojom::ContextMenuParams *params2Impl)
{
    params2Impl->misspelledWord = base::UTF16ToUTF8(params.misspelled_word);
    params2Impl->spellSuggestions.resize(params.dictionary_suggestions.size());
    for (std::size_t i = 0; i < params.dictionary_suggestions.size(); ++i) {
        params2Impl->spellSuggestions[i] = base::UTF16ToUTF8(params.dictionary_suggestions[i]);
    }
}

} // close unnamed namespace

namespace blpwtk2 {

                        // ---------------------------------
                        // class WebContentsViewDelegateImpl
                        // ---------------------------------

WebContentsViewDelegateImpl::WebContentsViewDelegateImpl(content::WebContents *webContents)
    : d_webContents(webContents)
{
}

content::WebDragDestDelegate*
WebContentsViewDelegateImpl::GetDragDestDelegate()
{
    return 0;
}

void WebContentsViewDelegateImpl::ShowContextMenu(
    content::RenderFrameHost          *renderFrameHost,
    const content::ContextMenuParams&  params)
{
    WebViewImpl *webViewImpl =
        static_cast<WebViewImpl*>(d_webContents->GetDelegate());
    webViewImpl->saveCustomContextMenuContext(renderFrameHost,
                                              params.custom_context);

    gfx::Point point(params.x, params.y);
    aura::WindowTreeHost* host = webViewImpl->getNativeView()->GetHost();
    host->ConvertDIPToScreenInPixels(&point);

    bool hasSelection = !params.selection_text.empty();
    mojom::ContextMenuParams params2Impl;

    params2Impl.x = point.x();
    params2Impl.y = point.y();
    params2Impl.canCut =
        params.is_editable && (params.edit_flags & blink::WebContextMenuData::kCanCut);
    params2Impl.canCopy =
        hasSelection || (params.is_editable && (params.edit_flags & blink::WebContextMenuData::kCanCopy));
    params2Impl.canPaste =
        params.is_editable && (params.edit_flags & blink::WebContextMenuData::kCanPaste);
    params2Impl.canDelete =
        params.is_editable && (params.edit_flags & blink::WebContextMenuData::kCanDelete);

    convertCustomItems(params, &params2Impl);
    convertSpellcheck(params, &params2Impl);

    ContextMenuParams params2(&params2Impl);
    webViewImpl->showContextMenu(params2);
}

void WebContentsViewDelegateImpl::StoreFocus()
{
}

bool WebContentsViewDelegateImpl::Focus()
{
    return false;
}

}  // close namespace blpwtk2

// vim: ts=4 et

