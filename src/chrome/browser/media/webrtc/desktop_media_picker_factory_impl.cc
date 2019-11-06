// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_media_picker_factory_impl.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_list_ash.h"
#include "chrome/browser/media/webrtc/native_desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"
#include "content/public/browser/desktop_capture.h"

DesktopMediaPickerFactoryImpl::DesktopMediaPickerFactoryImpl() = default;

DesktopMediaPickerFactoryImpl::~DesktopMediaPickerFactoryImpl() = default;

// static
DesktopMediaPickerFactoryImpl* DesktopMediaPickerFactoryImpl::GetInstance() {
  static base::NoDestructor<DesktopMediaPickerFactoryImpl> impl;
  return impl.get();
}

std::unique_ptr<DesktopMediaPicker>
DesktopMediaPickerFactoryImpl::CreatePicker() {
// DesktopMediaPicker is implemented only for Windows, OSX and Aura Linux
// builds.
#if defined(TOOLKIT_VIEWS) || defined(OS_MACOSX)
  return DesktopMediaPicker::Create();
#else
  return nullptr;
#endif
}

std::vector<std::unique_ptr<DesktopMediaList>>
DesktopMediaPickerFactoryImpl::CreateMediaList(
    const std::vector<content::DesktopMediaID::Type>& types) {
  // Keep same order as the input |sources| and avoid duplicates.
  std::vector<std::unique_ptr<DesktopMediaList>> source_lists;
  bool have_screen_list = false;
  bool have_window_list = false;
  bool have_tab_list = false;
  for (auto source_type : types) {
    switch (source_type) {
      case content::DesktopMediaID::TYPE_NONE:
        break;
      case content::DesktopMediaID::TYPE_SCREEN: {
        if (have_screen_list)
          continue;
        std::unique_ptr<DesktopMediaList> screen_list;
#if defined(OS_CHROMEOS)
        screen_list = std::make_unique<DesktopMediaListAsh>(
            content::DesktopMediaID::TYPE_SCREEN);
#else   // !defined(OS_CHROMEOS)
        screen_list = std::make_unique<NativeDesktopMediaList>(
            content::DesktopMediaID::TYPE_SCREEN,
            content::desktop_capture::CreateScreenCapturer());
#endif  // !defined(OS_CHROMEOS)
        have_screen_list = true;
        source_lists.push_back(std::move(screen_list));
        break;
      }
      case content::DesktopMediaID::TYPE_WINDOW: {
        if (have_window_list)
          continue;
        std::unique_ptr<DesktopMediaList> window_list;
#if defined(OS_CHROMEOS)
        window_list = std::make_unique<DesktopMediaListAsh>(
            content::DesktopMediaID::TYPE_WINDOW);
#else   // !defined(OS_CHROMEOS)
        window_list = std::make_unique<NativeDesktopMediaList>(
            content::DesktopMediaID::TYPE_WINDOW,
            content::desktop_capture::CreateWindowCapturer());
#endif  // !defined(OS_CHROMEOS)
        have_window_list = true;
        source_lists.push_back(std::move(window_list));
        break;
      }
      case content::DesktopMediaID::TYPE_WEB_CONTENTS: {
        if (have_tab_list)
          continue;
        std::unique_ptr<DesktopMediaList> tab_list =
            std::make_unique<TabDesktopMediaList>();
        have_tab_list = true;
        source_lists.push_back(std::move(tab_list));
        break;
      }
    }
  }
  return source_lists;
}
