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


// This file defines the ClientInfo class.

#ifndef O3D_CORE_CROSS_CLIENT_INFO_H_
#define O3D_CORE_CROSS_CLIENT_INFO_H_

#include "core/cross/types.h"
#include "core/cross/service_locator.h"
#include "core/cross/service_implementation.h"

namespace o3d {

// This class is used to report infomation about the client.
class ClientInfo {
 public:
  ClientInfo();

  // The number of objects the client is currently tracking.
  int num_objects() const {
    return num_objects_;
  }

  // The amount of texture memory used.
  int texture_memory_used() const {
    return texture_memory_used_;
  };

  // The amount of texture memory used.
  int buffer_memory_used() const {
    return buffer_memory_used_;
  }

  // Whether or not we are using the software renderer.
  bool software_renderer() const {
    return software_renderer_;
  }

  // Whether or not shaders are GLSL.
  bool glsl() const {
#if defined(RENDERER_GLES2)
    return true;
#else
    return false;
#endif
  }

  // Whether render in 2d Mode
  bool render_2d() const {
#if defined(SUPPORT_CAIRO) && defined(FORCE_CAIRO)
    // Force Cairo o2d mode for testing purposes
    return true;
#elif defined(SUPPORT_CAIRO)
    // Some day when we have fallback from o3d to o2d,
    // this will have to support logic returning actual o2d vs. o3d mode.
    return false;
#else
    return false;
#endif
  }

  // Whether or not the underlying GPU supports non power of 2 textures.
  // NOTE: O3D always supports non power of 2 textures from a public API
  // point of view and massages the data underneath to make this work.
  // The point of this flag is mostly that if you are doing any kind of
  // dynamic texture updating, like video, then you might want to use a
  // power of two texture so O3D doesn't have to do extra work of converting
  // your NPOT texture into a POT texture behind the scenes.
  bool non_power_of_two_textures() const {
    return non_power_of_two_textures_;
  }

  // Gets the O3D version.
  const String& version() const {
    return version_;
  }

 private:
  friend class ClientInfoManager;

  int num_objects_;
  int texture_memory_used_;
  int buffer_memory_used_;
  bool software_renderer_;
  bool non_power_of_two_textures_;
  String version_;
};

// A class to manage the client info so other classes can easily look it up.
class ClientInfoManager {
 public:
  static const InterfaceId kInterfaceId;

  explicit ClientInfoManager(ServiceLocator* service_locator);

  const ClientInfo& client_info();

  // Adds or subtracts from the amount of texture memory used.
  void AdjustTextureMemoryUsed(int amount) {
    client_info_.texture_memory_used_ += amount;
    DCHECK(client_info_.texture_memory_used_ >= 0);
  }

  // Adds or subtracts from the amount of texture memory used.
  void AdjustBufferMemoryUsed(int amount) {
    client_info_.buffer_memory_used_ += amount;
    DCHECK(client_info_.buffer_memory_used_ >= 0);
  }

  void SetSoftwareRenderer(bool used) {
    client_info_.software_renderer_ = used;
  }

  void SetNonPowerOfTwoTextures(bool npot) {
    client_info_.non_power_of_two_textures_ = npot;
  }

private:
  ServiceImplementation<ClientInfoManager> service_;

  ClientInfo client_info_;

  DISALLOW_COPY_AND_ASSIGN(ClientInfoManager);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_CLIENT_INFO_H_
