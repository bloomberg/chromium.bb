// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_registry.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "storage/browser/test/fake_blob.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace storage {
namespace {

std::string UuidFromBlob(mojo::PendingRemote<blink::mojom::Blob> pending_blob) {
  mojo::Remote<blink::mojom::Blob> blob(std::move(pending_blob));

  base::RunLoop loop;
  std::string received_uuid;
  blob->GetInternalUUID(base::BindOnce(
      [](base::OnceClosure quit_closure, std::string* uuid_out,
         const std::string& uuid) {
        *uuid_out = uuid;
        std::move(quit_closure).Run();
      },
      loop.QuitClosure(), &received_uuid));
  loop.Run();
  return received_uuid;
}

TEST(BlobUrlRegistry, URLRegistration) {
  const std::string kBlobId1 = "Blob1";
  const std::string kType = "type1";
  const std::string kDisposition = "disp1";
  const std::string kBlobId2 = "Blob2";
  const GURL kURL = GURL("blob://Blob1");
  const GURL kURL2 = GURL("blob://Blob2");

  base::test::SingleThreadTaskEnvironment task_environment_;

  FakeBlob blob1(kBlobId1);
  FakeBlob blob2(kBlobId2);

  BlobUrlRegistry registry;
  EXPECT_FALSE(registry.IsUrlMapped(kURL));
  EXPECT_FALSE(registry.GetBlobFromUrl(kURL));
  EXPECT_FALSE(registry.RemoveUrlMapping(kURL));
  EXPECT_EQ(0u, registry.url_count());

  EXPECT_TRUE(registry.AddUrlMapping(kURL, blob1.Clone()));
  EXPECT_FALSE(registry.AddUrlMapping(kURL, blob2.Clone()));

  EXPECT_TRUE(registry.IsUrlMapped(kURL));
  EXPECT_EQ(kBlobId1, UuidFromBlob(registry.GetBlobFromUrl(kURL)));
  EXPECT_EQ(1u, registry.url_count());

  EXPECT_TRUE(registry.AddUrlMapping(kURL2, blob2.Clone()));
  EXPECT_EQ(2u, registry.url_count());
  EXPECT_TRUE(registry.RemoveUrlMapping(kURL2));
  EXPECT_FALSE(registry.IsUrlMapped(kURL2));

  // Both urls point to the same blob.
  EXPECT_TRUE(registry.AddUrlMapping(kURL2, blob1.Clone()));
  EXPECT_EQ(UuidFromBlob(registry.GetBlobFromUrl(kURL)),
            UuidFromBlob(registry.GetBlobFromUrl(kURL2)));
}

}  // namespace
}  // namespace storage
