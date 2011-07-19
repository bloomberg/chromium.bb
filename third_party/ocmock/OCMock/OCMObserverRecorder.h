//---------------------------------------------------------------------------------------
//  $Id: OCMObserverRecorder.h 57 2010-07-19 06:14:27Z erik $
//  Copyright (c) 2009 by Mulle Kybernetik. See License file for details.
//---------------------------------------------------------------------------------------

#import <Foundation/Foundation.h>

@interface OCMObserverRecorder : NSObject 
{
	NSNotification *recordedNotification;
}

- (void)notificationWithName:(NSString *)name object:(id)sender;

- (BOOL)matchesNotification:(NSNotification *)aNotification;

- (BOOL)argument:(id)expectedArg matchesArgument:(id)observedArg;

@end
