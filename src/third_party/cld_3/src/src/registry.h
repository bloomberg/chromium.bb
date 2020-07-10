/* Copyright 2016 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// Registry for component registration. These classes can be used for creating
// registries of components conforming to the same interface. This is useful for
// making a component-based architecture where the specific implementation
// classes can be selected at runtime. There is support for both class-based and
// instance based registries.
//
// Example:
//  function.h:
//
//   class Function : public RegisterableInstance<Function> {
//    public:
//     virtual double Evaluate(double x) = 0;
//   };
//
//   #define REGISTER_FUNCTION(type, component)
//     REGISTER_INSTANCE_COMPONENT(Function, type, component);
//
//  function.cc:
//
//   REGISTER_INSTANCE_REGISTRY("function", Function);
//
//   class Cos : public Function {
//    public:
//     double Evaluate(double x) { return cos(x); }
//   };
//
//   class Exp : public Function {
//    public:
//     double Evaluate(double x) { return exp(x); }
//   };
//
//   REGISTER_FUNCTION("cos", Cos);
//   REGISTER_FUNCTION("exp", Exp);
//
//   Function *f = Function::Lookup("cos");
//   double result = f->Evaluate(arg);

#ifndef REGISTRY_H_
#define REGISTRY_H_

#include <string.h>

#include <string>

#include "base.h"

namespace chrome_lang_id {

// Component metadata with information about name, class, and code location.
class ComponentMetadata {
 public:
  ComponentMetadata(const char *name, const char *class_name, const char *file,
                    int line)
      : name_(name),
        class_name_(class_name),
        file_(file),
        line_(line),
        link_(NULL) {}

  // Getters.
  const char *name() const { return name_; }
  const char *class_name() const { return class_name_; }
  const char *file() const { return file_; }
  int line() const { return line_; }

  // Metadata objects can be linked in a list.
  ComponentMetadata *link() const { return link_; }
  void set_link(ComponentMetadata *link) { link_ = link; }

 private:
  // Component name.
  const char *name_;

  // Name of class for component.
  const char *class_name_;

  // Code file and location where the component was registered.
  const char *file_;
  int line_;

  // Link to next metadata object in list.
  ComponentMetadata *link_;
};

// The master registry contains all registered component registries. A registry
// is not registered in the master registry until the first component of that
// type is registered.
class RegistryMetadata : public ComponentMetadata {
 public:
  RegistryMetadata(const char *name, const char *class_name, const char *file,
                   int line)
      : ComponentMetadata(name, class_name, file, line) {}

  // Registers a component registry in the master registry.
  static void Register(RegistryMetadata *registry);
};

// Registry for components. An object can be registered with a type name in the
// registry. The named instances in the registry can be returned using the
// Lookup() method. The components in the registry are put into a linked list
// of components. It is important that the component registry can be statically
// initialized in order not to depend on initialization order.
template <class T>
struct ComponentRegistry {
  typedef ComponentRegistry<T> Self;

  // Component registration class.
  class Registrar : public ComponentMetadata {
   public:
    // Registers new component by linking itself into the component list of
    // the registry.
    Registrar(Self *registry, const char *type, const char *class_name,
              const char *file, int line, T *object)
        : ComponentMetadata(type, class_name, file, line), object_(object) {
      // Register registry in master registry if this is the first registered
      // component of this type.
      if (registry->components == NULL) {
        RegistryMetadata::Register(
            new RegistryMetadata(registry->name, registry->class_name,
                                 registry->file, registry->line));
      }

      // Register component in registry.
      set_link(registry->components);
      registry->components = this;
    }

    // Returns component type.
    const char *type() const { return name(); }

    // Returns component object.
    T *object() const { return object_; }

    // Returns the next component in the component list.
    Registrar *next() const { return static_cast<Registrar *>(link()); }

   private:
    // Component object.
    T *object_;
  };

  // Finds registrar for named component in registry.
  const Registrar *GetComponent(const char *type) const {
    Registrar *r = components;
    while (r != NULL && strcmp(type, r->type()) != 0) r = r->next();
    CLD3_DCHECK(r != nullptr);

    return r;
  }

  // Finds a named component in the registry.
  T *Lookup(const char *type) const { return GetComponent(type)->object(); }
  T *Lookup(const string &type) const { return Lookup(type.c_str()); }

  // Textual description of the kind of components in the registry.
  const char *name;

  // Base class name of component type.
  const char *class_name;

  // File and line where the registry is defined.
  const char *file;
  int line;

  // Linked list of registered components.
  Registrar *components;
};

// Base class for registerable class-based components.
template <class T>
class RegisterableClass {
 public:
  // Factory function type.
  typedef T *(Factory)();

  // Registry type.
  typedef ComponentRegistry<Factory> Registry;

  // Should be called before any call to Create() or registry(), i.e., before
  // using the registration mechanism to register and or instantiate subclasses
  // of T.
  static void CreateRegistry(
      const char *name,
      const char *class_name,
      const char *file,
      int line) {
    registry_ = new Registry();
    registry_->name = name;
    registry_->class_name = class_name;
    registry_->file = file;
    registry_->line = line;
    registry_->components = nullptr;
  }

  // Should be called when one is done using the registration mechanism for
  // class T.
  static void DeleteRegistry() {
    delete registry_;
    registry_ = nullptr;
  }

  // Creates a new component instance.
  static T *Create(const string &type) { return registry()->Lookup(type)(); }

  // Returns registry for class.
  static Registry *registry() { return registry_; }

 private:
  // Registry for class.
  static Registry *registry_;
};

// Base class for registerable instance-based components.
template <class T>
class RegisterableInstance {
 public:
  // Registry type.
  typedef ComponentRegistry<T> Registry;

 private:
  // Registry for class.
  static Registry registry_;
};

}  // namespace chrome_lang_id

#endif  // REGISTRY_H_
