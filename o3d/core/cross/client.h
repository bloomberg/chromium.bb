/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the class definition of the o3d::Client,
// the main entry point to O3D.

#ifndef O3D_CORE_CROSS_CLIENT_H_
#define O3D_CORE_CROSS_CLIENT_H_

#include <build/build_config.h>
#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "core/cross/service_dependency.h"
#include "core/cross/error_status.h"
#include "core/cross/draw_list_manager.h"
#include "core/cross/counter_manager.h"
#include "core/cross/object_manager.h"
#include "core/cross/semantic_manager.h"
#include "core/cross/transformation_context.h"
#include "core/cross/render_node.h"
#include "core/cross/callback.h"
#include "core/cross/event.h"
#include "core/cross/event_callback.h"
#include "core/cross/event_manager.h"
#include "core/cross/lost_resource_callback.h"
#include "core/cross/render_event.h"
#include "core/cross/render_surface.h"
#include "core/cross/tick_event.h"
#include "core/cross/timer.h"
#include "core/cross/timingtable.h"
#include "core/cross/transform.h"

namespace o3d {
class MessageQueue;
class Profiler;
class State;
class Pack;

// The Client class is the main point of entry to O3D.  It defines methods
// for creating and deleting packs and internal use only methods for creating
// most objects. Each new object created by the Client is assigned a unique ID
// which can be used to efficiently retrieve the object using the appropriate
// Get*ById() method.
//
// The Client has a root transform for the transform graph and a root render
// node for the render graph.
class Client {
  friend class ObjectBase;
  friend class ParamObject;

 public:

  explicit Client(ServiceLocator* service_locator);
  ~Client();

  typedef NonRecursiveCallback1Manager<const RenderEvent&>
      RenderCallbackManager;
  typedef RenderCallbackManager::CallbackType RenderCallback;

  // Name of the default pack that the default rendergraph rendernodes belong
  // to.
  static const char* kDefaultPackName;

  Id id() const {
    return id_;
  }

  // Sets the renderer to be used for all platform-specific graphics
  // methods and sets up the default rendergraph
  void Init();

  // Cleansup certain things in preparation for unloading the plugin.
  // This is for Javascript because there are certain conditions, the Render
  // callback for example, which can cause a javascript error in the browser
  // while the page is being unloaded. This function, if called during
  // window.onunload, handles those cases.
  void Cleanup();

  // Pack methods --------------------------

  // Creates a pack object, and registers it within the Client's internal
  // dictionary strutures.  Note that multiple packs may share the same name.
  // The system does not enforce pack name uniqueness.
  // Returns:
  //  A smart-pointer reference to the newly created pack object.
  Pack* CreatePack();

  // Node methods --------------------------

  // Returns the transform graph root transform
  // Parameters:
  //  None
  // Returns:
  //  A pointer to the transform graph root Transform
  inline Transform* root() const {
    return root_.Get();
  }

  // RenderNode methods --------------------------

  enum RenderMode {
    RENDERMODE_CONTINUOUS,  // Draw as often as possible up to refresh rate.
    RENDERMODE_ON_DEMAND,   // Draw once, then only when the OS requests it
                            //    (like uncovering part of a window.)
  };

  RenderMode render_mode() const {
    return render_mode_;
  }

  void set_render_mode(RenderMode render_mode);

  // Returns the rendergraph root render node.
  // Parameters:
  //  None.
  // Returns:
  //  A pointer to the rendergraph root rendernode.
  inline RenderNode* render_graph_root() const {
    return rendergraph_root_;
  }

  // Searches the entire Client's rendernode dictionary for rendernodes that
  // match the given name. It will find rendernodes created by the Client
  // regardless of whether or not they are part of the rendergraph.
  // Parameters:
  //  name: Node name to look for.
  //  render_nodes: RenderNodeArray to receive list of nodes. It anything is in
  //                the array will be cleared.
  void GetRenderNodesFast(const String& name,
                          RenderNodeArray* render_nodes) const;

  // Renders a subtree of the rendergraph.
  // Parameters:
  //  tree_root: The root of the subtree to be drawn.
  // Returns:
  //  Nothing
  void RenderTree(RenderNode *tree_root);

