/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

#include <exception>

#include "native_client/src/include/nacl_compiler_annotations.h"


class MyException {
 public:
  explicit MyException(int value) : value(value) {}
  int value;
};

class MyExceptionDerived : public MyException {
 public:
  explicit MyExceptionDerived(int value) : MyException(value) {}
};

void thrower() {
  throw MyException(123);
}

// Test throwing and catching a simple exception.
void test_simple_throw_and_catch() {
  bool caught = false;
  try {
    thrower();
  } catch (MyException &exc) {
    assert(exc.value == 123);
    caught = true;
  }
  assert(caught);
}


// Test that catch() blocks are checked in order.
void test_catch_type_matching(bool derived) {
  bool caught = false;
  try {
    if (derived) {
      throw MyExceptionDerived(100);
    } else {
      throw MyException(200);
    }
  } catch (MyExceptionDerived &exc) {
    assert(derived);
    assert(exc.value == 100);
    caught = true;
  } catch (MyException &exc) {
    assert(!derived);
    assert(exc.value == 200);
    caught = true;
  }
  assert(caught);
}


// Test that a catch(T) block matches subtypes of T.
void test_catch_subtype_matching() {
  bool caught = false;
  try {
    throw MyExceptionDerived(123);
  } catch (MyException &exc) {
    assert(exc.value == 123);
    caught = true;
  }
  assert(caught);
}


// Test that "catch(...)" (catch-all) blocks work.
void test_catch_all() {
  bool caught = false;
  try {
    throw MyException(0);
  } catch (...) {
    caught = true;
  }
  assert(caught);
}


// Prevent inlining in order to test that the unwinder properly
// searches upwards through multiple frames, skipping this
// non-matching frame.
__attribute__((noinline))
void non_catcher() {
  try {
    thrower();
  } catch (int) {
    assert(false);
  }
}

// Test a try+catch in which none of the catch blocks match.  Prevent
// inlining otherwise the inliner reduces this to a try+catch with
// multiple catches, which is already tested by
// test_catch_type_matching().
void test_nested_try_blocks() {
  bool caught = false;
  try {
    non_catcher();
  } catch (MyException &exc) {
    assert(exc.value == 123);
    caught = true;
  }
  assert(caught);
}


void throw_in_catch_block() {
  try {
    throw MyException(100);
  } catch (MyException &exc) {
    throw MyException(200);
  }
}

// Test throwing inside a catch() block.  This tests that the SJLJ EH
// unwinder updates the frame list correctly before longjmp()ing to
// the catch block.
void test_throw_in_catch_block() {
  bool caught = false;
  try {
    throw_in_catch_block();
  } catch (MyException &exc2) {
    assert(exc2.value == 200);
    caught = true;
  }
  assert(caught);
}


class Dtor {
  bool *ptr_;
 public:
  explicit Dtor(bool *ptr) : ptr_(ptr) {}
  ~Dtor() { *ptr_ = true; }
};

// Without "noinline", this can get inlined into test_dtor(), which
// means we are no longer testing the "cleanup" attribute of
// landingpads.
__attribute__((noinline))
void func_with_dtor(bool *ptr) {
  Dtor dtor(ptr);
  throw MyException(0);
}

// Test that destructors get called.  This tests the LLVM "resume"
// instruction.
void test_dtor() {
  bool caught = false;
  try {
    func_with_dtor(&caught);
  } catch (MyException &) {
    assert(caught);
  }
  assert(caught);
}


void rethrow_func() {
  try {
    throw MyException(300);
  } catch (...) {
    throw;
  }
  assert(false);
}

// Test re-throwing the current exception with "throw;".
void test_rethrow() {
  bool caught = false;
  try {
    rethrow_func();
  } catch (MyException &exc) {
    assert(exc.value == 300);
    caught = true;
  }
  assert(caught);
}


// Test that throw+catch does not clobber the current exception, which
// can be re-thrown with "throw;".
void test_stack_of_exceptions() {
  try {
    try {
      throw MyException(10);
    } catch (...) {
      // This try+catch block should have no net effect.
      try {
        throw MyException(20);
      } catch (MyException &exc1) {
        assert(exc1.value == 20);
      }
      // Rethrowing should throw the original exception.  The C++
      // runtime's __cxa_begin_catch() and __cxa_end_catch() functions
      // need to maintain a thread-local stack of exceptions in order
      // to implement this.
      throw;
    }
  } catch (MyException &exc2) {
    assert(exc2.value == 10);
  }
}


