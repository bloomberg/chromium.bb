//---------------------------------------------------------------------------------------
//  $Id: OCMPassByRefSetter.m 57 2010-07-19 06:14:27Z erik $
//  Copyright (c) 2009 by Mulle Kybernetik. See License file for details.
//---------------------------------------------------------------------------------------

#import "OCMPassByRefSetter.h"


@implementation OCMPassByRefSetter

- (id)initWithValue:(id)aValue
{
	[super init];
	value = [aValue retain];
	return self;
}

- (void)dealloc
{
	[value release];
	[super dealloc];
}

- (id)value
{
	return value;
}

@end
