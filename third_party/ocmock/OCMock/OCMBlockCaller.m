//---------------------------------------------------------------------------------------
//	$Id: OCMBlockCaller.m 57 2010-07-19 06:14:27Z erik $
//  Copyright (c) 2010 by Mulle Kybernetik. See License file for details.
//---------------------------------------------------------------------------------------

#import "OCMBlockCaller.h"

#if NS_BLOCKS_AVAILABLE

@implementation OCMBlockCaller

-(id)initWithCallBlock:(void (^)(NSInvocation *))theBlock 
{
	[super init];
	block = [theBlock copy];
	return self;
}

-(void)dealloc 
{
	[block release];
	[super dealloc];
}

- (void)handleInvocation:(NSInvocation *)anInvocation
{
	block(anInvocation);
}

@end

#endif
