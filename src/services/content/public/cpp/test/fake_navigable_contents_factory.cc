// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/public/cpp/test/fake_navigable_contents_factory.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "services/content/public/cpp/test/fake_navigable_contents.h"
#include "services/content/public/mojom/navigable_contents.mojom.h"

namespace content {

FakeNavigableContentsFactory::FakeNavigableContentsFactory() = default;

FakeNavigableContentsFactory::~FakeNavigableContentsFactory() = default;

void FakeNavigableContentsFactory::BindRequest(
    mojom::NavigableContentsFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void FakeNavigableContentsFactory::WaitForAndBindNextContentsRequest(
    FakeNavigableContents* contents) {
  base::RunLoop loop;
  next_create_contents_callback_ = base::BindLambdaForTesting(
      [&loop, contents](mojom::NavigableContentsRequest request,
                        mojom::NavigableContentsClientPtr client) {
        contents->Bind(std::move(request), std::move(client));
        loop.Quit();
      });
  loop.Run();
}

void FakeNavigableContentsFactory::CreateContents(
    mojom::NavigableContentsParamsPtr params,
    mojom::NavigableContentsRequest request,
    mojom::NavigableContentsClientPtr client) {
  if (!next_create_contents_callback_) {
    LOG(ERROR) << "Dropping unexpected CreateContents() request.";
    return;
  }

  std::move(next_create_contents_callback_)
      .Run(std::move(request), std::move(client));
}

}  // namespace content
