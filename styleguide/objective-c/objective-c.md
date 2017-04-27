# Chromium Objective-C and Objective-C++ style guide

_For other languages, please see the [Chromium style guides](https://chromium.googlesource.com/chromium/src/+/master/styleguide/styleguide.md)._

Chromium follows the
[Google Objective-C style guide](https://google.github.io/styleguide/objcguide.xml)
unless an exception is listed below.

A checkout should give you
[clang-format](https://chromium.googlesource.com/chromium/src/+/master/docs/clang_format.md)
to automatically format Objective-C and Objective-C++ code. By policy, Clang's
formatting of code should always be accepted in code reviews. If Clang's
formatting doesn't follow this style guide, file a bug.

## Line length

For consistency with the 80 character line length used in Chromium C++ code,
Objective-C and Objective-C++ code also has an 80 character line length.

## Chromium C++ style

Just as [Google Objective-C style](https://google.github.io/styleguide/objcguide.xml)
follows [Google C++ Style](https://google.github.io/styleguide/cppguide.html),
Chromium Objective-C and Objective-C++ follows [Chromium C++ style](../c++/c++.md).

## Code Formatting

Use `nil` for null pointers to Objective-C objects, and `nullptr` for C++
objects.

## Objective-C++ style matches the language

Within an Objective-C++ source file, follow the style for the language of the
function or method you're implementing.

In order to minimize clashes between the differing naming styles when mixing
Cocoa/Objective-C and C++, follow the style of the method being implemented.

For code in an `@implementation` block, use the Objective-C naming rules. For
code in a method of a C++ class, use the C++ naming rules.

For C functions and constants defined in a namespace, use C++ style, even if
most of the file is Objective-C.

`TEST` and `TEST_F` macros expand to C++ methods, so even if a unit test is
mostly testing Objective-C objects and methods, the test should be written using
C++ style.

## All files are Objective-C++

Chrome back-end code is all C++ and we want to leverage many C++ features, such as stack-based classes and namespaces. As a result, all front-end Bling files should be .mm files, as we expect eventually they will contain C++ code or language features.
## Use scoped_nsobject<T> and WeakNSObject<T> where ARC is not available. 

While there are no smart pointers in Objective-C, Chrome has `scoped_nsobject<T>` and `WeakNSObject<T>` to automatically manage (and document) object ownership. 

Under ARC, scoped_nsobject<T> and WeakNSObject<T> should only be used for interfacing with existing APIs that take these, or for declaring a C++ member variable in a header. Otherwise use __weak variables and strong/weak properties. **Note that scoped_nsobject and WeakNSObject provide the same API under ARC**, i.e. scoped_nsobject<T> foo([[Bar alloc] init]); is correct both under ARC and non-ARC.

`scoped_nsobject<T>` should be used for all owned member variables in C++ classes (except the private classes that only exist in implementation files) and Objective-C classes built without ARC, even if that means writing dedicated getters and setters to implement `@property` declarations. Same goes for WeakNSObject - always use it to express weak ownership of an Objective-C object, unless you are writing ARC code. We'd rather have a little more boilerplate code than a leak.

This also means that most common uses of `autorelease` (as recommended by the Obj-C Style Guide) are no longer necessary. For example, the guide recommends this pattern for temporary objects:

    MyObject* temp = [[[MyObject alloc] init] autorelease];

Instead, you can use `scoped_nsobject<T>` to avoid the autorelease and ensure the object is cleaned up automatically.

    scoped_nsobject<MyObject> temp([[MyObject alloc] init]);

Obviously, the use of `autorelease` is allowed when the object is the return value or it needs to live beyond the current scope.

## Use ObjCCast<T> and ObjcCCastStrict<T> 

As C++ style guide tells you, we never use C casts and prefer `static_cast<T>` and `dynamic_cast<T>`. However, for Objective-C casts we have two specific casts: `base::mac::ObjCCast<T>arg` is similar to `dynamic_cast<T>`, and `ObjcCCastStrict` `DCHECKs` against that class.  
## IBOutlets

While Apple recommends creating properties for IBOutlets, we discourage that since it makes the details of the view hierarchy public. Instead, declare a private variable, mark that as the IBOutlet, and then create a private retained property (i.e., declared in the `@interface MyObject ()` block in the implementation file) for that variable. Ensure that you have an `ObjCPropertyReleaser` (see [this CL](https://chromereviews.googleplex.com/3578024/) for an example) and that will handle releasing the XIB objects.

## Blocks

We follow Google style for blocks, except that historically we have used 2-space indentation for blocks that are parameters, rather than 4. You may continue to use this style when it is consistent with the surrounding code.

## NOTIMPLEMENTED and NOTREACHED logging macros

`NOTREACHED`: This function should not be called. If it is, we have a problem somewhere else.
`NOTIMPLEMENTED`: This isn't implemented because we don't use it yet. If it's called, then we need to figure out what it should do.

When something is called, but don't need an implementation, just comment that rather than using a logging macro.
## TODO comments

Sometimes we include TODO comments in code. Generally we follow [C++ style](https://google.github.io/styleguide/cppguide.html#TODO_Comments), but here are some more specific practices we've agreed upon as a team:
* **Every TODO must have a bug**
* Bug should be labeled with **Hotlist-TODO-iOS**
* Always list bug in parentheses following "TODO"
    * `// TODO(crbug.com/######): Something that needs doing.`
    * Do NOT include http://
* Optionally include a username for reference
* Optionally include expiration date (make sure it's documented in the bug!)

