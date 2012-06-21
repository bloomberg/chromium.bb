// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/mac/lib/rlz_value_store_mac.h"

#include "base/mac/foundation_util.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "rlz/lib/assert.h"
#include "rlz/lib/lib_values.h"
#include "rlz/lib/rlz_lib.h"

#import <Foundation/Foundation.h>
#include <pthread.h>

using base::mac::ObjCCast;

namespace rlz_lib {

// These are written to disk and should not be changed.
NSString* const kPingTimeKey = @"pingTime";
NSString* const kAccessPointKey = @"accessPoints";
NSString* const kProductEventKey = @"productEvents";
NSString* const kStatefulEventKey = @"statefulEvents";

namespace {

NSString* GetNSProductName(Product product) {
  return base::SysUTF8ToNSString(GetProductName(product));
}

NSString* GetNSAccessPointName(AccessPoint p) {
  return base::SysUTF8ToNSString(GetAccessPointName(p));
}

// Retrieves a subdictionary in |p| for key |k|, creating it if necessary.
// If the dictionary contains an object for |k| that is not a mutable
// dictionary, that object is replaced with an empty mutable dictinary.
NSMutableDictionary* GetOrCreateDict(
    NSMutableDictionary* p, NSString* k) {
  NSMutableDictionary* d = ObjCCast<NSMutableDictionary>([p objectForKey:k]);
  if (!d) {
    d = [NSMutableDictionary dictionaryWithCapacity:0];
    [p setObject:d forKey:k];
  }
  return d;
}

}  // namespace

RlzValueStoreMac::RlzValueStoreMac(NSMutableDictionary* dict,
                                   NSString* plist_path)
  : dict_([dict retain]), plist_path_([plist_path retain]) {
}

RlzValueStoreMac::~RlzValueStoreMac() {
}

bool RlzValueStoreMac::HasAccess(AccessType type) {
  NSFileManager* manager = [NSFileManager defaultManager];
  switch (type) {
    case kReadAccess:  return [manager isReadableFileAtPath:plist_path_];
    case kWriteAccess: return [manager isWritableFileAtPath:plist_path_];
  }
}

bool RlzValueStoreMac::WritePingTime(Product product, int64 time) {
  NSNumber* n = [NSNumber numberWithLongLong:time];
  [ProductDict(product) setObject:n forKey:kPingTimeKey];
  return true;
}

bool RlzValueStoreMac::ReadPingTime(Product product, int64* time) {
  if (NSNumber* n =
      ObjCCast<NSNumber>([ProductDict(product) objectForKey:kPingTimeKey])) {
    *time = [n longLongValue];
    return true;
  }
  return false;
}

bool RlzValueStoreMac::ClearPingTime(Product product) {
  [ProductDict(product) removeObjectForKey:kPingTimeKey];
  return true;
}


bool RlzValueStoreMac::WriteAccessPointRlz(AccessPoint access_point,
                                           const char* new_rlz) {
  NSMutableDictionary* d = GetOrCreateDict(WorkingDict(), kAccessPointKey);
  [d setObject:base::SysUTF8ToNSString(new_rlz)
      forKey:GetNSAccessPointName(access_point)];
  return true;
}

bool RlzValueStoreMac::ReadAccessPointRlz(AccessPoint access_point,
                                          char* rlz,
                                          size_t rlz_size) {
  // Reading a non-existent access point counts as success.
  if (NSDictionary* d = ObjCCast<NSDictionary>(
        [WorkingDict() objectForKey:kAccessPointKey])) {
    NSString* val = ObjCCast<NSString>(
        [d objectForKey:GetNSAccessPointName(access_point)]);
    if (!val) {
      if (rlz_size > 0)
        rlz[0] = '\0';
      return true;
    }

    std::string s = base::SysNSStringToUTF8(val);
    if (s.size() >= rlz_size) {
      rlz[0] = 0;
      ASSERT_STRING("GetAccessPointRlz: Insufficient buffer size");
      return false;
    }
    strncpy(rlz, s.c_str(), rlz_size);
    return true;
  }
  if (rlz_size > 0)
    rlz[0] = '\0';
  return true;
}

bool RlzValueStoreMac::ClearAccessPointRlz(AccessPoint access_point) {
  if (NSMutableDictionary* d = ObjCCast<NSMutableDictionary>(
      [WorkingDict() objectForKey:kAccessPointKey])) {
    [d removeObjectForKey:GetNSAccessPointName(access_point)];
  }
  return true;
}


bool RlzValueStoreMac::AddProductEvent(Product product,
                                       const char* event_rlz) {
  [GetOrCreateDict(ProductDict(product), kProductEventKey)
      setObject:[NSNumber numberWithBool:YES]
      forKey:base::SysUTF8ToNSString(event_rlz)];
  return true;
}

bool RlzValueStoreMac::ReadProductEvents(Product product,
                                         std::vector<std::string>* events) {
  if (NSDictionary* d = ObjCCast<NSDictionary>(
      [ProductDict(product) objectForKey:kProductEventKey])) {
    for (NSString* s in d)
      events->push_back(base::SysNSStringToUTF8(s));
    return true;
  }
  return true;
}

bool RlzValueStoreMac::ClearProductEvent(Product product,
                                         const char* event_rlz) {
  if (NSMutableDictionary* d = ObjCCast<NSMutableDictionary>(
      [ProductDict(product) objectForKey:kProductEventKey])) {
    [d removeObjectForKey:base::SysUTF8ToNSString(event_rlz)];
    return true;
  }
  return false;
}

bool RlzValueStoreMac::ClearAllProductEvents(Product product) {
  [ProductDict(product) removeObjectForKey:kProductEventKey];
  return true;
}


bool RlzValueStoreMac::AddStatefulEvent(Product product,
                                        const char* event_rlz) {
  [GetOrCreateDict(ProductDict(product), kStatefulEventKey)
      setObject:[NSNumber numberWithBool:YES]
      forKey:base::SysUTF8ToNSString(event_rlz)];
  return true;
}

bool RlzValueStoreMac::IsStatefulEvent(Product product,
                                       const char* event_rlz) {
  if (NSDictionary* d = ObjCCast<NSDictionary>(
        [ProductDict(product) objectForKey:kStatefulEventKey])) {
    return [d objectForKey:base::SysUTF8ToNSString(event_rlz)] != nil;
  }
  return false;
}

bool RlzValueStoreMac::ClearAllStatefulEvents(Product product) {
  [ProductDict(product) removeObjectForKey:kStatefulEventKey];
  return true;
}


void RlzValueStoreMac::CollectGarbage() {
  NOTIMPLEMENTED();
}

NSDictionary* RlzValueStoreMac::dictionary() {
  return dict_.get();
}

NSMutableDictionary* RlzValueStoreMac::WorkingDict() {
  std::string brand(SupplementaryBranding::GetBrand());
  if (brand.empty())
    return dict_;

  NSString* brand_ns =
      [@"brand_" stringByAppendingString:base::SysUTF8ToNSString(brand)];

  return GetOrCreateDict(dict_.get(), brand_ns);
}

NSMutableDictionary* RlzValueStoreMac::ProductDict(Product p) {
  return GetOrCreateDict(WorkingDict(), GetNSProductName(p));
}


namespace {

// Creating a recursive cross-process mutex on windows is one line. On mac,
// there's no primitve for that, so this lock is emulated by an in-process
// mutex to get the recursive part, followed by a cross-process lock for the
// cross-process part.

// This is a struct so that it doesn't need a static initializer.
struct RecursiveCrossProcessLock {
  // Tries to acquire a recursive cross-process lock. Note that this _always_
  // acquires the in-process lock (if it wasn't already acquired). The parent
  // directory of |lock_file| must exist.
  bool TryGetCrossProcessLock(NSString* lock_filename);