class ExcBase1 {
 public:
  ExcBase1() : base1_val(100) {}
  int base1_val;
};

class ExcBase2 {
 public:
  ExcBase2() : base2_val(200) {}
  int base2_val;
};

class ExcDerived : public ExcBase1, public ExcBase2 {
};

// Test that the pointer to the exception is adjusted correctly when
// passing it to the catch block involves upcasting.  Non-zero
// adjustments are required when using multiple inheritance: ExcBase2
// is allocated inside ExcDerived at a non-zero offset.
void test_multiple_inheritance_exception() {
  bool caught = false;
  try {
    throw ExcDerived();
  } catch (ExcBase2 &exc) {
    assert(exc.base2_val == 200);
    caught = true;
  }
  assert(caught);
}

// This is like test_multiple_inheritance_exception(), but it tests
// throwing and catching a pointer-to-class type rather than a class
// type.  The same pointer upcasting adjustment for multiple
// inheritance is required in both cases.
void test_multiple_inheritance_exception_ptr() {
  bool caught = false;
  ExcDerived *exc = new ExcDerived();
  try {
    throw exc;
  } catch (ExcBase2 *exc) {
    assert(exc->base2_val == 200);
    caught = true;
  }
  assert(caught);
  delete exc;
}


class DtorThrowCatch {
 public:
  ~DtorThrowCatch() {
    // This try+catch block should have no net effect.
    try {
      throw MyException(40);
    } catch (MyException &exc) {
      assert(exc.value == 40);
    }
  }
};

// This needs to be a separate, non-inlined function so that we are
// testing its landingpad executing _Unwind_Resume() (or its SJLJ EH
// equivalent).
__attribute__((noinline))
void nested_throw_catch() {
  DtorThrowCatch obj;
  UNREFERENCED_PARAMETER(obj);
  throw MyException(50);
}

// Test that throw+catch inside a destructor does not clobber the
// current exception.
//
// This tests that _Unwind_Resume() (or its SJLJ EH equivalent) uses
// the correct exception.  _Unwind_Resume() needs to take an exception
// as an argument and cannot refer to a thread-local "current
// exception".
void test_throw_catch_nested_in_dtor() {
  bool caught = false;
  try {
    nested_throw_catch();
  } catch (MyException &exc) {
    assert(exc.value == 50);
    caught = true;
  }
  assert(caught);
}


class RethrowExc {};

bool g_dtor_called;

void rethrow_unexpected_handler() {
  // func_with_exception_spec()'s destructors should be run before the
  // exception spec (a "filter" clause in LLVM) is checked and we are
  // called.
  //
  // This means that the std::set_unexpected() handler should be
  // called from a landingpad inside the function with the exception
  // spec (by calling __cxa_call_unexpected()), rather than by the
  // personality function (or the SJLJ EH equivalent) which checks the
  // exception against the exception spec.
  assert(g_dtor_called);

  throw RethrowExc();
}

// Example of a function with an exception spec, i.e. the "throw(...)"
// attribute on the function.
void func_with_exception_spec() throw(RethrowExc) {
  Dtor dtor(&g_dtor_called);
  throw MyException(100);
}

// Test that exception specs work.  Test that if an exception doesn't
// match the exception spec of a function, the current
// std::set_unexpected() handler is called.  The handler is allowed to
// throw a new exception that matches the exception spec -- we test
// this case to avoid aborting execution.
void test_exception_spec() {
  g_dtor_called = false;
  std::unexpected_handler old_unexpected_handler =
      std::set_unexpected(rethrow_unexpected_handler);
  bool caught = false;
  try {
    func_with_exception_spec();
  } catch (RethrowExc &) {
    caught = true;
  }
  assert(caught);
  // Clean up.
  std::set_unexpected(old_unexpected_handler);
}


