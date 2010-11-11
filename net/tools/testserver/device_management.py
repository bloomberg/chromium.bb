#!/usr/bin/python2.5
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A bare-bones test server for testing cloud policy support.

This implements a simple cloud policy test server that can be used to test
chrome's device management service client. The policy information is read from
from files in a directory. The files should contain policy definitions in JSON
format, using the top-level dictionary as a key/value store. The format is
identical to what the Linux implementation reads from /etc. Here is an example:

{
  "HomepageLocation" : "http://www.chromium.org"
}

"""

import cgi
import logging
import random
import re
import sys

# The name and availability of the json module varies in python versions.
try:
  import simplejson as json
except ImportError:
  try:
    import json
  except ImportError:
    json = None

import device_management_backend_pb2 as dm

class RequestHandler(object):
  """Decodes and handles device management requests from clients.

  The handler implements all the request parsing and protobuf message decoding
  and encoding. It calls back into the server to lookup, register, and
  unregister clients.
  """

  def __init__(self, server, path, headers, request):
    """Initialize the handler.

    Args:
      server: The TestServer object to use for (un)registering clients.
      path: A string containing the request path and query parameters.
      headers: A rfc822.Message-like object containing HTTP headers.
      request: The request data received from the client as a string.
    """
    self._server = server
    self._path = path
    self._headers = headers
    self._request = request
    self._params = None

  def GetUniqueParam(self, name):
    """Extracts a unique query parameter from the request.

    Args:
      name: Names the parameter to fetch.
    Returns:
      The parameter value or None if the parameter doesn't exist or is not
      unique.
    """
    if not self._params:
      self._params = cgi.parse_qs(self._path[self._path.find('?')+1:])

    param_list = self._params.get(name, [])
    if len(param_list) == 1:
      return param_list[0]
    return None;

  def HandleRequest(self):
    """Handles a request.

    Parses the data supplied at construction time and returns a pair indicating
    http status code and response data to be sent back to the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    rmsg = dm.DeviceManagementRequest()
    rmsg.ParseFromString(self._request)

    self.DumpMessage('Request', rmsg)

    request_type = self.GetUniqueParam('request')
    if request_type == 'register':
      return self.ProcessRegister(rmsg.register_request)
    elif request_type == 'unregister':
      return self.ProcessUnregister(rmsg.unregister_request)
    elif request_type == 'policy':
      return self.ProcessPolicy(rmsg.policy_request)
    else:
      return (400, 'Invalid request parameter')

  def ProcessRegister(self, msg):
    """Handles a register request.

    Checks the query for authorization and device identifier, registers the
    device with the server and constructs a response.

    Args:
      msg: The DeviceRegisterRequest message received from the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    # Check the auth token and device ID.
    match = re.match('GoogleLogin auth=(\\w+)',
                     self._headers.getheader('Authorization', ''))
    if not match:
      return (403, 'No authorization')
    auth_token = match.group(1)

    device_id = self.GetUniqueParam('deviceid')
    if not device_id:
      return (400, 'Missing device identifier')

    # Register the device and create a token.
    dmtoken = self._server.RegisterDevice(device_id)

    # Send back the reply.
    response = dm.DeviceManagementResponse()
    response.error = dm.DeviceManagementResponse.SUCCESS
    response.register_response.device_management_token = dmtoken

    self.DumpMessage('Response', response)

    return (200, response.SerializeToString())

  def ProcessUnregister(self, msg):
    """Handles a register request.

    Checks for authorization, unregisters the device and constructs the
    response.

    Args:
      msg: The DeviceUnregisterRequest message received from the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    # Check the management token.
    token, response = self.CheckToken();
    if not token:
      return response

    # Unregister the device.
    self._server.UnregisterDevice(token);

    # Prepare and send the response.
    response = dm.DeviceManagementResponse()
    response.error = dm.DeviceManagementResponse.SUCCESS
    response.unregister_response.CopyFrom(dm.DeviceUnregisterResponse())

    self.DumpMessage('Response', response)

    return (200, response.SerializeToString())

  def ProcessPolicy(self, msg):
    """Handles a policy request.

    Checks for authorization, encodes the policy into protobuf representation
    and constructs the repsonse.

    Args:
      msg: The DevicePolicyRequest message received from the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    # Check the management token.
    token, response = self.CheckToken()
    if not token:
      return response

    # Stuff the policy dictionary into a response message and send it back.
    response = dm.DeviceManagementResponse()
    response.error = dm.DeviceManagementResponse.SUCCESS
    response.policy_response.CopyFrom(dm.DevicePolicyResponse())

    # Respond only if the client requested policy for the cros/device scope,
    # since that's where chrome policy is supposed to live in.
    if msg.policy_scope == 'cros/device':
      setting = response.policy_response.setting.add()
      setting.policy_key = 'chrome-policy'
      policy_value = dm.GenericSetting()
      for (key, value) in self._server.policy.iteritems():
        entry = policy_value.named_value.add()
        entry.name = key
        entry_value = dm.GenericValue()
        if isinstance(value, bool):
          entry_value.value_type = dm.GenericValue.VALUE_TYPE_BOOL
          entry_value.bool_value = value
        elif isinstance(value, int):
          entry_value.value_type = dm.GenericValue.VALUE_TYPE_INT64
          entry_value.int64_value = value
        elif isinstance(value, str) or isinstance(value, unicode):
          entry_value.value_type = dm.GenericValue.VALUE_TYPE_STRING
          entry_value.string_value = value
        elif isinstance(value, list):
          entry_value.value_type = dm.GenericValue.VALUE_TYPE_STRING_ARRAY
          for list_entry in value:
            entry_value.string_array.append(str(list_entry))
        entry.value.CopyFrom(entry_value)
      setting.policy_value.CopyFrom(policy_value)

    self.DumpMessage('Response', response)

    return (200, response.SerializeToString())

  def CheckToken(self):
    """Helper for checking whether the client supplied a valid DM token.

    Extracts the token from the request and passed to the server in order to
    look up the client. Returns a pair of token and error response. If the token
    is None, the error response is a pair of status code and error message.

    Returns:
      A pair of DM token and error response. If the token is None, the message
      will contain the error response to send back.
    """
    error = None

    match = re.match('GoogleDMToken token=(\\w+)',
                     self._headers.getheader('Authorization', ''))
    if match:
      dmtoken = match.group(1)
      if not dmtoken:
        error = dm.DeviceManagementResponse.DEVICE_MANAGEMENT_TOKEN_INVALID
      elif not self._server.LookupDevice(dmtoken):
        error = dm.DeviceManagementResponse.DEVICE_NOT_FOUND
      else:
        return (dmtoken, None)

    response = dm.DeviceManagementResponse()
    response.error = error

    self.DumpMessage('Response', response)

    return (None, (200, response.SerializeToString()))

  def DumpMessage(self, label, msg):
    """Helper for logging an ASCII dump of a protobuf message."""
    logging.debug('%s\n%s' % (label, str(msg)))

class TestServer(object):
  """Handles requests and keeps global service state."""

  def __init__(self, policy_path):
    """Initializes the server.

    Args:
      policy_path: Names the file to read JSON-formatted policy from.
    """
    self._registered_devices = {}
    self.policy = {}
    if json is None:
      print 'No JSON module, cannot parse policy information'
    else :
      try:
        self.policy = json.loads(open(policy_path).read())
      except IOError:
        print 'Failed to load policy from %s' % policy_path

  def HandleRequest(self, path, headers, request):
    """Handles a request.

    Args:
      path: The request path and query parameters received from the client.
      headers: A rfc822.Message-like object containing HTTP headers.
      request: The request data received from the client as a string.
    Returns:
      A pair of HTTP status code and response data to send to the client.
    """
    handler = RequestHandler(self, path, headers, request)
    return handler.HandleRequest()

  def RegisterDevice(self, device_id):
    """Registers a device and generate a DM token for it.

    Args:
      device_id: The device identifier provided by the client.

    Returns:
      The newly generated device token for the device.
    """
    dmtoken_chars = []
    while len(dmtoken_chars) < 32:
      dmtoken_chars.append(random.choice('0123456789abcdef'))
    dmtoken= ''.join(dmtoken_chars)
    self._registered_devices[dmtoken] = device_id
    return dmtoken

  def LookupDevice(self, dmtoken):
    """Looks up a device by DMToken.

    Args:
      dmtoken: The device management token provided by the client.

    Returns:
      The corresponding device identifier or None if not found.
    """
    return self._registered_devices.get(dmtoken, None)

  def UnregisterDevice(self, dmtoken):
    """Unregisters a device identified by the given DM token.

    Args:
      dmtoken: The device management token provided by the client.
    """
    if dmtoken in self._registered_devices:
      del self._registered_devices[dmtoken]
