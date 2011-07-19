//---------------------------------------------------------------------------------------
//  $Id: OCMRealObjectForwarder.h 68 2010-08-20 13:20:52Z erik $
//  Copyright (c) 2010 by Mulle Kybernetik. See License file for details.
//---------------------------------------------------------------------------------------

#import <Foundation/Foundation.h>

@interface OCMRealObjectForwarder : NSObject 
{
}

- (void)handleInvocation:(NSInvocation *)anInvocation;

@end
