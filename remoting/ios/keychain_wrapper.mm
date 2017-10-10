// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/keychain_wrapper.h"

#include "base/logging.h"

#import "remoting/ios/domain/host_info.h"

static const UInt8 kKeychainItemIdentifier[] = "org.chromium.RemoteDesktop\0";

NSString* const kPairingSecretSeperator = @"|";

NSString* const kKeychainPairingId = @"kKeychainPairingId";
NSString* const kKeychainPairingSecret = @"kKeychainPairingSecret";

@interface KeychainWrapper () {
  NSMutableDictionary* _keychainData;
  NSMutableDictionary* _userInfoQuery;
}
@end

@implementation KeychainWrapper

// KeychainWrapper is a singleton.
+ (KeychainWrapper*)instance {
  static KeychainWrapper* sharedInstance = nil;
  static dispatch_once_t guard;
  dispatch_once(&guard, ^{
    sharedInstance = [[KeychainWrapper alloc] init];
  });
  return sharedInstance;
}

- (id)init {
  if ((self = [super init])) {
    OSStatus keychainErr = noErr;
    _userInfoQuery = [[NSMutableDictionary alloc] init];
    [_userInfoQuery setObject:(__bridge id)kSecClassGenericPassword
                       forKey:(__bridge id)kSecClass];
    NSData* keychainItemID =
        [NSData dataWithBytes:kKeychainItemIdentifier
                       length:strlen((const char*)kKeychainItemIdentifier)];
    [_userInfoQuery setObject:keychainItemID
                       forKey:(__bridge id)kSecAttrService];
    [_userInfoQuery setObject:(__bridge id)kSecMatchLimitOne
                       forKey:(__bridge id)kSecMatchLimit];
    [_userInfoQuery setObject:(__bridge id)kCFBooleanTrue
                       forKey:(__bridge id)kSecReturnAttributes];

    // TODO(crbug.com/773503): Use ScopedCFTypeRef.
    CFMutableDictionaryRef outDictionary = nil;
    keychainErr = SecItemCopyMatching((__bridge CFDictionaryRef)_userInfoQuery,
                                      (CFTypeRef*)&outDictionary);
    if (keychainErr == noErr) {
      _keychainData = [self
          secItemFormatToDictionary:(__bridge_transfer NSMutableDictionary*)
                                        outDictionary];
    } else if (keychainErr == errSecItemNotFound) {
      [self resetKeychainItem];

      if (outDictionary) {
        CFRelease(outDictionary);
        _keychainData = nil;
      }
    } else {
      LOG(FATAL) << "Serious error: " << keychainErr;
      if (outDictionary) {
        CFRelease(outDictionary);
        _keychainData = nil;
      }
    }
  }
  return self;
}

#pragma mark - Public

- (void)setRefreshToken:(NSString*)refreshToken {
  [self setObject:refreshToken forKey:(__bridge id)kSecValueData];
}

- (NSString*)refreshToken {
  return [self objectForKey:(__bridge id)kSecValueData];
}

- (void)commitPairingCredentialsForHost:(NSString*)host
                                     id:(NSString*)pairingId
                                 secret:(NSString*)secret {
  NSString* keysString = [self objectForKey:(__bridge id)kSecAttrGeneric];
  NSMutableDictionary* keys = [self stringToMap:keysString];
  NSString* pairingIdAndSecret = [NSString
      stringWithFormat:@"%@%@%@", pairingId, kPairingSecretSeperator, secret];
  [keys setObject:pairingIdAndSecret forKey:host];
  [self setObject:[self mapToString:keys] forKey:(__bridge id)kSecAttrGeneric];
}

- (NSDictionary*)pairingCredentialsForHost:(NSString*)host {
  NSString* keysString = [self objectForKey:(__bridge id)kSecAttrGeneric];
  NSMutableDictionary* keys = [self stringToMap:keysString];
  NSString* pairingIdAndSecret = [keys objectForKey:host];
  if (!pairingIdAndSecret ||
      [pairingIdAndSecret rangeOfString:kPairingSecretSeperator].location ==
          NSNotFound) {
    return nil;
  }
  NSArray* components =
      [pairingIdAndSecret componentsSeparatedByString:kPairingSecretSeperator];
  DCHECK(components.count == 2);
  return @{
    kKeychainPairingId : components[0],
    kKeychainPairingSecret : components[1],
  };
}

#pragma mark - Map to String helpers

