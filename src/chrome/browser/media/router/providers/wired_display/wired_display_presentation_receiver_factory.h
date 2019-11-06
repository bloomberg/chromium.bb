// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_WIRED_DISPLAY_WIRED_DISPLAY_PRESENTATION_RECEIVER_FACTORY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_WIRED_DISPLAY_WIRED_DISPLAY_PRESENTATION_RECEIVER_FACTORY_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_presentation_receiver.h"

class Profile;

namespace gfx {
class Rect;
}  // namespace gfx

namespace media_router {

// A factory for creating a platform-specific WiredDisplayPresentationReceiver.
class WiredDisplayPresentationReceiverFactory {
 public:
  using CreateReceiverCallback =
      base::RepeatingCallback<std::unique_ptr<WiredDisplayPresentationReceiver>(
          Profile* profile,
          const gfx::Rect& bounds,
          base::OnceClosure termination_callback,
          base::RepeatingCallback<void(const std::string&)>
              title_change_callback)>;

  static std::unique_ptr<WiredDisplayPresentationReceiver> Create(
      Profile* profile,
      const gfx::Rect& bounds,
      base::OnceClosure termination_callback,
      base::RepeatingCallback<void(const std::string&)> title_change_callback);

  // Sets the callback used to instantiate a presentation receiver. Used only in
  // tests.
  static void SetCreateReceiverCallbackForTest(CreateReceiverCallback callback);

 private:
  friend struct base::LazyInstanceTraitsBase<
      WiredDisplayPresentationReceiverFactory>;

  WiredDisplayPresentationReceiverFactory();
  virtual ~WiredDisplayPresentationReceiverFactory();

  static WiredDisplayPresentationReceiverFactory* GetInstance();

  // Used in tests. When this is set, it is used for creating a receiver.
  CreateReceiverCallback create_receiver_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(WiredDisplayPresentationReceiverFactory);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_WIRED_DISPLAY_WIRED_DISPLAY_PRESENTATION_RECEIVER_FACTORY_H_