  // Releases the lock. Should always be called, even if
  // TryGetCrossProcessLock() returns false.
  void ReleaseLock();

  pthread_mutex_t recursive_lock_;
  pthread_t locking_thread_;

  NSDistributedLock* file_lock_;
} g_recursive_lock = {
  // PTHREAD_RECURSIVE_MUTEX_INITIALIZER doesn't exist before 10.7 and is buggy
  // on 10.7 (http://gcc.gnu.org/bugzilla/show_bug.cgi?id=51906#c34), so emulate
  // recursive locking with a normal non-recursive mutex.
  PTHREAD_MUTEX_INITIALIZER
};

bool RecursiveCrossProcessLock::TryGetCrossProcessLock(
    NSString* lock_filename) {
  bool just_got_lock = false;

  // Emulate a recursive mutex with a non-recursive one.
  if (pthread_mutex_trylock(&recursive_lock_) == EBUSY) {
    if (pthread_equal(pthread_self(), locking_thread_) == 0) {
      // Some other thread has the lock, wait for it.
      pthread_mutex_lock(&recursive_lock_);
      CHECK(locking_thread_ == 0);
      just_got_lock = true;
    }
  } else {
    just_got_lock = true;
  }

  locking_thread_ = pthread_self();

  // Try to acquire file lock.
  if (just_got_lock) {
    const int kMaxTimeoutMS = 5000;  // Matches windows.
    const int kSleepPerTryMS = 200;

    CHECK(!file_lock_);
    file_lock_ = [[NSDistributedLock alloc] initWithPath:lock_filename];

    BOOL got_file_lock = NO;
    int elapsedMS = 0;
    while (!(got_file_lock = [file_lock_ tryLock]) &&
           elapsedMS < kMaxTimeoutMS) {
      usleep(kSleepPerTryMS * 1000);
      elapsedMS += kSleepPerTryMS;
    }

    if (!got_file_lock) {
      [file_lock_ release];
      file_lock_ = nil;
      return false;
    }
    return true;
  } else {
    return file_lock_ != nil;
  }
}

void RecursiveCrossProcessLock::ReleaseLock() {
  if (file_lock_) {
    [file_lock_ unlock];
    [file_lock_ release];
    file_lock_ = nil;
  }

  locking_thread_ = 0;
  pthread_mutex_unlock(&recursive_lock_);
}


// This is set during test execution, to write RLZ files into a temporary
// directory instead of the user's Application Support folder.
NSString* g_test_folder;

// RlzValueStoreMac keeps its data in memory and only writes it to disk when
// ScopedRlzValueStoreLock goes out of scope. Hence, if several
// ScopedRlzValueStoreLocks are nested, they all need to use the same store
// object.

// This counts the nesting depth.
int g_lock_depth = 0;

// This is the store object that might be shared. Only set if g_lock_depth > 0.
RlzValueStoreMac* g_store_object = NULL;


NSString* CreateRlzDirectory() {
  NSFileManager* manager = [NSFileManager defaultManager];
  NSArray* paths = NSSearchPathForDirectoriesInDomains(
      NSApplicationSupportDirectory, NSUserDomainMask, /*expandTilde=*/YES);
  NSString* folder = nil;
  if ([paths count] > 0)
    folder = ObjCCast<NSString>([paths objectAtIndex:0]);
  if (!folder)
    folder = [@"~/Library/Application Support" stringByStandardizingPath];
  folder = [folder stringByAppendingPathComponent:@"Google/RLZ"];

  if (g_test_folder)
    folder = [g_test_folder stringByAppendingPathComponent:folder];

  [manager createDirectoryAtPath:folder
     withIntermediateDirectories:YES
                      attributes:nil
                           error:nil];
  return folder;
}

// Returns the path of the rlz plist store, also creates the parent directory
// path if it doesn't exist.
NSString* RlzPlistFilename() {
  NSString* const kRlzFile = @"RlzStore.plist";
  return [CreateRlzDirectory() stringByAppendingPathComponent:kRlzFile];
}

// Returns the path of the rlz lock file, also creates the parent directory
// path if it doesn't exist.
NSString* RlzLockFilename() {
  NSString* const kRlzFile = @"lockfile";
  return [CreateRlzDirectory() stringByAppendingPathComponent:kRlzFile];
}

}  // namespace

ScopedRlzValueStoreLock::ScopedRlzValueStoreLock() {
  bool got_distributed_lock =
      g_recursive_lock.TryGetCrossProcessLock(RlzLockFilename());
  // At this point, we hold the in-process lock, no matter the value of
  // |got_distributed_lock|.

  ++g_lock_depth;

  if (!got_distributed_lock) {
    // Give up. |store_| isn't set, which signals to callers that acquiring
    // the lock failed. |g_recursive_lock| will be released by the
    // destructor.
    CHECK(!g_store_object);
    return;
  }

  if (g_lock_depth > 1) {
    // Reuse the already existing store object.
    CHECK(g_store_object);
    store_.reset(g_store_object);
    return;
  }

  CHECK(!g_store_object);

  NSString* plist = RlzPlistFilename();

  // Create an empty file if none exists yet.
  NSFileManager* manager = [NSFileManager defaultManager];
  if (![manager fileExistsAtPath:plist isDirectory:NULL])
    [[NSDictionary dictionary] writeToFile:plist atomically:YES];

  NSMutableDictionary* dict =
      [NSMutableDictionary dictionaryWithContentsOfFile:plist];
  VERIFY(dict);

  if (dict) {
    store_.reset(new RlzValueStoreMac(dict, plist));
    g_store_object = (RlzValueStoreMac*)store_.get();
  }
}

ScopedRlzValueStoreLock::~ScopedRlzValueStoreLock() {
  --g_lock_depth;
  CHECK(g_lock_depth >= 0);

  if (g_lock_depth > 0) {
    // Other locks are still using store_, don't free it yet.
    ignore_result(store_.release());
    return;
  }

  if (store_.get()) {
    g_store_object = NULL;

    NSDictionary* dict =
        static_cast<RlzValueStoreMac*>(store_.get())->dictionary();
    VERIFY([dict writeToFile:RlzPlistFilename() atomically:YES]);
  }

  // Check that "store_ set" => "file_lock acquired". The converse isn't true,
  // for example if the rlz data file can't be read.
  if (store_.get())
    CHECK(g_recursive_lock.file_lock_);
  if (!g_recursive_lock.file_lock_)
    CHECK(!store_.get());

  g_recursive_lock.ReleaseLock();
}

RlzValueStore* ScopedRlzValueStoreLock::GetStore() {
  return store_.get();
}

namespace testing {

void SetRlzStoreDirectory(const FilePath& directory) {
  base::mac::ScopedNSAutoreleasePool pool;

  [g_test_folder release];
  if (directory.empty()) {
    g_test_folder = nil;
  } else {
    // Not Unsafe on OS X.
    g_test_folder =
      [[NSString alloc] initWithUTF8String:directory.AsUTF8Unsafe().c_str()];
  }
}

}  // namespace testing

}  // namespace rlz_lib
