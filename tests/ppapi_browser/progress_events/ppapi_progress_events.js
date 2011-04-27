// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Class to track the progress events received by a particular plugin instance.
function EventStateMachine() {
  // Work around how JS binds 'this'.
  var this_ = this;
  // Given a particular state, what are the acceptable event types.
  this.expectedNext = {
    'BEGIN': { 'loadstart': 1 },
    'loadstart': { 'progress': 1, 'error': 1, 'abort': 1, 'load': 1 },
    'progress': { 'progress': 1, 'error': 1, 'abort': 1, 'load': 1 },
    'error': { 'loadend': 1 },
    'abort': { 'loadend': 1 },
    'load': { 'loadend': 1 },
    'loadend': { },
    'UNEXPECTED': { },
  };
  // The current state (and index into expectedNext).
  this.currentState = 'BEGIN';
  // For each recognized state, a count of the times it was reached.
  this.stateHistogram = {
    'BEGIN': 0,
    'loadstart': 0,
    'progress': 0,
    'error': 0,
    'abort': 0,
    'load': 0,
    'loadend': 0,
    'UNEXPECTED': 0
  };
  // The state transition function.
  this.transitionTo = function(event_type) {
    // The index values of this_.expectedNext are the only valid states.
    // Invalid event types are normalized to 'UNEXPECTED'.
    if (this_.expectedNext[event_type] == undefined) {
      console.log('unexpected ' + event_type);
      event_type = 'UNEXPECTED';
    }
    // Check that the next event type is expected from the current state.
    // If not, we transition to the state 'UNEXPECTED'.
    if (!(event_type in this_.expectedNext[this_.currentState])) {
      console.log('unexpected ' + event_type + ' from ' + this_.currentState);
      event_type = 'UNEXPECTED';
    }
    this_.currentState = event_type;
    this_.stateHistogram[this_.currentState]++;
  }
}

// event_machines is a collection of EventStateMachines, one for each element
// id that dispatches an event of a type we are listening for.
window.event_machines = { };
// Look up the EventStateMachine for the id.
function lookupEventMachine(element_id) {
  var event_machine = window.event_machines[element_id];
  if (event_machine == undefined) {
    // This is the first event for this target.  Create an EventStateMachine.
    event_machine = new EventStateMachine();
    window.event_machines[element_id] = event_machine;
  }
  return event_machine;
}
// Sets up event listeners on the body element for all the progress
// event types.  Delegation to the body allows this to be done only once
// per document.
var setListeners = function(body_element) {
  var eventListener = function(e) {
    // Find the target element of the event.
    var target_element = e.target;
    // Body only dispatches for elements having the 'naclModule' CSS class.
    if (target_element.className != 'naclModule') {
      return;
    }
    var element_id = target_element.id;
    // Look up the EventStateMachine for the target of the event.
    var event_machine = lookupEventMachine(element_id);
    // Update the state of the machine.
    event_machine.transitionTo(e.type);
  }
  // Add the listener for all of the ProgressEvent event types.
  body_element.addEventListener('loadstart', eventListener, true);
  body_element.addEventListener('progress', eventListener, true);
  body_element.addEventListener('error', eventListener, true);
  body_element.addEventListener('abort', eventListener, true);
  body_element.addEventListener('load', eventListener, true);
  body_element.addEventListener('loadend', eventListener, true);
}

// Performs some tests to make sure that progress events follow the expected
// state transitions to end in an expected state.
function testProgressEventStateMachine(tester,
                                       embedId,
                                       progressMinCount,
                                       errorCount,
                                       abortCount,
                                       loadCount,
                                       lastError) {
  var eventMachine = lookupEventMachine(embedId);
  // Test the expected number of occurrences, with some duplication.
  tester.addTest('begin_count_' + embedId, function() {
    // There should be no 'BEGIN' event.
    assertEqual(eventMachine.stateHistogram['BEGIN'], 0);
  });
  tester.addTest('loadstart_count_' + embedId, function() {
    // There should be one 'loadstart' event.
    assertEqual(eventMachine.stateHistogram['loadstart'], 1);
  });
  tester.addTest('progress_min_count_' + embedId, function() {
    // There should be at least one progress event when the manifest file is
    // loaded and another when the .nexe is loaded.
    assert(eventMachine.stateHistogram['progress'] >= progressMinCount);
  });
  tester.addTest('error_count_' + embedId, function() {
    // Check that the right number of 'error' events were dispatched.
    assertEqual(eventMachine.stateHistogram['error'], errorCount);
  });
  tester.addTest('abort_count_' + embedId, function() {
    // Check that the right number of 'abort' events were dispatched.
    assertEqual(eventMachine.stateHistogram['abort'], abortCount);
  });
  tester.addTest('load_count_' + embedId, function() {
    // Check that the right number of 'load' events were dispatched.
    assertEqual(eventMachine.stateHistogram['load'], loadCount);
  })
  tester.addTest('loadend_count_' + embedId, function() {
    // There should be one 'loadend' event.
    assertEqual(eventMachine.stateHistogram['loadend'], 1);
  });
  tester.addTest('unexpected_count_' + embedId, function() {
    // There should be no 'UNEXPECTED' event.
    assertEqual(eventMachine.stateHistogram['UNEXPECTED'], 0);
  });
  tester.addTest('end_state_' + embedId, function() {
    // Test that the progress events followed the expected sequence to
    // completion in the 'loadend' state.
    assertEqual(eventMachine.currentState, 'loadend');
  });
  tester.addTest('last_error_string_' + embedId, function() {
    // If an error or abort was reported, check that lastError is set
    // to the correct value.
    if ((eventMachine.stateHistogram['error'] > 0 ||
         eventMachine.stateHistogram['abort'] > 0)) {
      var embed = $(embedId);
      assertEqual(embed.lastError, lastError);
    }
  });
}