- (NSMutableDictionary*)stringToMap:(NSString*)mapString {
  NSError* err;

  if (mapString &&
      [mapString respondsToSelector:@selector(dataUsingEncoding:)]) {
    NSData* data = [mapString dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary* pairingMap;
    if (data) {
      pairingMap = (NSDictionary*)[NSJSONSerialization
          JSONObjectWithData:data
                     options:NSJSONReadingMutableContainers
                       error:&err];
    }
    if (!err) {
      return [NSMutableDictionary dictionaryWithDictionary:pairingMap];
    }
  }
  // failed to load a dictionary, make a new one.
  return [NSMutableDictionary dictionaryWithCapacity:1];
}

- (NSString*)mapToString:(NSDictionary*)map {
  if (map) {
    NSError* err;
    NSData* jsonData =
        [NSJSONSerialization dataWithJSONObject:map options:0 error:&err];
    if (!err) {
      return [[NSString alloc] initWithData:jsonData
                                   encoding:NSUTF8StringEncoding];
    }
  }
  // failed to convert the map, make nil string.
  return nil;
}

#pragma mark - Private

// Implement the mySetObject:forKey method, which writes attributes to the
// keychain:
- (void)setObject:(id)inObject forKey:(id)key {
  if (inObject == nil)
    return;
  id currentObject = [_keychainData objectForKey:key];
  if (![currentObject isEqual:inObject]) {
    [_keychainData setObject:inObject forKey:key];
    [self writeToKeychain];
  }
}

// Implement the myObjectForKey: method, which reads an attribute value from a
// dictionary:
- (id)objectForKey:(id)key {
  return [_keychainData objectForKey:key];
}

- (void)resetKeychainItem {
  if (!_keychainData) {
    _keychainData = [[NSMutableDictionary alloc] init];
  } else if (_keychainData) {
    NSMutableDictionary* tmpDictionary =
        [self dictionaryToSecItemFormat:_keychainData];
    OSStatus errorcode = SecItemDelete((__bridge CFDictionaryRef)tmpDictionary);
    if (errorcode == errSecItemNotFound) {
      // this is ok.
    } else if (errorcode != noErr) {
      LOG(FATAL) << "Problem deleting current keychain item. errorcode: "
                 << errorcode;
    }
  }

  [_keychainData setObject:@"gaia_refresh_token"
                    forKey:(__bridge id)kSecAttrLabel];
  [_keychainData setObject:@"Gaia fresh token"
                    forKey:(__bridge id)kSecAttrDescription];
  [_keychainData setObject:@"" forKey:(__bridge id)kSecValueData];
  [_keychainData setObject:@"" forKey:(__bridge id)kSecClass];
  [_keychainData setObject:@"" forKey:(__bridge id)kSecAttrGeneric];
}

- (NSMutableDictionary*)dictionaryToSecItemFormat:
    (NSDictionary*)dictionaryToConvert {
  NSMutableDictionary* returnDictionary =
      [NSMutableDictionary dictionaryWithDictionary:dictionaryToConvert];

  NSData* keychainItemID =
      [NSData dataWithBytes:kKeychainItemIdentifier
                     length:strlen((const char*)kKeychainItemIdentifier)];
  [returnDictionary setObject:keychainItemID
                       forKey:(__bridge id)kSecAttrService];
  [returnDictionary setObject:(__bridge id)kSecClassGenericPassword
                       forKey:(__bridge id)kSecClass];

  NSString* passwordString =
      [dictionaryToConvert objectForKey:(__bridge id)kSecValueData];
  [returnDictionary
      setObject:[passwordString dataUsingEncoding:NSUTF8StringEncoding]
         forKey:(__bridge id)kSecValueData];
  return returnDictionary;
}

- (NSMutableDictionary*)secItemFormatToDictionary:
    (NSDictionary*)dictionaryToConvert {
  NSMutableDictionary* returnDictionary =
      [NSMutableDictionary dictionaryWithDictionary:dictionaryToConvert];

  [returnDictionary setObject:(__bridge id)kCFBooleanTrue
                       forKey:(__bridge id)kSecReturnData];
  [returnDictionary setObject:(__bridge id)kSecClassGenericPassword
                       forKey:(__bridge id)kSecClass];

  CFDataRef passwordData = NULL;
  OSStatus keychainError = noErr;
  keychainError = SecItemCopyMatching(
      (__bridge CFDictionaryRef)returnDictionary, (CFTypeRef*)&passwordData);
  if (keychainError == noErr) {
    [returnDictionary removeObjectForKey:(__bridge id)kSecReturnData];

    NSString* password = [[NSString alloc]
        initWithBytes:[(__bridge_transfer NSData*)passwordData bytes]
               length:[(__bridge NSData*)passwordData length]
             encoding:NSUTF8StringEncoding];
    [returnDictionary setObject:password forKey:(__bridge id)kSecValueData];
  } else if (keychainError == errSecItemNotFound) {
    LOG(WARNING) << "Nothing was found in the keychain.";
    if (passwordData) {
      CFRelease(passwordData);
      passwordData = nil;
    }
  } else {
    LOG(FATAL) << "Serious error: " << keychainError;
    if (passwordData) {
      CFRelease(passwordData);
      passwordData = nil;
    }
  }
  return returnDictionary;
}

- (void)writeToKeychain {
  CFDictionaryRef attributes = nil;
  NSMutableDictionary* updateItem = nil;

  if (SecItemCopyMatching((__bridge CFDictionaryRef)_userInfoQuery,
                          (CFTypeRef*)&attributes) == noErr) {
    updateItem = [NSMutableDictionary
        dictionaryWithDictionary:(__bridge_transfer NSDictionary*)attributes];

    [updateItem setObject:[_userInfoQuery objectForKey:(__bridge id)kSecClass]
                   forKey:(__bridge id)kSecClass];

    NSMutableDictionary* tempCheck =
        [self dictionaryToSecItemFormat:_keychainData];
    [tempCheck removeObjectForKey:(__bridge id)kSecClass];

    OSStatus errorcode = SecItemUpdate((__bridge CFDictionaryRef)updateItem,
                                       (__bridge CFDictionaryRef)tempCheck);
    if (errorcode != noErr) {
      LOG(FATAL) << "Couldn't update the Keychain Item. errorcode: "
                 << errorcode;
    }
  } else {
    OSStatus errorcode =
        SecItemAdd((__bridge CFDictionaryRef)
                       [self dictionaryToSecItemFormat:_keychainData],
                   NULL);
    if (errorcode != noErr) {
      LOG(FATAL) << "Couldn't add the Keychain Item. errorcode: " << errorcode;
    }
    if (attributes) {
      CFRelease(attributes);
      attributes = nil;
    }
  }
}

@end
