// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/animated_image_view_example.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/lottie/animation.h"
#include "ui/views/border.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

namespace {

// This class can load a skottie(and lottie) animation file from disk and play
// it in a view as AnimatedImageView.
// See https://skia.org/user/modules/skottie for more info on skottie.
class AnimationGallery : public BoxLayoutView, public TextfieldController {
 public:
  AnimationGallery() {
    View* image_view_container = nullptr;
    BoxLayoutView* file_container = nullptr;
    Builder<BoxLayoutView>(this)
        .SetOrientation(BoxLayout::Orientation::kVertical)
        .SetInsideBorderInsets(gfx::Insets(10))
        .SetBetweenChildSpacing(10)
        .AddChildren(
            Builder<Textfield>()
                .CopyAddressTo(&size_input_)
                .SetPlaceholderText(u"Size in dip (Empty for default)")
                .SetController(this),
            Builder<View>()
                .CopyAddressTo(&image_view_container)
                .SetUseDefaultFillLayout(true)
                .AddChild(Builder<AnimatedImageView>()
                              .CopyAddressTo(&animated_image_view_)
                              .SetBorder(CreateSolidSidedBorder(
                                  1, 1, 1, 1, SK_ColorBLACK))),
            Builder<BoxLayoutView>()
                .CopyAddressTo(&file_container)
                .SetInsideBorderInsets(gfx::Insets(10))
                .SetBetweenChildSpacing(10)
                .AddChildren(
                    Builder<Textfield>()
                        .CopyAddressTo(&file_chooser_)
                        .SetPlaceholderText(u"Enter path to lottie JSON file"),
                    Builder<MdTextButton>()
                        .SetCallback(base::BindRepeating(
                            &AnimationGallery::ButtonPressed,
                            base::Unretained(this)))
                        .SetText(u"Render")))
        .BuildChildren();
    SetFlexForView(image_view_container, 1);
    file_container->SetFlexForView(file_chooser_, 1);
  }

  AnimationGallery(const AnimationGallery&) = delete;
  AnimationGallery& operator=(const AnimationGallery&) = delete;

  ~AnimationGallery() override = default;

  // TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const std::u16string& new_contents) override {
    if (sender == size_input_) {
      if (!base::StringToInt(new_contents, &size_) && (size_ > 0)) {
        size_ = 0;
        size_input_->SetText(std::u16string());
      }
      Update();
    }
  }

  void ButtonPressed(const ui::Event& event) {
    std::string json;
    base::ScopedAllowBlockingForTesting allow_blocking;
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
    base::FilePath path(base::UTF16ToUTF8(file_chooser_->GetText()));
#else
    base::FilePath path(base::UTF16ToWide(file_chooser_->GetText()));
#endif  // defined(OS_POSIX)
    if (base::ReadFileToString(path, &json)) {
      auto skottie = cc::SkottieWrapper::CreateSerializable(
          std::vector<uint8_t>(json.begin(), json.end()));
      animated_image_view_->SetAnimatedImage(
          std::make_unique<lottie::Animation>(skottie));
      animated_image_view_->Play();
      Update();
    }
  }

 private:
  void Update() {
    if (size_ > 24)
      animated_image_view_->SetImageSize(gfx::Size(size_, size_));
    else
      animated_image_view_->ResetImageSize();
    InvalidateLayout();
  }

  AnimatedImageView* animated_image_view_ = nullptr;
  Textfield* size_input_ = nullptr;
  Textfield* file_chooser_ = nullptr;

  int size_ = 0;
};

}  // namespace

AnimatedImageViewExample::AnimatedImageViewExample()
    : ExampleBase("Animated Image View") {}

AnimatedImageViewExample::~AnimatedImageViewExample() = default;

void AnimatedImageViewExample::CreateExampleView(View* container) {
  container->SetUseDefaultFillLayout(true);
  container->AddChildView(std::make_unique<AnimationGallery>());
}

}  // namespace examples
}  // namespace views
