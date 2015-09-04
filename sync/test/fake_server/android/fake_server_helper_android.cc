// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/android/fake_server_helper_android.h"

#include <jni.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "jni/FakeServerHelper_jni.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/network_resources.h"
#include "sync/protocol/sync.pb.h"
#include "sync/test/fake_server/bookmark_entity_builder.h"
#include "sync/test/fake_server/entity_builder_factory.h"
#include "sync/test/fake_server/fake_server.h"
#include "sync/test/fake_server/fake_server_network_resources.h"
#include "sync/test/fake_server/fake_server_verifier.h"
#include "sync/test/fake_server/tombstone_entity.h"
#include "sync/test/fake_server/unique_client_entity.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

FakeServerHelperAndroid::FakeServerHelperAndroid(JNIEnv* env, jobject obj) {
}

FakeServerHelperAndroid::~FakeServerHelperAndroid() { }

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  FakeServerHelperAndroid* fake_server_android =
      new FakeServerHelperAndroid(env, obj);
  return reinterpret_cast<intptr_t>(fake_server_android);
}

jlong FakeServerHelperAndroid::CreateFakeServer(JNIEnv* env, jobject obj) {
  fake_server::FakeServer* fake_server = new fake_server::FakeServer();
  return reinterpret_cast<intptr_t>(fake_server);
}

jlong FakeServerHelperAndroid::CreateNetworkResources(JNIEnv* env,
                                                      jobject obj,
                                                      jlong fake_server) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  syncer::NetworkResources* resources =
      new fake_server::FakeServerNetworkResources(fake_server_ptr->AsWeakPtr());
  return reinterpret_cast<intptr_t>(resources);
}

void FakeServerHelperAndroid::DeleteFakeServer(JNIEnv* env,
                                               jobject obj,
                                               jlong fake_server) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  delete fake_server_ptr;
}

jboolean FakeServerHelperAndroid::VerifyEntityCountByTypeAndName(
    JNIEnv* env,
    jobject obj,
    jlong fake_server,
    jlong count,
    jint model_type_int,
    jstring name) {
  syncer::ModelType model_type = static_cast<syncer::ModelType>(model_type_int);
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server::FakeServerVerifier fake_server_verifier(fake_server_ptr);
  testing::AssertionResult result =
      fake_server_verifier.VerifyEntityCountByTypeAndName(
          count, model_type, base::android::ConvertJavaStringToUTF8(env, name));

  if (!result)
    LOG(WARNING) << result.message();

  return result;
}

jboolean FakeServerHelperAndroid::VerifySessions(
    JNIEnv* env,
    jobject obj,
    jlong fake_server,
    jobjectArray url_array) {
  std::multiset<std::string> tab_urls;
  for (int i = 0; i < env->GetArrayLength(url_array); i++) {
    jstring s = (jstring) env->GetObjectArrayElement(url_array, i);
    tab_urls.insert(base::android::ConvertJavaStringToUTF8(env, s));
  }
  fake_server::SessionsHierarchy expected_sessions;
  expected_sessions.AddWindow(tab_urls);

  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server::FakeServerVerifier fake_server_verifier(fake_server_ptr);
  testing::AssertionResult result =
      fake_server_verifier.VerifySessions(expected_sessions);

  if (!result)
    LOG(WARNING) << result.message();

  return result;
}

base::android::ScopedJavaLocalRef<jobjectArray>
FakeServerHelperAndroid::GetSyncEntitiesByModelType(JNIEnv* env,
                                                    jobject obj,
                                                    jlong fake_server,
                                                    jint model_type_int) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  syncer::ModelType model_type = static_cast<syncer::ModelType>(model_type_int);

  std::vector<sync_pb::SyncEntity> entities =
      fake_server_ptr->GetSyncEntitiesByModelType(model_type);

  std::vector<std::string> entity_strings;
  for (size_t i = 0; i < entities.size(); ++i) {
    std::string s;
    entities[i].SerializeToString(&s);
    entity_strings.push_back(s);
  }
  return base::android::ToJavaArrayOfByteArray(env, entity_strings);
}

void FakeServerHelperAndroid::InjectUniqueClientEntity(
    JNIEnv* env,
    jobject obj,
    jlong fake_server,
    jstring name,
    jbyteArray serialized_entity_specifics) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  sync_pb::EntitySpecifics entity_specifics;
  DeserializeEntitySpecifics(env, serialized_entity_specifics,
                             entity_specifics);

  fake_server_ptr->InjectEntity(
      fake_server::UniqueClientEntity::CreateForInjection(
          base::android::ConvertJavaStringToUTF8(env, name), entity_specifics));
}

