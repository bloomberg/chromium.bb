// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This example demonstrates loading, running and scripting a very simple NaCl
// module.  To load the NaCl module, the browser first looks for the
// CreateModule() factory method (at the end of this file).  It calls
// CreateModule() once to load the module code from your .nexe.  After the
// .nexe code is loaded, CreateModule() is not called again.
//
// Once the .nexe code is loaded, the browser then calls the
// HelloWorldModule::CreateInstance()
// method on the object returned by CreateModule().  It calls CreateInstance()
// each time it encounters an <embed> tag that references your NaCl module.
//
// When the browser encounters JavaScript that references your NaCl module, it
// calls the GetInstanceObject() method on the object returned from
// CreateInstance().  In this example, the returned object is a subclass of
// ScriptableObject, which handles the scripting support.

#include <ppapi/cpp/dev/scriptable_object_deprecated.h>
#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/module.h>
#include <ppapi/cpp/var.h>
#include <string>

#include "native_client/src/include/nacl_compiler_annotations.h"

namespace {
// Helper function to set the scripting exception.  Both |exception| and
// |except_string| can be NULL.  If |exception| is NULL, this function does
// nothing.
void SetExceptionString(pp::Var* exception, const std::string& except_string) {
  if (exception) {
    *exception = except_string;
  }
}

// Exception strings.  These are passed back to the browser when errors
// happen during property accesses or method calls.
const char* const kExceptionMethodNotAString = "Method name is not a string";
const char* const kExceptionNoMethodName = "No method named ";
const char* const kExceptionPropertyNotAString =
    "Property name is not a string";
const char* const kExceptionNoPropertyName = "No property named ";

}  // namespace

namespace hello_world {
// Return value for the "helloWorld" method.
const char* const kHelloWorld = "hello, world";
// Value of the "fortyTwo" property.
const int32_t kFortyTwo = 42;

// Method name for helloWorld, as seen by JavaScript code.
const char* const kHelloWorldMethodId = "helloWorld";

// Property name for fortyTwo, as seen by Javascript code.
const char* const kFortyTwoPropertyId = "fortyTwo";

// This class exposes the scripting interface for this NaCl module.  The
// HasMethod() method is called by the browser when executing a method call on
// the |helloWorldModule| object (see the reverseText() function in
// hello_world.html).  The name of the JavaScript function (e.g. "fortyTwo") is
// passed in the |method| parameter as a string pp::Var.  If HasMethod()
// returns |true|, then the browser will call the Call() method to actually
// invoke the method.
class HelloWorldScriptableObject : public pp::deprecated::ScriptableObject {
 public:
  // Determines whether a given method is implemented in this object.
  // @param[in] method A JavaScript string containing the method name to check
  // @param exception Unused
  // @return |true| if |method| is one of the exposed method names.
  virtual bool HasMethod(const pp::Var& method, pp::Var* exception);

  // Determines if a given property is available on this object.
  // @param[in] property A JavaScript string containing the property name.
  // @param[out] exception Filled in if an error occurs.
  // @return |true| if |property| is one of the exposed property names.
  virtual bool HasProperty(const pp::Var& property, pp::Var* exception);

  // Return the value of the property associated with the name |property|.
  // @param[in] property A JavaScript string containing the property name.
  // @param[out] exception Filled in if an error occurs.
  // @return the value of |property| if it exists, or a null variable if not.
  virtual pp::Var GetProperty(const pp::Var& property,
                              pp::Var* exception);

