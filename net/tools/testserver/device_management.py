#!/usr/bin/python2.5
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A bare-bones test server for testing cloud policy support.

This implements a simple cloud policy test server that can be used to test
chrome's device management service client. The policy information is read from
the file named device_management in the server's data directory. It contains
enforced and recommended policies for the device and user scope, and a list
of managed users.

The format of the file is JSON. The root dictionary contains a list under the
key "managed_users". It contains auth tokens for which the server will claim
that the user is managed. The token string "*" indicates that all users are
claimed to be managed. Other keys in the root dictionary identify request
scopes. Each request scope is described by a dictionary that holds two
sub-dictionaries: "mandatory" and "recommended". Both these hold the policy
definitions as key/value stores, their format is identical to what the Linux
implementation reads from /etc.

Example:

{
  "chromeos/device": {
    "mandatory": {
      "HomepageLocation" : "http://www.chromium.org"
    },
    "recommended": {
      "JavascriptEnabled": false,
    },
  },
  "managed_users": [
    "secret123456"
  ]
}


"""

import cgi
import logging
import os
import random
import re
import sys
import time
import tlslite
import tlslite.api

# The name and availability of the json module varies in python versions.
try:
  import simplejson as json
except ImportError:
  try:
    import json
  except ImportError:
    json = None

import device_management_backend_pb2 as dm
import cloud_policy_pb2 as cp


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
      self._params = cgi.parse_qs(self._path[self._path.find('?') + 1:])

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
    elif request_type == 'cloud_policy':
      return self.ProcessCloudPolicyRequest(rmsg.cloud_policy_request)
    elif request_type == 'managed_check':
      return self.ProcessManagedCheck(rmsg.managed_check_request)
    else:
      return (400, 'Invalid request parameter')

  def CheckGoogleLogin(self):
    """Extracts the GoogleLogin auth token from the HTTP request, and
    returns it. Returns None if the token is not present.
    """
    match = re.match('GoogleLogin auth=(\\w+)',
                     self._headers.getheader('Authorization', ''))
    if not match:
      return None
    return match.group(1)

  def GetDeviceName(self):
    """Returns the name for the currently authenticated device based on its
    device id.
    """
    return 'chromeos-' + self.GetUniqueParam('deviceid')

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
    if not self.CheckGoogleLogin():
      return (403, 'No authorization')

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

  def ProcessManagedCheck(self, msg):
    """Handles a 'managed check' request.

    Queries the list of managed users and responds the client if their user
    is managed or not.

    Args:
      msg: The ManagedCheckRequest message received from the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    # Check the management token.
    auth = self.CheckGoogleLogin()
    if not auth:
      return (403, 'No authorization')

    managed_check_response = dm.ManagedCheckResponse()
    if ('*' in self._server.policy['managed_users'] or
        auth in self._server.policy['managed_users']):
      managed_check_response.mode = dm.ManagedCheckResponse.MANAGED;
    else:
      managed_check_response.mode = dm.ManagedCheckResponse.UNMANAGED;

    # Prepare and send the response.
    response = dm.DeviceManagementResponse()
    response.error = dm.DeviceManagementResponse.SUCCESS
    response.managed_check_response.CopyFrom(managed_check_response)

    self.DumpMessage('Response', response)

    return (200, response.SerializeToString())

  def ProcessPolicy(self, msg):
    """Handles a policy request.

    Checks for authorization, encodes the policy into protobuf representation
    and constructs the response.

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
    if msg.policy_scope in self._server.policy:
      policy = self._server.policy[msg.policy_scope]['mandatory']
      setting = response.policy_response.setting.add()
      setting.policy_key = 'chrome-policy'
      policy_value = dm.GenericSetting()
      for (key, value) in policy.iteritems():
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

  def SetProtobufMessageField(self, group_message, field, field_value):
    '''Sets a field in a protobuf message.

    Args:
      group_message: The protobuf message.
      field: The field of the message to set, it shuold be a member of
          group_message.DESCRIPTOR.fields.
      field_value: The value to set.
    '''
    if field.label == field.LABEL_REPEATED:
      assert type(field_value) == list
      assert field.type == field.TYPE_STRING
      list_field = group_message.__getattribute__(field.name)
      for list_item in field_value:
        list_field.append(list_item)
    else:
      # Simple cases:
      if field.type == field.TYPE_BOOL:
        assert type(field_value) == bool
      elif field.type == field.TYPE_STRING:
        assert type(field_value) == str
      elif field.type == field.TYPE_INT64:
        assert type(field_value) == int
      else:
        raise Exception('Unknown field type %s' % field.type_name)
      group_message.__setattr__(field.name, field_value)

  def GatherPolicySettings(self, settings, policies):
    '''Copies all the policies from a dictionary into a protobuf of type
    CloudPolicySettings.

    Args:
      settings: The destination: a CloudPolicySettings protobuf.
      policies: The source: a dictionary containing policies under keys
          'recommended' and 'mandatory'.
    '''
    for group in settings.DESCRIPTOR.fields:
      # Create protobuf message for group.
      group_message = eval('cp.' + group.message_type.name + '()')
      # We assume that this policy group will be recommended, and only switch
      # it to mandatory if at least one of its members is mandatory.
      group_message.policy_options.mode = cp.PolicyOptions.RECOMMENDED
      # Indicates if at least one field was set in |group_message|.
      got_fields = False
      # Iterate over fields of the message and feed them from the
      # policy config file.
      for field in group_message.DESCRIPTOR.fields:
        field_value = None
        if field.name in policies['mandatory']:
          group_message.policy_options.mode = cp.PolicyOptions.MANDATORY
          field_value = policies['mandatory'][field.name]
        elif field.name in policies['recommended']:
          field_value = policies['recommended'][field.name]
        if field_value != None:
          got_fields = True
          self.SetProtobufMessageField(group_message, field, field_value)
      if got_fields:
        settings.__getattribute__(group.name).CopyFrom(group_message)

  def ProcessCloudPolicyRequest(self, msg):
    """Handles a cloud policy request. (New protocol for policy requests.)

    Checks for authorization, encodes the policy into protobuf representation,
    signs it and constructs the repsonse.

    Args:
      msg: The CloudPolicyRequest message received from the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    token, response = self.CheckToken()
    if not token:
      return response

    settings = cp.CloudPolicySettings()

    if msg.policy_scope in self._server.policy:
      # Respond is only given if the scope is specified in the config file.
      # Normally 'chromeos/device' and 'chromeos/user' should be accepted.
      self.GatherPolicySettings(settings,
                                self._server.policy[msg.policy_scope])

    # Construct response
    signed_response = dm.SignedCloudPolicyResponse()
    signed_response.settings.CopyFrom(settings)
    signed_response.timestamp = int(time.time())
    signed_response.request_token = token;
    signed_response.device_name = self.GetDeviceName()

    cloud_response = dm.CloudPolicyResponse()
    cloud_response.signed_response = signed_response.SerializeToString()
    signed_data = cloud_response.signed_response
    cloud_response.signature = (
        self._server.private_key.hashAndSign(signed_data).tostring())
    for certificate in self._server.cert_chain:
      cloud_response.certificate_chain.append(
          certificate.writeBytes().tostring())

    response = dm.DeviceManagementResponse()
    response.error = dm.DeviceManagementResponse.SUCCESS
    response.cloud_policy_response.CopyFrom(cloud_response)

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
    dmtoken = None
    request_device_id = self.GetUniqueParam('deviceid')
    match = re.match('GoogleDMToken token=(\\w+)',
                     self._headers.getheader('Authorization', ''))
    if match:
      dmtoken = match.group(1)
    if not dmtoken:
      error = dm.DeviceManagementResponse.DEVICE_MANAGEMENT_TOKEN_INVALID
    elif (not request_device_id or
          not self._server.LookupDevice(dmtoken) == request_device_id):
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

  def __init__(self, policy_path, policy_cert_chain):
    """Initializes the server.

    Args:
      policy_path: Names the file to read JSON-formatted policy from.
      policy_cert_chain: List of paths to X.509 certificate files of the
          certificate chain used for signing responses.
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

    self.private_key = None
    self.cert_chain = []
    for cert_path in policy_cert_chain:
      try:
        cert_text = open(cert_path).read()
      except IOError:
        print 'Failed to load certificate from %s' % cert_path
      certificate = tlslite.api.X509()
      certificate.parse(cert_text)
      self.cert_chain.append(certificate)
      if self.private_key is None:
        self.private_key = tlslite.api.parsePEMKey(cert_text, private=True)
        assert self.private_key != None

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
    dmtoken = ''.join(dmtoken_chars)
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
