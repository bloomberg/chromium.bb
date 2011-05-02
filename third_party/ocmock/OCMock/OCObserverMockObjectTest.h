//---------------------------------------------------------------------------------------
//  $Id: OCObserverMockObjectTest.h 57 2010-07-19 06:14:27Z erik $
//  Copyright (c) 2009 by Mulle Kybernetik. See License file for details.
//---------------------------------------------------------------------------------------

#import <SenTestingKit/SenTestingKit.h>


@interface OCObserverMockObjectTest : SenTestCase 
{
	NSNotificationCenter *center;
	
	id mock;
}

@end
