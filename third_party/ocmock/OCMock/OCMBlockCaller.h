//---------------------------------------------------------------------------------------
//  $Id: OCMBlockCaller.h 68 2010-08-20 13:20:52Z erik $
//  Copyright (c) 2010 by Mulle Kybernetik. See License file for details.
//---------------------------------------------------------------------------------------

#import <Foundation/Foundation.h>

#if NS_BLOCKS_AVAILABLE

@interface OCMBlockCaller : NSObject 
{
	void (^block)(NSInvocation *);
}

- (id)initWithCallBlock:(void (^)(NSInvocation *))theBlock;

- (void)handleInvocation:(NSInvocation *)anInvocation;

@end

#endif