  // Invoke the function associated with |method|.  The argument list passed
  // via JavaScript is marshaled into a vector of pp::Vars.  None of the
  // functions in this example take arguments, so this vector is always empty.
  // @param[in] method A JavaScript string with the name of the method to call
  // @param[in] args A list of the JavaScript parameters passed to this method
  // @param exception unused
  // @return the return value of the invoked method
  virtual pp::Var Call(const pp::Var& method,
                       const std::vector<pp::Var>& args,
                       pp::Var* exception);
};

bool HelloWorldScriptableObject::HasMethod(const pp::Var& method,
                                           pp::Var* exception) {
  if (!method.is_string()) {
    SetExceptionString(exception, kExceptionMethodNotAString);
    return false;
  }
  std::string method_name = method.AsString();
  return method_name == kHelloWorldMethodId;
}

bool HelloWorldScriptableObject::HasProperty(const pp::Var& property,
                                             pp::Var* exception) {
  if (!property.is_string()) {
    SetExceptionString(exception, kExceptionPropertyNotAString);
    return false;
  }
  const std::string property_name = property.AsString();
  return property_name == kFortyTwoPropertyId;
}

pp::Var HelloWorldScriptableObject::Call(const pp::Var& method,
                                         const std::vector<pp::Var>& args,
                                         pp::Var* exception) {
  if (!method.is_string()) {
    SetExceptionString(exception, kExceptionMethodNotAString);
    return pp::Var();
  }
  std::string method_name = method.AsString();
  if (method_name == kHelloWorldMethodId) {
    return pp::Var(kHelloWorld);
  } else {
    SetExceptionString(exception,
                       std::string(kExceptionNoMethodName) + method_name);
  }
  return pp::Var();
}

pp::Var HelloWorldScriptableObject::GetProperty(const pp::Var& property,
                                                pp::Var* exception) {
  if (!property.is_string()) {
    SetExceptionString(exception, kExceptionPropertyNotAString);
    return pp::Var();
  }
  std::string property_name = property.AsString();
  if (property_name == kFortyTwoPropertyId) {
    return pp::Var(kFortyTwo);
  }
  SetExceptionString(exception,
                     std::string(kExceptionNoPropertyName) + property_name);
  return pp::Var();
}

// The Instance class.  One of these exists for each instance of your NaCl
// module on the web page.  The browser will ask the Module object to create
// a new Instance for each occurrence of the <embed> tag that has these
// attributes:
//     type="application/x-nacl"
//     src="hello_world.nmf"
// The Instance can return a ScriptableObject representing itself.  When the
// browser encounters JavaScript that wants to access the Instance, it calls
// the GetInstanceObject() method.  All the scripting work is done through
// the returned ScriptableObject.
class HelloWorldInstance : public pp::Instance {
 public:
  explicit HelloWorldInstance(PP_Instance instance) : pp::Instance(instance) {}
  virtual ~HelloWorldInstance() {}

  // Return a new pp::deprecated::ScriptableObject as a JavaScript Var
  // The pp::Var takes over ownership of the HelloWorldScriptableObject
  // and is responsible for deallocating memory.
  virtual pp::Var GetInstanceObject() {
    HelloWorldScriptableObject* hw_object = new HelloWorldScriptableObject();
#ifdef PPAPI_INSTANCE_REMOVE_SCRIPTING
    UNREFERENCED_PARAMETER(hw_object);
    NACL_NOTREACHED();
    return pp::Var();
#else
    return pp::Var(this, hw_object);
#endif
  }
};

// The Module class.  The browser calls the CreateInstance() method to create
// an instance of you NaCl module on the web page.  The browser creates a new
// instance for each <embed> tag with type="application/x-nacl".
class HelloWorldModule : public pp::Module {
 public:
  HelloWorldModule() : pp::Module() {}
  virtual ~HelloWorldModule() {}

  // Create and return a HelloWorldInstance object.  The browser is responsible
  // for calling delete when done.
  // @param[in] instance a handle to a plug-in instance.
  // @return a newly created HelloWorldInstance.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new HelloWorldInstance(instance);
  }
};
}  // namespace hello_world


namespace pp {
// Factory function called by the browser when the module is first loaded.
// The browser keeps a singleton of this module.  It calls the
// CreateInstance() method on the object you return to make instances.  There
// is one instance per <embed> tag on the page.  This is the main binding
// point for your NaCl module with the browser.  The browser is responsible for
// deleting the returned Module.
// @return new HelloWorldModule.
Module* CreateModule() {
  return new hello_world::HelloWorldModule();
}
}  // namespace pp
