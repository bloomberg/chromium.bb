// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "skia/public/interfaces/test/traits_test_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"

namespace skia {

namespace {

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() {}

 protected:
  mojom::TraitsTestServicePtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

 private:
  // TraitsTestService:
  void EchoImageFilter(const sk_sp<SkImageFilter>& i,
                       const EchoImageFilterCallback& callback) override {
    callback.Run(i);
  }

  base::MessageLoop loop_;
  mojo::BindingSet<TraitsTestService> traits_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

static sk_sp<SkImageFilter> make_scale(float amount,
                                       sk_sp<SkImageFilter> input) {
  SkScalar s = amount;
  SkScalar matrix[20] = {s, 0, 0, 0, 0, 0, s, 0, 0, 0,
                         0, 0, s, 0, 0, 0, 0, 0, s, 0};
  sk_sp<SkColorFilter> filter(
      SkColorFilter::MakeMatrixFilterRowMajor255(matrix));
  return SkColorFilterImageFilter::Make(std::move(filter), std::move(input));
}

}  // namespace

TEST_F(StructTraitsTest, ImageFilter) {
  sk_sp<SkImageFilter> input(make_scale(0.5f, nullptr));
  SkString input_str;
  input->toString(&input_str);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  sk_sp<SkImageFilter> output;
  proxy->EchoImageFilter(input, &output);
  SkString output_str;
  output->toString(&output_str);
  EXPECT_EQ(input_str, output_str);
}

}  // namespace skia
