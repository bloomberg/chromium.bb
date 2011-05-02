//---------------------------------------------------------------------------------------
//  $Id: OCPartialMockRecorder.m 68 2010-08-20 13:20:52Z erik $
//  Copyright (c) 2009-2010 by Mulle Kybernetik. See License file for details.
//---------------------------------------------------------------------------------------

#import "OCPartialMockObject.h"
#import "OCMRealObjectForwarder.h"
#import "OCPartialMockRecorder.h"


@implementation OCPartialMockRecorder

- (id)andForwardToRealObject
{
	[invocationHandlers addObject:[[[OCMRealObjectForwarder	alloc] init] autorelease]];
	return self;
}


- (void)forwardInvocation:(NSInvocation *)anInvocation
{
	[super forwardInvocation:anInvocation];
	// not as clean as I'd wish...
	[(OCPartialMockObject *)signatureResolver setupForwarderForSelector:[anInvocation selector]];
}

@end
