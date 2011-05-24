// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_INSTANCE_H_
#define PPAPI_CPP_INSTANCE_H_

/**
 * @file
 * Defines the API ...
 *
 * @addtogroup CPP
 * @{
 */

#include <map>
#include <string>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

struct PP_InputEvent;

/** The C++ interface to the Pepper API. */
namespace pp {

class Graphics2D;
class ImageData;
class Point;
class Rect;
class Rect;
class Resource;
class Surface3D_Dev;
class URLLoader;
class Var;
class Widget_Dev;

class Instance {
 public:
  explicit Instance(PP_Instance instance);
  virtual ~Instance();

  PP_Instance pp_instance() const { return pp_instance_; }

  /**
   * Initializes this plugin with the given arguments.
   * @param argc The argument count
   * @param argn The argument names
   * @param argv The argument values
   * @return True on success. Returning false causes the plugin
   * instance to be deleted and no other functions to be called.
   */
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);


  // @{
  /** @name PPP_Instance methods for the plugin to override: */

  /** See PPP_Instance.DidChangeView. */
  virtual void DidChangeView(const Rect& position, const Rect& clip);

  /** See PPP_Instance.DidChangeFocus. */
  virtual void DidChangeFocus(bool has_focus);

  /** See PPP_Instance.HandleInputEvent. */
  virtual bool HandleInputEvent(const PP_InputEvent& event);

  /** See PPP_Instance.HandleDocumentLoad. */
  virtual bool HandleDocumentLoad(const URLLoader& url_loader);

  /** See PPP_Instance.GetSelectedText. */
  virtual Var GetSelectedText(bool html);
  // @}

  // @{
  /** @name PPB_Instance methods for querying the browser: */

#ifndef PPAPI_INSTANCE_REMOVE_SCRIPTING
  /** See PPP_Instance.GetInstanceObject. */
  virtual Var GetInstanceObject();

  /** See PPB_Instance.GetWindowObject. */
  Var GetWindowObject();

  /** See PPB_Instance.GetOwnerElementObject. */
  Var GetOwnerElementObject();

  /** See PPB_Instance.ExecuteScript. */
  Var ExecuteScript(const Var& script, Var* exception = NULL);
#endif

  /** See PPB_Instance.BindGraphics. */
  bool BindGraphics(const Graphics2D& graphics);

  /** See PPB_Instance.BindGraphics. */
  bool BindGraphics(const Surface3D_Dev& graphics);

  /** See PPB_Instance.IsFullFrame. */
  bool IsFullFrame();

  // These functions use the PPP_Messaging and PPB_Messaging interfaces, so that
  // messaging can be done conveniently for a pp::Instance without using a
  // separate C++ class.

  /** See PPP_Messaging.HandleMessage. */
  virtual void HandleMessage(const Var& message);
  /** See PPB_Messaging.PostMessage. */
  void PostMessage(const Var& message);

  // @}

  /**
   * Associates a plugin instance with an interface,
   * creating an object... {PENDING: clarify!}
   *
   * Many optional interfaces are associated with a plugin instance. For
   * example, the find in PPP_Find interface receives updates on a per-instance
   * basis. This "per-instance" tracking allows such objects to associate
   * themselves with an instance as "the" handler for that interface name.
   *
   * In the case of the find example, the find object registers with its
   * associated instance in its constructor and unregisters in its destructor.
   * Then whenever it gets updates with a PP_Instance parameter, it can
   * map back to the find object corresponding to that given PP_Instance by
   * calling GetPerInstanceObject.
   *
   * This lookup is done on a per-interface-name basis. This means you can
   * only have one object of a given interface name associated with an
   * instance.
   *
   * If you are adding a handler for an additional interface, be sure to
   * register with the module (AddPluginInterface) for your interface name to
   * get the C calls in the first place.
   *
   * @see RemovePerInstanceObject
   * @see GetPerInstanceObject
   */
  void AddPerInstanceObject(const std::string& interface_name, void* object);

  /**
   * {PENDING: summarize Remove method here}
   *
   * @see AddPerInstanceObject
   */
  void RemovePerInstanceObject(const std::string& interface_name, void* object);

  /**
   * Look up an object previously associated with an instance. Returns NULL
   * if the instance is invalid or there is no object for the given interface
   * name on the instance.
   *
   * @see AddPerInstanceObject
   */
  static void* GetPerInstanceObject(PP_Instance instance,
                                    const std::string& interface_name);

 private:
  PP_Instance pp_instance_;

  typedef std::map<std::string, void*> InterfaceNameToObjectMap;
  InterfaceNameToObjectMap interface_name_to_objects_;
};

}  // namespace pp

/**
 * @}
 * End addtogroup CPP
 */
#endif  // PPAPI_CPP_INSTANCE_H_
