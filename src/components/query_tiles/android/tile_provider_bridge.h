// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_ANDROID_TILE_PROVIDER_BRIDGE_H_
#define COMPONENTS_QUERY_TILES_ANDROID_TILE_PROVIDER_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/supports_user_data.h"
#include "components/query_tiles/tile_service.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace query_tiles {

// Helper class responsible for bridging the TileProvider between C++ and Java.
class TileProviderBridge : public base::SupportsUserData::Data {
 public:
  // Returns a Java TileProviderBridge for |tile_service|.  There will
  // be only one bridge per TileProviderBridge.
  static ScopedJavaLocalRef<jobject> GetBridgeForTileService(
      TileService* tile_service);

  explicit TileProviderBridge(TileService* tile_service);
  ~TileProviderBridge() override;

  // Methods called from Java via JNI.
  void GetQueryTiles(JNIEnv* env,
                     const JavaParamRef<jobject>& jcaller,
                     const JavaParamRef<jobject>& jcallback);

 private:
  // A reference to the Java counterpart of this class.  See
  // TileProviderBridge.java.
  ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned.
  TileService* tile_service_;

  DISALLOW_COPY_AND_ASSIGN(TileProviderBridge);
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_ANDROID_TILE_PROVIDER_BRIDGE_H_