void FakeServerHelperAndroid::ModifyEntitySpecifics(
    JNIEnv* env,
    jobject obj,
    jlong fake_server,
    jstring id,
    jbyteArray serialized_entity_specifics) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  sync_pb::EntitySpecifics entity_specifics;
  DeserializeEntitySpecifics(env, serialized_entity_specifics,
                             entity_specifics);

  fake_server_ptr->ModifyEntitySpecifics(
      base::android::ConvertJavaStringToUTF8(env, id), entity_specifics);
}

void FakeServerHelperAndroid::DeserializeEntitySpecifics(
    JNIEnv* env,
    jbyteArray serialized_entity_specifics,
    sync_pb::EntitySpecifics& entity_specifics) {
  int specifics_bytes_length = env->GetArrayLength(serialized_entity_specifics);
  jbyte* specifics_bytes =
      env->GetByteArrayElements(serialized_entity_specifics, NULL);
  std::string specifics_string(reinterpret_cast<char *>(specifics_bytes),
                               specifics_bytes_length);

  if (!entity_specifics.ParseFromString(specifics_string))
    NOTREACHED() << "Could not deserialize EntitySpecifics";
}

void FakeServerHelperAndroid::InjectBookmarkEntity(
    JNIEnv* env,
    jobject obj,
    jlong fake_server,
    jstring title,
    jstring url,
    jstring parent_id) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server_ptr->InjectEntity(
      CreateBookmarkEntity(env, title, url, parent_id));
}

void FakeServerHelperAndroid::InjectBookmarkFolderEntity(JNIEnv* env,
                                                         jobject obj,
                                                         jlong fake_server,
                                                         jstring title,
                                                         jstring parent_id) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          base::android::ConvertJavaStringToUTF8(env, title));
  bookmark_builder.SetParentId(
      base::android::ConvertJavaStringToUTF8(env, parent_id));

  fake_server_ptr->InjectEntity(bookmark_builder.BuildFolder());
}

void FakeServerHelperAndroid::ModifyBookmarkEntity(JNIEnv* env,
                                                   jobject obj,
                                                   jlong fake_server,
                                                   jstring entity_id,
                                                   jstring title,
                                                   jstring url,
                                                   jstring parent_id) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  scoped_ptr<fake_server::FakeServerEntity> bookmark =
      CreateBookmarkEntity(env, title, url, parent_id);
  sync_pb::SyncEntity proto;
  bookmark->SerializeAsProto(&proto);
  fake_server_ptr->ModifyBookmarkEntity(
      base::android::ConvertJavaStringToUTF8(env, entity_id),
      base::android::ConvertJavaStringToUTF8(env, parent_id),
      proto.specifics());
}

void FakeServerHelperAndroid::ModifyBookmarkFolderEntity(JNIEnv* env,
                                                         jobject obj,
                                                         jlong fake_server,
                                                         jstring entity_id,
                                                         jstring title,
                                                         jstring parent_id) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          base::android::ConvertJavaStringToUTF8(env, title));
  bookmark_builder.SetParentId(
      base::android::ConvertJavaStringToUTF8(env, parent_id));

  sync_pb::SyncEntity proto;
  bookmark_builder.BuildFolder()->SerializeAsProto(&proto);
  fake_server_ptr->ModifyBookmarkEntity(
      base::android::ConvertJavaStringToUTF8(env, entity_id),
      base::android::ConvertJavaStringToUTF8(env, parent_id),
      proto.specifics());
}

scoped_ptr<fake_server::FakeServerEntity>
FakeServerHelperAndroid::CreateBookmarkEntity(JNIEnv* env,
                                              jstring title,
                                              jstring url,
                                              jstring parent_id) {
  std::string url_as_string = base::android::ConvertJavaStringToUTF8(env, url);
  GURL gurl = GURL(url_as_string);
  if (!gurl.is_valid()) {
    NOTREACHED() << "The given string (" << url_as_string
                 << ") is not a valid URL.";
  }

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          base::android::ConvertJavaStringToUTF8(env, title));
  bookmark_builder.SetParentId(
          base::android::ConvertJavaStringToUTF8(env, parent_id));
  return bookmark_builder.BuildBookmark(gurl);
}

base::android::ScopedJavaLocalRef<jstring>
FakeServerHelperAndroid::GetBookmarkBarFolderId(
    JNIEnv* env,
    jobject obj,
    jlong fake_server) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  return base::android::ConvertUTF8ToJavaString(
      env, fake_server_ptr->GetBookmarkBarFolderId());
}

void FakeServerHelperAndroid::DeleteEntity(
    JNIEnv* env,
    jobject obj,
    jlong fake_server,
    jstring id) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  std::string native_id = base::android::ConvertJavaStringToUTF8(env, id);
  fake_server_ptr->InjectEntity(
      fake_server::TombstoneEntity::Create(native_id));
}

// static
bool FakeServerHelperAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
