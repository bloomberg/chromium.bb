// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package bindings

import (
	"fmt"
	"sync"

	"mojo/public/go/system"
)

// Connector owns a message pipe handle. It can read and write messages
// from the message pipe waiting on it if necessary. The operation are
// thread-safe.
type Connector struct {
	mu   sync.RWMutex // protects pipe handle
	pipe system.MessagePipeHandle

	done      chan struct{}
	waitMutex sync.Mutex
	waiter    AsyncWaiter
	waitChan  chan WaitResponse
}

// NewStubConnector returns a new |Connector| instance that sends and
// receives messages from a provided message pipe handle.
func NewConnector(pipe system.MessagePipeHandle, waiter AsyncWaiter) *Connector {
	return &Connector{
		pipe:     pipe,
		waiter:   waiter,
		done:     make(chan struct{}),
		waitChan: make(chan WaitResponse, 1),
	}
}

// ReadMessage reads a message from message pipe waiting on it if necessary.
func (c *Connector) ReadMessage() (*Message, error) {
	// Make sure that only one goroutine at a time waits a the handle.
	// We use separate lock so that we can send messages to the message pipe
	// while waiting on the pipe.
	//
	// It is better to acquire this lock first so that a potential queue of
	// readers will wait while closing the message pipe in case of Close()
	// call.
	c.waitMutex.Lock()
	defer c.waitMutex.Unlock()
	// Use read lock to use pipe handle without modifying it.
	c.mu.RLock()
	defer c.mu.RUnlock()

	if !c.pipe.IsValid() {
		return nil, fmt.Errorf("message pipe is closed")
	}
	// Check if we already have a message.
	result, bytes, handles := c.pipe.ReadMessage(system.MOJO_READ_MESSAGE_FLAG_NONE)
	if result == system.MOJO_RESULT_SHOULD_WAIT {
		waitId := c.waiter.AsyncWait(c.pipe, system.MOJO_HANDLE_SIGNAL_READABLE, c.waitChan)
		select {
		case <-c.waitChan:
			result, bytes, handles = c.pipe.ReadMessage(system.MOJO_READ_MESSAGE_FLAG_NONE)
			if result != system.MOJO_RESULT_OK {
				return nil, fmt.Errorf("error reading message: %v", result)
			}
		case <-c.done:
			c.waiter.CancelWait(waitId)
			return nil, fmt.Errorf("server stub is closed")
		}
	} else if result != system.MOJO_RESULT_OK {
		return nil, fmt.Errorf("error reading message: %v", result)
	}
	return ParseMessage(bytes, handles)
}

// WriteMessage writes a message to the message pipe.
func (c *Connector) WriteMessage(message *Message) error {
	// Use read lock to use pipe handle without modifying it.
	c.mu.RLock()
	defer c.mu.RUnlock()
	if !c.pipe.IsValid() {
		return fmt.Errorf("message pipe is closed")
	}
	return WriteMessage(c.pipe, message)
}

// Close closes the message pipe aborting wait on the message pipe.
// Panics if you try to close the |Connector| more than once.
func (c *Connector) Close() {
	// Stop waiting to acquire the lock.
	close(c.done)
	// Use write lock to modify the pipe handle.
	c.mu.Lock()
	c.pipe.Close()
	c.mu.Unlock()
}
