// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadSafeScriptContainer_h
#define ThreadSafeScriptContainer_h

#include <memory>

#include "modules/ModulesExport.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KURLHash.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/ThreadSafeRefCounted.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebVector.h"

namespace blink {

// ThreadSafeScriptContainer stores the scripts of a service worker for
// startup. This container is created for each service worker. The IO thread
// adds scripts to the container, and the worker thread takes the scripts.
//
// This class uses explicit synchronization because it needs to support
// synchronous importScripts() from the worker thread.
//
// This class is ThreadSafeRefCounted because there is no ordering guarantee of
// lifetime of its owners, i.e. ServiceWorkerInstalledScriptsManager and its
// Internal class. ServiceWorkerInstalledScriptsManager is destroyed earlier
// than Internal if the worker is terminated before all scripts are streamed,
// and Internal is destroyed earlier if all of scripts are received before
// finishing script evaluation.
class MODULES_EXPORT ThreadSafeScriptContainer final
    : public WTF::ThreadSafeRefCounted<ThreadSafeScriptContainer> {
 public:
  // TODO(leonhsl): Eliminate RawScriptData struct and use
  // InstalledScriptsManager::ScriptData directly.
  using BytesChunk = WebVector<char>;

  // Container of a script. All the fields of this class need to be
  // cross-thread-transfer-safe.
  class MODULES_EXPORT RawScriptData {
   public:
    static std::unique_ptr<RawScriptData> Create(
        const WTF::String& encoding,
        WTF::Vector<BytesChunk> script_text,
        WTF::Vector<BytesChunk> meta_data);
    static std::unique_ptr<RawScriptData> CreateInvalidInstance();

    ~RawScriptData();

    void AddHeader(const WTF::String& key, const WTF::String& value);

    // Returns false if it fails to receive the script from the browser.
    bool IsValid() const { return is_valid_; }
    // The encoding of the script text.
    const WTF::String& Encoding() const {
      DCHECK(is_valid_);
      return encoding_;
    }
    // An array of raw byte chunks of the script text.
    const WTF::Vector<BytesChunk>& ScriptTextChunks() const {
      DCHECK(is_valid_);
      return script_text_;
    }
    // An array of raw byte chunks of the scripts's meta data from the script's
    // V8 code cache.
    const WTF::Vector<BytesChunk>& MetaDataChunks() const {
      DCHECK(is_valid_);
      return meta_data_;
    }

    // The HTTP headers of the script.
    std::unique_ptr<CrossThreadHTTPHeaderMapData> TakeHeaders() {
      DCHECK(is_valid_);
      return std::move(headers_);
    }

   private:
    RawScriptData(const WTF::String& encoding,
                  WTF::Vector<BytesChunk> script_text,
                  WTF::Vector<BytesChunk> meta_data,
                  bool is_valid);
    const bool is_valid_;
    WTF::String encoding_;
    WTF::Vector<BytesChunk> script_text_;
    WTF::Vector<BytesChunk> meta_data_;
    std::unique_ptr<CrossThreadHTTPHeaderMapData> headers_;
  };

  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();
  ThreadSafeScriptContainer();

  enum class ScriptStatus {
    // The script data has been received.
    kReceived,
    // The script data has been received but it has already been taken.
    kTaken,
    // Receiving the script has failed.
    kFailed,
    // The script data has not been received yet.
    kPending
  };

  // Called on the IO thread.
  void AddOnIOThread(const KURL& script_url,
                     std::unique_ptr<RawScriptData> raw_data);

  // Called on the worker thread.
  ScriptStatus GetStatusOnWorkerThread(const KURL& script_url);

  // Removes the script. After calling this, ScriptStatus for the
  // script will be kPending.
  // Called on the worker thread.
  void ResetOnWorkerThread(const KURL& script_url);

  // Waits until the script is added. The thread is blocked until the script is
  // available or receiving the script fails. Returns false if an error happens
  // and the waiting script won't be available forever.
  // Called on the worker thread.
  bool WaitOnWorkerThread(const KURL& script_url);

  // Called on the worker thread.
  std::unique_ptr<RawScriptData> TakeOnWorkerThread(const KURL& script_url);

  // Called if no more data will be added.
  // Called on the IO thread.
  void OnAllDataAddedOnIOThread();

 private:
  friend class WTF::ThreadSafeRefCounted<ThreadSafeScriptContainer>;
  ~ThreadSafeScriptContainer();

  // |mutex_| protects |waiting_cv_|, |script_data_|, |waiting_url_| and
  // |are_all_data_added_|.
  WTF::Mutex mutex_;
  // |waiting_cv_| is signaled when a script whose url matches to |waiting_url|
  // is added, or OnAllDataAdded is called. The worker thread waits on this, and
  // the IO thread signals it.
  ThreadCondition waiting_cv_;
  WTF::HashMap<KURL, std::unique_ptr<RawScriptData>> script_data_;
  KURL waiting_url_;
  bool are_all_data_added_;
};

}  // namespace blink

#endif  // ThreadSafeScriptContainer_h