  // Sets the render callback.
  // NOTE: The client takes ownership of the RenderCallback you pass in. It will
  // be deleted if you call SetRenderCallback a second time or if you call
  // ClearRenderCallback
  //
  // Parameters:
  //   render_callback: RenderCallback to call each frame.
  void SetRenderCallback(RenderCallback* render_callback);

  // Clears the render callback
  // NOTE: The client takes ownership of the RenderCallback you pass in to
  // SetRenderCallback. It will be deleted if you call SetRenderCallback a
  // second time or if you call ClearRenderCallback
  void ClearRenderCallback();

  // Sets the callback for a events of a supplied type.
  // NOTE: The client takes ownership of the EventCallback you pass in. It will
  // be deleted if you call SetEventCallback a second time for the same event
  // type or if you call ClearEventCallback for that type.
  //
  // Parameters:
  //   event_callback: EventCallback to call each time an event of the right
  //                   type occurs.
  //   type: Type of event this callback handles.
  void SetEventCallback(Event::Type type, EventCallback* render_callback);
  void SetEventCallback(String type_name, EventCallback* render_callback);

  // Clears the callback for events of a given type.
  void ClearEventCallback(Event::Type type);
  void ClearEventCallback(String type_name);

  // Automatically drops some events to throttle event bandwidth.
  void AddEventToQueue(const Event& event);
  // Adds a resize event to the queue.
  void SendResizeEvent(int width, int height, bool fullscreen);

  // Sets the lost resources callback.
  // NOTE: The client takes ownership of the LostResourcesCallback you pass in.
  // It will be deleted if you call SetLostResourcesCallback a second time or if
  // you call ClearLostResourcesCallback
  //
  // Parameters:
  //   callback: LostResourcesCallback to call each frame.
  void SetLostResourcesCallback(LostResourcesCallback* callback);

  // Clears the lost resources callback
  // NOTE: The client takes ownership of the LostResourcesCallback you pass in
  // to SetLostResourcesCallback. It will be deleted if you call
  // SetLostResourcesCallback a second time or if you call
  // ClearLostResourcesCallback.
  void ClearLostResourcesCallback();

  // Forces a render of the current scene if the current render mode is
  // RENDERMODE_ON_DEMAND.
  void Render();

  // Sets the post render callback.
  // NOTE: The client takes ownership of the RenderCallback you pass in. It will
  // be deleted if you call SetPostRenderCallback a second time or if you call
  // ClearPostRenderCallback
  //
  // Parameters:
  //   post_render_callback: RenderCallback to call at the end of
  //                         the render cycle in each frame.
  void SetPostRenderCallback(RenderCallback* post_render_callback);

  // Clears the post render callback
  // NOTE: The client takes ownership of the RenderCallback you pass in to
  // SetPostRenderCallback. It will be deleted if you call post
  // SetRenderCallback a second time or if you call ClearRenderCallback.
  void ClearPostRenderCallback();

  // Updates the current state of the objects handled by the Client and
  // processes any messages found in the message queue then renders the client.
  // This is the function anything hosting the client, like a plugin, should
  // call to render.
  // Parameters:
  //   send_callback : whether to make the javascript render callback.
  //       Generally you want to pass true, but if the render is happening
  //       in non-windowed mode (eg on a Mac) and is in response to an update
  //       event rather than a timer, it can be useful to pass false to prevent
  //       the javascript code triggering another update and causing an infinite
  //       calback loop. Case in point is the custom camera example, which
  //       modifies some HTML form text fields on render callback, which on
  //       Firefox causes a plugin invalidation and round and round we would
  //       go.
  void RenderClient(bool send_callback);

  // In some situations (on Mac OS X at least) calling the user's
  // render callback can cause OS events to be dispatched which cause
  // the plugin to become reentrant. Detect this at a higher level.
  bool IsRendering();

  // If Renderer::max_fps has been set in RENDERMODE_CONTINUOUS mode, we don't
  // draw on each tick but just let new textures drive the rendering. There is
  // only one exception: if we haven't received any new textures for a while, we
  // still need to draw in order to trigger rendering callback. Since there
  // might be some UI depends on rendering callback.
  // This function determines if this has happened and if we need a draw.
  bool NeedsContinuousRender();

