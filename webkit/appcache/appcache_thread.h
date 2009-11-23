// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_THREAD_H_
#define WEBKIT_APPCACHE_APPCACHE_THREAD_H_

namespace tracked_objects {
class Location;
}
class Task;

namespace appcache {

// The appcache system uses two threads, an IO thread and a DB thread.
// It does not create these threads, the embedder is responsible for
// providing them to the appcache library by providing a concrete
// implementation of the PostTask and CurrentlyOn methods declared here,
// and by calling the InitIDs method prior to using the appcache library.
class AppCacheThread {
 public:
  static void InitIDs(int db, int io) {
    db_ = db;
    io_ = io;
  }
  static int db() { return db_; }
  static int io() { return io_; }

  static bool PostTask(int id,
                       const tracked_objects::Location& from_here,
                       Task* task);
  static bool CurrentlyOn(int id);

 private:
  AppCacheThread();
  ~AppCacheThread();

  static int db_;
  static int io_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_THREAD_H_
