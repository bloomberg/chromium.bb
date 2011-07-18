//
//  ObjCVector.h
//  TestFramework
//
//  Created by Robert Sesek on 5/10/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#ifdef __cplusplus
struct ObjCVectorImp;
#else
typedef struct _ObjCVectorImpT ObjCVectorImp;
#endif

@interface ObjCVector : NSObject {
 @private
  ObjCVectorImp* imp_;
}

- (id)init;

- (void)addObject:(id)obj;
- (void)addObject:(id)obj atIndex:(NSUInteger)index;

- (void)removeObject:(id)obj;
- (void)removeObjectAtIndex:(NSUInteger)index;

- (id)objectAtIndex:(NSUInteger)index;

@end
