// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_INSTANCE_H_
#define PPAPI_CPP_INSTANCE_H_

/// @file
/// Defines the C++ wrapper for a plugin instance.
///
/// @addtogroup CPP
/// @{

#include <map>
#include <string>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

struct PP_InputEvent;

/// The C++ interface to the Pepper API.
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
  /// Construction of an instance should only be done in response to a browser
  /// request in Module::CreateInstance. Otherwise, the instance will lack the
  /// proper bookkeeping in the browser and in the C++ wrapper.
  ///
  /// Init() will be called immediately after the constructor. This allows you
  /// to perform initialization tasks that can fail and to report that failure
  /// to the browser.
  explicit Instance(PP_Instance instance);

  /// When the instance is removed from the web page, the pp::Instance object
  /// will be deleted. You should never delete the Instance object yourself
  /// since the lifetime is handled by the C++ wrapper and is controlled by the
  /// browser's calls to the PPP_Instance interface.
  ///
  /// The PP_Instance identifier will still be valid during this call so the
  /// plugin can perform cleanup-related tasks. Once this function returns, the
  /// PP_Instance handle will be invalid. This means that you can't do any
  /// asynchronous operations like network requests or file writes from this
  /// destructor since they will be immediately canceled.
  ///
  /// <strong>Important note:</strong> This function may be skipped in certain
  /// circumstances when Chrome does "fast shutdown". Fast shutdown will happen
  /// in some cases when all plugin instances are being deleted, and no cleanup
  /// functions will be called. The module will just be unloaded and the process
  /// terminated.
  virtual ~Instance();

  /// Returns the PP_Instance identifying this object. When using the PPAPI C++
  /// wrappers this is not normally necessary, but is required when using the
  /// lower-level C APIs.
  PP_Instance pp_instance() const { return pp_instance_; }

  /// Initializes this plugin with the given arguments. This will be called
  /// immediately after the instance object is constructed.
  ///
  /// @param[in] argc The number of arguments contained in @a argn and @a argv.
  ///
  /// @param[in] argn An array of argument names.  These argument names are
  /// supplied in the <embed> tag, for example:
  /// <embed id="nacl_module" dimensions="2"> will produce two argument
  /// names: "id" and "dimensions."
  ///
  /// @param[in] argv An array of argument values.  These are the values of the
  /// arguments listed in the <embed> tag, for example
  /// <embed id="nacl_module" dimensions="2"> will produce two argument
  /// values: "nacl_module" and "2".  The indices of these values match the
  /// indices of the corresponding names in @a argn.
  ///
  /// @return True on success. Returning false causes the plugin
  /// instance to be deleted and no other functions to be called.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);

  /// @{
  /// @name PPP_Instance methods for the plugin to override:

  /// Notification that the position, the size, or the clip rectangle of the
  /// element in the browser that corresponds to this instance has changed.
  ///
  /// A typical implementation will check the size of the @a position argument
  /// and reallocate the graphics context when a different size is received.
  /// Note that this function will be called for scroll events where the size
  /// doesn't change, so you should always check that the size is actually
  /// different before doing any reallocations.
  ///
  /// @param[in] position The location on the page of the instance. This is
  /// relative to the top left corner of the viewport, which changes as the
  /// page is scrolled. Generally the size of this value will be used to create
  /// a graphics device, and the position is ignored (most things are relative
  /// to the instance so the absolute position isn't useful in most cases).
  ///
  /// @param[in] clip The visible region of the instance. This is relative to
  /// the top left of the plugin's coordinate system (not the page).  If the
  /// plugin is invisible, @a clip will be (0, 0, 0, 0).
  ///
  /// It's recommended to check for invisible instances and to stop
  /// generating graphics updates in this case to save system resources. It's
  /// not usually worthwhile, however, to generate partial updates according to
  /// the clip when the instance is partially visible. Instead, update the
  /// entire region. The time saved doing partial paints is usually not
  /// significant and it can create artifacts when scrolling (this notification
  /// is sent asynchronously from scolling so there can be flashes of old
  /// content in the exposed regions).
  virtual void DidChangeView(const Rect& position, const Rect& clip);

  /// Notification that the instance has gained or lost focus. Having focus
  /// means that keyboard events will be sent to the instance. An instance's
  /// default condition is that it will not have focus.
  ///
  /// Note: clicks on instances will give focus only if you handle the
  /// click event. Return @a true from HandleInputEvent to signal that the click
  /// event was handled. Otherwise the browser will bubble the event and give
  /// focus to the element on the page that actually did end up consuming it.
  /// If you're not getting focus, check to make sure you're returning true from
  /// the mouse click in HandleInputEvent.
  ///
  /// @param[in] has_focus Indicates the new focused state of the instance.
  virtual void DidChangeFocus(bool has_focus);

  /// Handles input events. Returns true if the event was handled or false if
  /// it was not. The default implementation does nothing and returns false.
  ///
  /// If the event was handled, it will not be forwarded to the web page or
  /// browser. If it was not handled, it will bubble according to the normal
  /// rules. So it is important that a module respond accurately with whether
  /// event propagation should continue.
  ///
  /// Event propagation also controls focus. If you handle an event like a mouse
  /// event, typically the instance will be given focus. Returning false means
  /// that the click will be given to a lower part of the page and your module
  /// will not receive focus. This allows an instance to be partially
  /// transparent, where clicks on the transparent areas will behave like clicks
  /// to the underlying page.
  ///
  /// @param[in] event The input event.
  ///
  /// @return true if @a event was handled, false otherwise.
  virtual bool HandleInputEvent(const PP_InputEvent& event);

  /// Notification of a data stream available after an instance was created
  /// based on the MIME type of a DOMWindow navigation. This only applies to
  /// modules that are pre-registered to handle certain MIME types. If you
  /// haven't specifically registered to handle a MIME type or aren't positive
  /// this applies to you, you can ignore this function. The default
  /// implementation just returns false.
  ///
  /// The given url_loader corresponds to a URLLoader object that is
  /// already opened. Its response headers may be queried using GetResponseInfo.
  /// If you want to use the URLLoader to read data, you will need to save a
  /// copy of it or the underlying resource will be freed when this function
  /// returns and the load will be canceled.
  ///
  /// This method returns false if the module cannot handle the data. In
  /// response to this method, the module should call ReadResponseBody to read
  /// the incoming data.
  ///
  /// @param[in] url_loader A PP_Resource an open PPB_URLLoader instance.
  ///
  /// @return true if the data was handled, false otherwise.
  virtual bool HandleDocumentLoad(const URLLoader& url_loader);

  /// @}

  /// @{
  /// @name PPB_Instance methods for querying the browser:

