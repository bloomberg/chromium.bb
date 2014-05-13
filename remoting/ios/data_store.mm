// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/data_store.h"

@interface DataStore (Private)
- (NSString*)itemArchivePath;
@end

@implementation DataStore {
 @private
  NSMutableArray* _allHosts;
  NSManagedObjectContext* _context;
  NSManagedObjectModel* _model;
}

// Create or Get a static data store
+ (DataStore*)sharedStore {
  static DataStore* sharedStore = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken,
                ^{ sharedStore = [[super allocWithZone:nil] init]; });

  return sharedStore;
}

// General methods
+ (id)allocWithZone:(NSZone*)zone {
  return [self sharedStore];
}

// Load data store from SQLLite backing store
- (id)init {
  self = [super init];

  if (self) {
    // Read in ChromotingModel.xdatamodeld
    _model = [NSManagedObjectModel mergedModelFromBundles:nil];

    NSPersistentStoreCoordinator* psc = [[NSPersistentStoreCoordinator alloc]
        initWithManagedObjectModel:_model];

    NSString* path = [self itemArchivePath];
    NSURL* storeUrl = [NSURL fileURLWithPath:path];

    NSError* error = nil;

    NSDictionary* tryOptions = @{
      NSMigratePersistentStoresAutomaticallyOption : @YES,
      NSInferMappingModelAutomaticallyOption : @YES
    };
    NSDictionary* makeOptions =
        @{NSMigratePersistentStoresAutomaticallyOption : @YES};

    if (![psc addPersistentStoreWithType:NSSQLiteStoreType
                           configuration:nil
                                     URL:storeUrl
                                 options:tryOptions
                                   error:&error]) {
      // An incompatible version of the store exists, delete it and start over
      [[NSFileManager defaultManager] removeItemAtURL:storeUrl error:nil];

      [psc addPersistentStoreWithType:NSSQLiteStoreType
                        configuration:nil
                                  URL:storeUrl
                              options:makeOptions
                                error:&error];
      [NSException raise:@"Open failed"
                  format:@"Reason: %@", [error localizedDescription]];
    }

    // Create the managed object context
    _context = [[NSManagedObjectContext alloc] init];
    [_context setPersistentStoreCoordinator:psc];

    // The managed object context can manage undo, but we don't need it
    [_context setUndoManager:nil];

    _allHosts = nil;
  }
  return self;
}

// Committing to backing store
- (BOOL)saveChanges {
  NSError* err = nil;
  BOOL successful = [_context save:&err];
  return successful;
}

// Looking up the backing store path
- (NSString*)itemArchivePath {
  NSArray* documentDirectories = NSSearchPathForDirectoriesInDomains(
      NSDocumentDirectory, NSUserDomainMask, YES);

  // Get one and only document directory from that list
  NSString* documentDirectory = [documentDirectories objectAtIndex:0];

  return [documentDirectory stringByAppendingPathComponent:@"store.data"];
}

// Return an array of all known hosts, if the list hasn't been loaded yet, then
// load it now
- (NSArray*)allHosts {
  if (!_allHosts) {
    NSFetchRequest* request = [[NSFetchRequest alloc] init];

    NSEntityDescription* e =
        [[_model entitiesByName] objectForKey:@"HostPreferences"];

    [request setEntity:e];

    NSError* error;
    NSArray* result = [_context executeFetchRequest:request error:&error];
    if (!result) {
      [NSException raise:@"Fetch failed"
                  format:@"Reason: %@", [error localizedDescription]];
    }
    _allHosts = [result mutableCopy];
  }

  return _allHosts;
}

// Return a HostPreferences if it already exists, otherwise create a new
// HostPreferences to use
- (const HostPreferences*)createHost:(NSString*)hostId {

  const HostPreferences* p = [self getHostForId:hostId];

  if (p == nil) {
    p = [NSEntityDescription insertNewObjectForEntityForName:@"HostPreferences"
                                      inManagedObjectContext:_context];
    p.hostId = hostId;
    [_allHosts addObject:p];
  }
  return p;
}

- (void)removeHost:(HostPreferences*)p {
  [_context deleteObject:p];
  [_allHosts removeObjectIdenticalTo:p];
}

// Search the store for any matching HostPreferences
// return the 1st match or nil
- (const HostPreferences*)getHostForId:(NSString*)hostId {
  NSFetchRequest* request = [[NSFetchRequest alloc] init];

  NSEntityDescription* e =
      [[_model entitiesByName] objectForKey:@"HostPreferences"];
  [request setEntity:e];

  NSPredicate* predicate =
      [NSPredicate predicateWithFormat:@"(hostId = %@)", hostId];
  [request setPredicate:predicate];

  NSError* error;
  NSArray* result = [_context executeFetchRequest:request error:&error];
  if (!result) {
    [NSException raise:@"Fetch failed"
                format:@"Reason: %@", [error localizedDescription]];
  }

  for (HostPreferences* curHost in result) {
    return curHost;
  }
  return nil;
}

@end