  // Sets the texture to use when a Texture or Sampler is missing while
  // rendering. If you set it to NULL you'll get an error if you try to render
  // something that is missing a needed Texture, Sampler or ParamSampler
  // Parameters:
  //   texture: texture to use for missing texture or NULL.
  void SetErrorTexture(Texture* texture);

  // Tick Methods ----------------------------

  typedef NonRecursiveCallback1Manager<const o3d::TickEvent&>
      TickCallbackManager;
  typedef TickCallbackManager::CallbackType TickCallback;

  // Sets the tick callback.
  // NOTE: The client takes ownership of the TickCallback you pass in. It will
  // be deleted if you call SetTickCallback a second time or if you call
  // ClearTickCallback.
  //
  // Parameters:
  //   tick_callback: TickCallback to call each time the client processes a
  //       tick.
  void SetTickCallback(TickCallback* tick_callback);

  // Clears the tick callback NOTE: The client takes ownership of the
  // TickCallback you pass in to SetTickCallback. It will be deleted if you call
  // SetTickCallback a second time or if you call ClearTickCallback.
  void ClearTickCallback();

  // Tick the client. This method is called by the plugin to give the client
  // a chance to process NaCl messages and update animation.
  // Returns:
  //   true if message check was ok.
  bool Tick();

  // Indicates whether a call to Tick() is in progress. This is needed
  // to avoid reentrancy problems on some platforms.
  bool IsTicking() const {
    return is_ticking_;
  }

  // Searches in the Client for an object by its id. This function is for
  // Javascript.
  // Parameters:
  //   id: id of object to look for.
  // Returns:
  //   Pointer to the object or NULL if not found.
  ObjectBase* GetObjectById(Id id) const {
    return object_manager_->GetObjectById(id);
  }

  // Searches the Client for objects of a particular name and type.
  // This function is for Javascript.
  // Parameters:
  //   name: name of object to look for.
  //   type_name: name of class to look for.
  // Returns:
  //   Array of raw pointers to the found objects.
  ObjectBaseArray GetObjects(const String& name,
                             const String& type_name) const {
    return object_manager_->GetObjects(name, type_name);
  }

  // Searches by id for an Object created by the Client. To search
  // for an object regardless of type use:
  //   Client::GetById<ObjectBase>(obj_id)
  // To search for an object of a specific type use:
  //   Client::GetById<Type>(obj_id)
  // for example, to search for Transform use:
  //   Client::GetById<Transform>(transform_id)
  // Parameters:
  //  id: Unique id of the object to search for
  // Returns:
  //  Pointer to the object with matching id or NULL if no object is found
  template<class T> T* GetById(Id id) const {
    return object_manager_->GetById<T>(id);
  }

  // Search the client for all objects of a certain class
  // Returns:
  //   Array of Pointers to the requested class.
  template<typename T>
  std::vector<T*> GetByClass() const {
    return object_manager_->GetByClass<T>();
  }

  // Search the client for all objects of a certain class
  // Parameters:
  //  class_type_name: the Class of the object. It is okay to pass base types
  //                   for example Node::GetApparentClass()->name will return
  //                   both Transforms and Shapes.
  // Returns:
  //   Array of Object Pointers.
  ObjectBaseArray GetObjectsByClassName(const String& class_type_name) const {
    return object_manager_->GetObjectsByClassName(class_type_name);
  }

  // Returns the socket address of the IMC message queue associated with the
  // Client.
  String GetMessageQueueAddress() const;

  // Error Methods ----------------

  typedef Callback1<const String&> ErrorCallback;

  // Sets the error callback. NOTE: The client takes ownership of the
  // ErrorCallback you pass in. It will be deleted if you call SetErrorCallback
  // a second time or if you call ClearErrorCallback.
  //
  // Parameters:
  //   error_callback: ErrorCallback to call each time the client gets
  //       an error.
  void SetErrorCallback(ErrorCallback* error_callback);

  // Clears the Error callback NOTE: The client takes ownership of the
  // ErrorCallback you pass in to SetErrorCallback. It will be deleted if you
  // call SetErrorCallback a second time or if you call ClearErrorCallback.
  void ClearErrorCallback();

  // Gets the last reported error.
  const String& GetLastError() const;

  // Clears the stored last error.
  void ClearLastError();

  // Parameter methods ------------------