#ifndef PPAPI_INSTANCE_REMOVE_SCRIPTING
  /// See PPP_Instance.GetInstanceObject.
  virtual Var GetInstanceObject();

  /// See PPB_Instance.GetWindowObject.
  Var GetWindowObject();

  /// See PPB_Instance.GetOwnerElementObject.
  Var GetOwnerElementObject();

  /// See PPB_Instance.ExecuteScript.
  Var ExecuteScript(const Var& script, Var* exception = NULL);
#endif

  /// See PPB_Instance.BindGraphics.
  bool BindGraphics(const Graphics2D& graphics);

  /// See PPB_Instance.BindGraphics.
  bool BindGraphics(const Surface3D_Dev& graphics);

  /// See PPB_Instance.IsFullFrame.
  bool IsFullFrame();

  // These functions use the PPP_Messaging and PPB_Messaging interfaces, so that
  // messaging can be done conveniently for a pp::Instance without using a
  // separate C++ class.

  /// See PPP_Messaging.HandleMessage.
  virtual void HandleMessage(const Var& message);
  /// See PPB_Messaging.PostMessage.
  void PostMessage(const Var& message);

  /// @}

  /// Associates a plugin instance with an interface,
  /// creating an object... {PENDING: clarify!}
  ///
  /// Many optional interfaces are associated with a plugin instance. For
  /// example, the find in PPP_Find interface receives updates on a per-instance
  /// basis. This "per-instance" tracking allows such objects to associate
  /// themselves with an instance as "the" handler for that interface name.
  ///
  /// In the case of the find example, the find object registers with its
  /// associated instance in its constructor and unregisters in its destructor.
  /// Then whenever it gets updates with a PP_Instance parameter, it can
  /// map back to the find object corresponding to that given PP_Instance by
  /// calling GetPerInstanceObject.
  ///
  /// This lookup is done on a per-interface-name basis. This means you can
  /// only have one object of a given interface name associated with an
  /// instance.
  ///
  /// If you are adding a handler for an additional interface, be sure to
  /// register with the module (AddPluginInterface) for your interface name to
  /// get the C calls in the first place.
  ///
  /// @see RemovePerInstanceObject
  /// @see GetPerInstanceObject
  void AddPerInstanceObject(const std::string& interface_name, void* object);

  /// {PENDING: summarize Remove method here}
  ///
  /// @see AddPerInstanceObject
  void RemovePerInstanceObject(const std::string& interface_name, void* object);

  /// Look up an object previously associated with an instance. Returns NULL
  /// if the instance is invalid or there is no object for the given interface
  /// name on the instance.
  ///
  /// @see AddPerInstanceObject
  static void* GetPerInstanceObject(PP_Instance instance,
                                    const std::string& interface_name);

 private:
  PP_Instance pp_instance_;

  typedef std::map<std::string, void*> InterfaceNameToObjectMap;
  InterfaceNameToObjectMap interface_name_to_objects_;
};

}  // namespace pp

/// @}
/// End addtogroup CPP

#endif  // PPAPI_CPP_INSTANCE_H_
