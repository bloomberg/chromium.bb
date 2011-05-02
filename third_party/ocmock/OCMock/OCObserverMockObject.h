//---------------------------------------------------------------------------------------
//  $Id: OCObserverMockObject.h 57 2010-07-19 06:14:27Z erik $
//  Copyright (c) 2009 by Mulle Kybernetik. See License file for details.
//---------------------------------------------------------------------------------------

#import <Foundation/Foundation.h>

@interface OCObserverMockObject : NSObject 
{
	BOOL			expectationOrderMatters;
	NSMutableArray	*recorders;
}

- (void)setExpectationOrderMatters:(BOOL)flag;

- (id)expect;

- (void)verify;

- (void)handleNotification:(NSNotification *)aNotification;

@end
