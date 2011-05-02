//---------------------------------------------------------------------------------------
//  $Id: NSMethodSignature+OCMAdditions.m 57 2010-07-19 06:14:27Z erik $
//  Copyright (c) 2009 by Mulle Kybernetik. See License file for details.
//---------------------------------------------------------------------------------------

#import "NSMethodSignature+OCMAdditions.h"


@implementation NSMethodSignature(OCMAdditions)

- (const char *)methodReturnTypeWithoutQualifiers
{
	const char *returnType = [self methodReturnType];
	while(strchr("rnNoORV", returnType[0]) != NULL)
		returnType += 1;
	return returnType;
}

@end
