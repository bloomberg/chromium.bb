// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_HOST_RESOLVER_DEV_H_
#define PPAPI_CPP_DEV_HOST_RESOLVER_DEV_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/dev/ppb_host_resolver_dev.h"
#include "ppapi/cpp/dev/net_address_dev.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

namespace pp {

class CompletionCallback;
class InstanceHandle;

/// The <code>HostResolver_Dev</code> class supports host name resolution.
///
/// Permissions: In order to run <code>Resolve()</code>, apps permission
/// <code>socket</code> with subrule <code>resolve-host</code> is required.
/// For more details about network communication permissions, please see:
/// http://developer.chrome.com/apps/app_network.html
class HostResolver_Dev : public Resource {
 public:
  /// Default constructor for creating an is_null()
  /// <code>HostResolver_Dev</code> object.
  HostResolver_Dev();

  /// A constructor used to create a <code>HostResolver_Dev</code> object.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit HostResolver_Dev(const InstanceHandle& instance);

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>PPB_HostResolver_Dev</code> resource.
  HostResolver_Dev(PassRef, PP_Resource resource);

  /// The copy constructor for <code>HostResolver_Dev</code>.
  ///
  /// @param[in] other A reference to another <code>HostResolver_Dev</code>.
  HostResolver_Dev(const HostResolver_Dev& other);

  /// The destructor.
  virtual ~HostResolver_Dev();

  /// The assignment operator for <code>HostResolver_Dev</code>.
  ///
  /// @param[in] other A reference to another <code>HostResolver_Dev</code>.
  ///
  /// @return A reference to this <code>HostResolver_Dev</code> object.
  HostResolver_Dev& operator=(const HostResolver_Dev& other);

  /// Static function for determining whether the browser supports the
  /// <code>PPB_HostResolver_Dev</code> interface.
  ///
  /// @return true if the interface is available, false otherwise.
  static bool IsAvailable();

  /// Requests resolution of a host name. If the call completes successully, the
  /// results can be retrieved by <code>GetCanonicalName()</code>,
  /// <code>GetNetAddressCount()</code> and <code>GetNetAddress()</code>.
  ///
  /// @param[in] host The host name (or IP address literal) to resolve.
  /// @param[in] port The port number to be set in the resulting network
  /// addresses.
  /// @param[in] hint A <code>PP_HostResolver_Hint_Dev</code> structure
  /// providing hints for host resolution.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  /// <code>PP_ERROR_NOACCESS</code> will be returned if the caller doesn't have
  /// required permissions. <code>PP_ERROR_NAME_NOT_RESOLVED</code> will be
  /// returned if the host name couldn't be resolved.
  int32_t Resolve(const char* host,
                  uint16_t port,
                  const PP_HostResolver_Hint_Dev& hint,
                  const CompletionCallback& callback);

  /// Gets the canonical name of the host.
  ///
  /// @return A string <code>Var</code> on success, which is an empty string
  /// if <code>PP_HOSTRESOLVER_FLAGS_CANONNAME</code> is not set in the hint
  /// flags when calling <code>Resolve()</code>; an undefined <code>Var</code>
  /// if there is a pending <code>Resolve()</code> call or the previous
  /// <code>Resolve()</code> call failed.
  Var GetCanonicalName() const;

  /// Gets the number of network addresses.
  ///
  /// @return The number of available network addresses on success; 0 if there
  /// is a pending <code>Resolve()</code> call or the previous
  /// <code>Resolve()</code> call failed.
  uint32_t GetNetAddressCount() const;

  /// Gets a network address.
  ///
  /// @param[in] index An index indicating which address to return.
  ///
  /// @return A <code>NetAddress_Dev</code> object. The object will be null
  /// (i.e., is_null() returns true) if there is a pending
  /// <code>Resolve()</code> call or the previous <code>Resolve()</code> call
  /// failed, or the specified index is out of range.
  NetAddress_Dev GetNetAddress(uint32_t index) const;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_HOST_RESOLVER_DEV_H_
