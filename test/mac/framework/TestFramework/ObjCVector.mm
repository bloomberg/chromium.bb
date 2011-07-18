//
//  ObjCVector.mm
//  TestFramework
//
//  Created by Robert Sesek on 5/10/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import "ObjCVectorInternal.h"
#import "ObjCVector.h"

#include <vector>

@interface ObjCVector (Private)
- (std::vector<id>::iterator)makeIterator:(NSUInteger)index;
@end

@implementation ObjCVector

- (id)init {
  if ((self = [super init])) {
    imp_ = new ObjCVectorImp();
  }
  return self;
}

- (void)dealloc {
  delete imp_;
  [super dealloc];
}

- (void)addObject:(id)obj {
  imp_->v.push_back([obj retain]);
}

- (void)addObject:(id)obj atIndex:(NSUInteger)index {
  imp_->v.insert([self makeIterator:index], [obj retain]);
}

- (void)removeObject:(id)obj {
  for (std::vector<id>::iterator it = imp_->v.begin();
       it != imp_->v.end();
       ++it) {
    if ([*it isEqual:obj]) {
      [*it autorelease];
      imp_->v.erase(it);
      return;
    }
  }
}

- (void)removeObjectAtIndex:(NSUInteger)index {
  [imp_->v[index] autorelease];
  imp_->v.erase([self makeIterator:index]);
}

- (id)objectAtIndex:(NSUInteger)index {
  return imp_->v[index];
}

- (std::vector<id>::iterator)makeIterator:(NSUInteger)index {
  std::vector<id>::iterator it = imp_->v.begin();
  it += index;
  return it;
}

@end