  // Marks all parameters as so they will get re-evaluated
  void InvalidateAllParameters();

  // Profiling methods -------------------

  // Starts the timer ticking for the code range identified by key.
  void ProfileStart(const std::string& key);

  // Stops the timer for the code range identified by key.
  void ProfileStop(const std::string& key);

  // Resets the profiler, clearing out all data.
  void ProfileReset();

  // Dumps all profiler state to a string.
  String ProfileToString();

  // Reutrns a data: URL of the client area in png format.
  String ToDataURL();

  // This class is intended to be used on the stack, such that the variable gets
  // incremented on scope entry and decremented on scope exit.  It's currently
  // used in WindowProc to determine if we're reentrant or not, but may be
  // needed on other platforms as well.
  class ScopedIncrement {
   public:
    explicit ScopedIncrement(Client *client) {
      DCHECK(client);
      client_ = client;
      ++client_->calls_;
      DCHECK_GT(client_->calls_, 0);
    }
    int get() {
      DCHECK(client_);  // Don't call this after decrement!
      return client_->calls_;
    }
    void decrement() {
      if (client_) {
        DCHECK_GT(client_->calls_, 0);
        --client_->calls_;
        client_ = NULL;
      }
    }
    ~ScopedIncrement() {
      decrement();
    }
   private:
    Client *client_;
    DISALLOW_COPY_AND_ASSIGN(ScopedIncrement);
  };

  // Offscreen rendering methods -------------------

  // Sets up this Client so that RenderClient will cause the rendering
  // results to go into the given surfaces.
  void SetOffscreenRenderingSurfaces(
      RenderSurface::Ref surface,
      RenderDepthStencilSurface::Ref depth_surface);

 private:
  // Renders the client.
  void RenderClientInner(bool present, bool send_callback);

  // Gets a screenshot.
  String GetScreenshotAsDataURL();

  // MessageQueue that allows external code to communicate with the Client.
  scoped_ptr<MessageQueue> message_queue_;

  ServiceLocator* service_locator_;
  ServiceDependency<ObjectManager> object_manager_;
  ErrorStatus error_status_;
  DrawListManager draw_list_manager_;
  CounterManager counter_manager_;
  TransformationContext transformation_context_;
  SemanticManager semantic_manager_;
  ServiceDependency<Profiler> profiler_;
  ServiceDependency<Renderer> renderer_;
  ServiceDependency<EvaluationCounter> evaluation_counter_;

  // RenderTree was called.
  bool render_tree_called_;

  // Render mode.
  RenderMode render_mode_;

  // Used for rendering control
  bool texture_on_hold_;

  // Render Callbacks.
  RenderCallbackManager render_callback_manager_;

  RenderCallbackManager post_render_callback_manager_;

  // Render Event to pass to the render callback.
  RenderEvent render_event_;

  // This class holds on to all the handlers and the event queue for standard
  // JavaScript IO events.
  EventManager event_manager_;

  // Timer for getting the elapsed time between render updates.
  ElapsedTimeTimer render_elapsed_time_timer_;

  // Tick Callback.
  TickCallbackManager tick_callback_manager_;

  // Tick Event to pass to the tick callback.
  TickEvent tick_event_;

  // Timer for getting the elapsed time between tick updates.
  ElapsedTimeTimer tick_elapsed_time_timer_;

  // Whether a call to Tick() is currently active.
  bool is_ticking_;

  // Used to gather render time from mulitple RenderTree calls.
  float total_time_to_render_;

  // Time used for tick and message processing.
  float last_tick_time_;

  // Number of ticks since last reset.
  int tick_count_;

  // Reference to global transform graph root for Client.
  Transform::Ref root_;

  // Global Render Graph root for Client.
  RenderNode::Ref rendergraph_root_;

  ParamObject::Ref sas_param_object_;

  // The id of the client.
  Id id_;

  int calls_;  // Used to check reentrancy along with ScopedIncrement.

  RenderSurface::Ref offscreen_render_surface_;
  RenderDepthStencilSurface::Ref offscreen_depth_render_surface_;

  DISALLOW_COPY_AND_ASSIGN(Client);
};  // Client

}  // namespace o3d

#endif  // O3D_CORE_CROSS_CLIENT_H_
