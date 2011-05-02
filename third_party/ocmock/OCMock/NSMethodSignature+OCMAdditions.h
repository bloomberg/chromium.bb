//---------------------------------------------------------------------------------------
//  $Id: NSMethodSignature+OCMAdditions.h 57 2010-07-19 06:14:27Z erik $
//  Copyright (c) 2009 by Mulle Kybernetik. See License file for details.
//---------------------------------------------------------------------------------------

#import <Foundation/Foundation.h>

@interface NSMethodSignature(PrivateAPI)

+ (id)signatureWithObjCTypes:(const char *)types;

@end

@interface NSMethodSignature(OCMAdditions)

- (const char *)methodReturnTypeWithoutQualifiers;

@end
