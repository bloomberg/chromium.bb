// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "secondary_tile.h"

#include <windows.ui.startscreen.h>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "crypto/sha2.h"
#include "googleurl/src/gurl.h"
#include "win8/metro_driver/chrome_app_view.h"
#include "win8/metro_driver/winrt_utils.h"

namespace {

string16 GenerateTileId(const string16& url_str) {
  uint8 hash[crypto::kSHA256Length];
  crypto::SHA256HashString(UTF16ToUTF8(url_str), hash, sizeof(hash));
  std::string hash_str = base::HexEncode(hash, sizeof(hash));
  return UTF8ToUTF16(hash_str);
}

string16 GetLogoUrlString() {
  FilePath module_path;
  PathService::Get(base::DIR_MODULE, &module_path);
  string16 scheme(L"ms-appx:///");
  return scheme.append(module_path.BaseName().value())
               .append(L"/SecondaryTile.png");
}

BOOL IsPinnedToStartScreen(const string16& url_str) {
  mswr::ComPtr<winui::StartScreen::ISecondaryTileStatics> tile_statics;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_StartScreen_SecondaryTile,
      tile_statics.GetAddressOf());
  CheckHR(hr, "Failed to create instance of ISecondaryTileStatics");

  boolean exists;
  hr = tile_statics->Exists(MakeHString(GenerateTileId(url_str)), &exists);
  CheckHR(hr, "ISecondaryTileStatics.Exists failed");
  return exists;
}

void DeleteTileFromStartScreen(const string16& url_str) {
  DVLOG(1) << __FUNCTION__;
  mswr::ComPtr<winui::StartScreen::ISecondaryTileFactory> tile_factory;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_StartScreen_SecondaryTile,
      tile_factory.GetAddressOf());
  CheckHR(hr, "Failed to create instance of ISecondaryTileFactory");

  mswrw::HString id;
  id.Attach(MakeHString(GenerateTileId(url_str)));

  mswr::ComPtr<winui::StartScreen::ISecondaryTile> tile;
  hr = tile_factory->CreateWithId(id.Get(), tile.GetAddressOf());
  CheckHR(hr, "Failed to create tile");

  mswr::ComPtr<winfoundtn::IAsyncOperation<bool>> completion;
  hr = tile->RequestDeleteAsync(completion.GetAddressOf());
  CheckHR(hr, "RequestDeleteAsync failed");

  typedef winfoundtn::IAsyncOperationCompletedHandler<bool> RequestDoneType;
  mswr::ComPtr<RequestDoneType> handler(mswr::Callback<RequestDoneType>(
      globals.view, &ChromeAppView::TileRequestCreateDone));
  DCHECK(handler.Get() != NULL);
  hr = completion->put_Completed(handler.Get());
  CheckHR(hr, "Failed to put_Completed");
}

void CreateTileOnStartScreen(const string16& title_str,
                             const string16& url_str) {
  VLOG(1) << __FUNCTION__;
  mswr::ComPtr<winui::StartScreen::ISecondaryTileFactory> tile_factory;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_StartScreen_SecondaryTile,
      tile_factory.GetAddressOf());
  CheckHR(hr, "Failed to create instance of ISecondaryTileFactory");

  winui::StartScreen::TileOptions options =
      winui::StartScreen::TileOptions_ShowNameOnLogo;
  mswrw::HString title;
  title.Attach(MakeHString(title_str));
  mswrw::HString id;
  id.Attach(MakeHString(GenerateTileId(url_str)));
  mswrw::HString args;
  // The url is just passed into the tile agruments as is. Metro and desktop
  // chrome will see the arguments as command line parameters.
  // A GURL is used to ensure any spaces are properly escaped.
  GURL url(url_str);
  args.Attach(MakeHString(UTF8ToUTF16(url.spec())));

  mswr::ComPtr<winfoundtn::IUriRuntimeClassFactory> uri_factory;
  hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_Foundation_Uri,
      uri_factory.GetAddressOf());
  CheckHR(hr, "Failed to create URIFactory");

  mswrw::HString logo_url;
  logo_url.Attach(MakeHString(GetLogoUrlString()));
  mswr::ComPtr<winfoundtn::IUriRuntimeClass> uri;
  hr = uri_factory->CreateUri(logo_url.Get(), &uri);
  CheckHR(hr, "Failed to create URI");

  mswr::ComPtr<winui::StartScreen::ISecondaryTile> tile;
  hr = tile_factory->CreateTile(id.Get(),
                                title.Get(),
                                title.Get(),
                                args.Get(),
                                options,
                                uri.Get(),
                                tile.GetAddressOf());
  CheckHR(hr, "Failed to create tile");

  hr = tile->put_ForegroundText(winui::StartScreen::ForegroundText_Light);
  CheckHR(hr, "Failed to change foreground text color");

  mswr::ComPtr<winfoundtn::IAsyncOperation<bool>> completion;
  hr = tile->RequestCreateAsync(completion.GetAddressOf());
  CheckHR(hr, "RequestCreateAsync failed");

  typedef winfoundtn::IAsyncOperationCompletedHandler<bool> RequestDoneType;
  mswr::ComPtr<RequestDoneType> handler(mswr::Callback<RequestDoneType>(
      globals.view, &ChromeAppView::TileRequestCreateDone));
  DCHECK(handler.Get() != NULL);
  hr = completion->put_Completed(handler.Get());
  CheckHR(hr, "Failed to put_Completed");
}

void TogglePinnedToStartScreen(const string16& title_str,
                               const string16& url_str) {
  if (IsPinnedToStartScreen(url_str)) {
    DeleteTileFromStartScreen(url_str);
    return;
  }

  CreateTileOnStartScreen(title_str, url_str);
}

}  // namespace

BOOL MetroIsPinnedToStartScreen(const string16& url) {
  VLOG(1) << __FUNCTION__ << " url: " << url;
  return IsPinnedToStartScreen(url);
}

void MetroTogglePinnedToStartScreen(const string16& title,
                                    const string16& url) {
  DVLOG(1) << __FUNCTION__ << " title:" << title << " url: " << url;
  globals.appview_msg_loop->PostTask(
      FROM_HERE, base::Bind(&TogglePinnedToStartScreen, title, url));
}
