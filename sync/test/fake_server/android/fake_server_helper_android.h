// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_ANDROID_FAKE_SERVER_HELPER_ANDROID
#define SYNC_TEST_FAKE_SERVER_ANDROID_FAKE_SERVER_HELPER_ANDROID

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "sync/test/fake_server/entity_builder_factory.h"

// Helper for utilizing native FakeServer infrastructure in Android tests.
class FakeServerHelperAndroid {
 public:
  // Registers the native methods.
  static bool Register(JNIEnv* env);

  // Creates a FakeServerHelperAndroid.
  FakeServerHelperAndroid(JNIEnv* env, jobject obj);

  // Factory method for creating a native FakeServer object. The caller assumes
  // ownership.
  jlong CreateFakeServer(JNIEnv* env, jobject obj);

  // Factory method for creating a native NetworkResources object. The caller
  // assumes ownership.
  jlong CreateNetworkResources(JNIEnv* env, jobject obj, jlong fake_server);

  // Deletes the given |fake_server| (a FakeServer pointer created via
  // CreateFakeServer).
  void DeleteFakeServer(JNIEnv* env, jobject obj, jlong fake_server);

  // Returns true if and only if |fake_server| contains |count| entities that
  // match |model_type_string| and |name|.
  jboolean VerifyEntityCountByTypeAndName(JNIEnv* env,
                                          jobject obj,
                                          jlong fake_server,
                                          jlong count,
                                          jint model_type_int,
                                          jstring name);

  // Returns true iff |fake_server| has exactly one window of sessions with
  // tabs matching |url_array|. The order of the array does not matter.
  jboolean VerifySessions(JNIEnv* env,
                          jobject obj,
                          jlong fake_server,
                          jobjectArray url_array);

  // Return the entities for |model_type_string| on |fake_server|.
  base::android::ScopedJavaLocalRef<jobjectArray> GetSyncEntitiesByModelType(
      JNIEnv* env,
      jobject obj,
      jlong fake_server,
      jint model_type_int);

  // Injects a UniqueClientEntity into |fake_server|.
  void InjectUniqueClientEntity(JNIEnv* env,
                                jobject obj,
                                jlong fake_server,
                                jstring name,
                                jbyteArray serialized_entity_specifics);

  // Modifies the entity with |id| on |fake_server|.
  void ModifyEntitySpecifics(JNIEnv* env,
                             jobject obj,
                             jlong fake_server,
                             jstring name,
                             jbyteArray serialized_entity_specifics);

  // Injects a BookmarkEntity into |fake_server|.
  void InjectBookmarkEntity(JNIEnv* env,
                            jobject obj,
                            jlong fake_server,
                            jstring title,
                            jstring url,
                            jstring parent_id);

  // Injects a bookmark folder entity into |fake_server|.
  void InjectBookmarkFolderEntity(JNIEnv* env,
                                  jobject obj,
                                  jlong fake_server,
                                  jstring title,
                                  jstring parent_id);

  // Modify the BookmarkEntity with |entity_id| on |fake_server|.
  void ModifyBookmarkEntity(JNIEnv* env,
                            jobject obj,
                            jlong fake_server,
                            jstring entity_id,
                            jstring title,
                            jstring url,
                            jstring parent_id);

  // Modify the bookmark folder with |entity_id| on |fake_server|.
  void ModifyBookmarkFolderEntity(JNIEnv* env,
                                  jobject obj,
                                  jlong fake_server,
                                  jstring entity_id,
                                  jstring title,
                                  jstring parent_id);

  // Returns the bookmark bar folder ID.
  base::android::ScopedJavaLocalRef<jstring> GetBookmarkBarFolderId(
      JNIEnv* env,
      jobject obj,
      jlong fake_server);

  // Deletes an entity on the server. This is the JNI way of injecting a
  // tombstone.
  void DeleteEntity(JNIEnv* env,
                    jobject obj,
                    jlong fake_server,
                    jstring id);

 private:
  virtual ~FakeServerHelperAndroid();

  // Deserializes |serialized_entity_specifics| into |entity_specifics|.
  void DeserializeEntitySpecifics(JNIEnv* env,
                                  jbyteArray serialized_entity_specifics,
                                  sync_pb::EntitySpecifics& entity_specifics);

  // Creates a bookmark entity.
  scoped_ptr<fake_server::FakeServerEntity> CreateBookmarkEntity(
      JNIEnv* env,
      jstring title,
      jstring url,
      jstring parent_id);
};

#endif  // SYNC_TEST_FAKE_SERVER_ANDROID_FAKE_SERVER_HELPER_ANDROID