void rethrowing_unexpected_handler() {
  try {
    throw;
  } catch (MyException &exc) {
    assert(exc.value == 100);
  }
  throw RethrowExc();
}

// Test that the "current exception" inside the std::set_unexpected()
// handler is the exception that didn't match the exception spec.
// This allows the handler to inspect this exception by rethrowing and
// catching it.
void test_exception_spec_rethrow_inside_unexpected_handler() {
  std::unexpected_handler old_unexpected_handler =
      std::set_unexpected(rethrowing_unexpected_handler);
  bool caught = false;
  try {
    func_with_exception_spec();
  } catch (RethrowExc &) {
    caught = true;
  }
  assert(caught);
  // Clean up.
  std::set_unexpected(old_unexpected_handler);
}


void exception_spec_allowing_bad_exception() throw(std::bad_exception) {
  throw MyException(100);
}

// Test that if:
//  1) a std::set_unexpected() handler throws an exception that does
//     not match the exception spec, and
//  2) an exception spec allows "std::bad_exception",
// then the exception thrown by the set_unexpected() handler gets
// converted to a std::bad_exception exception.
void test_exception_spec_convert_to_bad_exception() {
  std::unexpected_handler old_unexpected_handler =
      std::set_unexpected(rethrow_unexpected_handler);
  bool caught = false;
  try {
    exception_spec_allowing_bad_exception();
  } catch (std::bad_exception &) {
    caught = true;
  }
  assert(caught);
  // Clean up.
  std::set_unexpected(old_unexpected_handler);
}


jmp_buf g_jmp_buf;

void terminate_handler() {
  longjmp(g_jmp_buf, 999);
}

void bad_unexpected_handler() {
  throw MyException(123);
}

// Test that if:
//  1) a std::set_unexpected() handler throws an exception that does
//     not match the exception spec, and
//  2) an exception spec does *not* allow "std::bad_exception",
// then the C++ runtime calls std::terminate().
void test_exception_spec_bad_throw_from_unexpected_handler() {
  std::unexpected_handler old_unexpected_handler =
      std::set_unexpected(bad_unexpected_handler);
  // To test that std::terminate() is called, but avoid aborting
  // execution, we register a terminate handler but longjmp() out of
  // it to continue execution.
  std::terminate_handler old_terminate_handler =
      std::set_terminate(terminate_handler);
  int val = setjmp(g_jmp_buf);
  if (val == 0) {
    try {
      func_with_exception_spec();
    } catch (MyException &) {
      // We should not be able to reach here by catching MyException,
      // because it does not meet func_with_exception_spec()'s
      // exception spec.  The std::set_unexpected() handler should not
      // be able to throw MyException to here.
      assert(false);
    }
    // We certainly don't expect func_with_exception_spec() to return.
    assert(false);
  }
  assert(val == 999);
  // Clean up.
  std::set_unexpected(old_unexpected_handler);
  std::set_terminate(old_terminate_handler);
}


#define RUN_TEST(CALL) printf("Running %s\n", #CALL); CALL;

int main() {
  RUN_TEST(test_simple_throw_and_catch());
  RUN_TEST(test_catch_type_matching(false));
  RUN_TEST(test_catch_type_matching(true));
  RUN_TEST(test_catch_subtype_matching());
  RUN_TEST(test_catch_all());
  RUN_TEST(test_nested_try_blocks());
  RUN_TEST(test_throw_in_catch_block());
  RUN_TEST(test_dtor());
  RUN_TEST(test_rethrow());
  RUN_TEST(test_stack_of_exceptions());
  RUN_TEST(test_multiple_inheritance_exception());
  RUN_TEST(test_multiple_inheritance_exception_ptr());
  RUN_TEST(test_throw_catch_nested_in_dtor());
  RUN_TEST(test_exception_spec());
  RUN_TEST(test_exception_spec_rethrow_inside_unexpected_handler());
  RUN_TEST(test_exception_spec_convert_to_bad_exception());
  // This leaves behind an active exception because of its use of
  // longjmp() to exit from a std::set_terminate() handler, so put it
  // last, just in case that accidentally affects other tests.
  RUN_TEST(test_exception_spec_bad_throw_from_unexpected_handler());

  // Indicate success.
  return 55;
}
