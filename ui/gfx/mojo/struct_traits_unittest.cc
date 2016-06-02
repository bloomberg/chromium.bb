// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/mojo/traits_test_service.mojom.h"
#include "ui/gfx/transform.h"

namespace gfx {

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
  void EchoTransform(const Transform& t,
                     const EchoTransformCallback& callback) override {
    callback.Run(t);
  }

  base::MessageLoop loop_;
  mojo::BindingSet<TraitsTestService> traits_test_bindings_;
};

}  // namespace

TEST_F(StructTraitsTest, Transform) {
  const float col1row1 = 1.f;
  const float col2row1 = 2.f;
  const float col3row1 = 3.f;
  const float col4row1 = 4.f;
  const float col1row2 = 5.f;
  const float col2row2 = 6.f;
  const float col3row2 = 7.f;
  const float col4row2 = 8.f;
  const float col1row3 = 9.f;
  const float col2row3 = 10.f;
  const float col3row3 = 11.f;
  const float col4row3 = 12.f;
  const float col1row4 = 13.f;
  const float col2row4 = 14.f;
  const float col3row4 = 15.f;
  const float col4row4 = 16.f;
  gfx::Transform input(col1row1, col2row1, col3row1, col4row1, col1row2,
                       col2row2, col3row2, col4row2, col1row3, col2row3,
                       col3row3, col4row3, col1row4, col2row4, col3row4,
                       col4row4);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::Transform output;
  proxy->EchoTransform(input, &output);
  EXPECT_EQ(col1row1, output.matrix().get(0, 0));
  EXPECT_EQ(col2row1, output.matrix().get(0, 1));
  EXPECT_EQ(col3row1, output.matrix().get(0, 2));
  EXPECT_EQ(col4row1, output.matrix().get(0, 3));
  EXPECT_EQ(col1row2, output.matrix().get(1, 0));
  EXPECT_EQ(col2row2, output.matrix().get(1, 1));
  EXPECT_EQ(col3row2, output.matrix().get(1, 2));
  EXPECT_EQ(col4row2, output.matrix().get(1, 3));
  EXPECT_EQ(col1row3, output.matrix().get(2, 0));
  EXPECT_EQ(col2row3, output.matrix().get(2, 1));
  EXPECT_EQ(col3row3, output.matrix().get(2, 2));
  EXPECT_EQ(col4row3, output.matrix().get(2, 3));
  EXPECT_EQ(col1row4, output.matrix().get(3, 0));
  EXPECT_EQ(col2row4, output.matrix().get(3, 1));
  EXPECT_EQ(col3row4, output.matrix().get(3, 2));
  EXPECT_EQ(col4row4, output.matrix().get(3, 3));
}

}  // namespace gfx
