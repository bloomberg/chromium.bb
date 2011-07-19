//---------------------------------------------------------------------------------------
//  $Id: OCPartialMockObject.h 74 2011-02-15 12:59:47Z erik $
//  Copyright (c) 2009 by Mulle Kybernetik. See License file for details.
//---------------------------------------------------------------------------------------

#import "OCClassMockObject.h"

@interface OCPartialMockObject : OCClassMockObject 
{
	NSObject	*realObject;
}

- (id)initWithObject:(NSObject *)anObject;

- (NSObject *)realObject;

- (void)stop;

- (void)setupSubclassForObject:(id)anObject;
- (void)setupForwarderForSelector:(SEL)selector;

@end


extern NSString *OCMRealMethodAliasPrefix;
