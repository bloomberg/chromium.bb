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
scopes. The user-request scope is described by a dictionary that holds two
sub-dictionaries: "mandatory" and "recommended". Both these hold the policy
definitions as key/value stores, their format is identical to what the Linux
implementation reads from /etc.
The device-scope holds the policy-definition directly as key/value stores in the
protobuf-format.

Example:

{
  "google/chromeos/device" : {
      "guest_mode_enabled" : false
  },
  "google/chromeos/user" : {
     "mandatory" : {
      "HomepageLocation" : "http://www.chromium.org",
      "IncognitoEnabled" : false
    },
     "recommended" : {
      "JavascriptEnabled": false
    }
  },
  "managed_users" : [
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
import tlslite.utils

# The name and availability of the json module varies in python versions.
try:
  import simplejson as json
except ImportError:
  try:
    import json
  except ImportError:
    json = None

import asn1der
import device_management_backend_pb2 as dm
import cloud_policy_pb2 as cp
import chrome_device_policy_pb2 as dp

# ASN.1 object identifier for PKCS#1/RSA.
PKCS1_RSA_OID = '\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01'

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

    logging.debug('gaia auth token -> ' +
                  self._headers.getheader('Authorization', ''))
    logging.debug('oauth token -> ' + str(self.GetUniqueParam('oauth_token')))
    logging.debug('deviceid -> ' + str(self.GetUniqueParam('deviceid')))
    self.DumpMessage('Request', rmsg)

    request_type = self.GetUniqueParam('request')
    # Check server side requirements, as defined in
    # device_management_backend.proto.
    if (self.GetUniqueParam('devicetype') != '2' or
        self.GetUniqueParam('apptype') != 'Chrome' or
        (request_type != 'ping' and
         len(self.GetUniqueParam('deviceid')) >= 64) or
        len(self.GetUniqueParam('agent')) >= 64):
      return (400, 'Invalid request parameter')
    if request_type == 'register':
      return self.ProcessRegister(rmsg.register_request)
    elif request_type == 'unregister':
      return self.ProcessUnregister(rmsg.unregister_request)
    elif request_type == 'policy' or request_type == 'ping':
      return self.ProcessPolicy(rmsg.policy_request, request_type)
    else:
      return (400, 'Invalid request parameter')

  def CheckGoogleLogin(self):
    """Extracts the auth token from the request and returns it. The token may
    either be a GoogleLogin token from an Authorization header, or an OAuth V2
    token from the oauth_token query parameter. Returns None if no token is
    present.
    """
    oauth_token = self.GetUniqueParam('oauth_token')
    if oauth_token:
      return oauth_token

    match = re.match('GoogleLogin auth=(\\w+)',
                     self._headers.getheader('Authorization', ''))
    if match:
      return match.group(1)

    return None

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
    auth = self.CheckGoogleLogin()
    if not auth:
      return (403, 'No authorization')

    policy = self._server.GetPolicies()
    if ('*' not in policy['managed_users'] and
        auth not in policy['managed_users']):
      return (403, 'Unmanaged')

    device_id = self.GetUniqueParam('deviceid')
    if not device_id:
      return (400, 'Missing device identifier')

    token_info = self._server.RegisterDevice(device_id,
                                             msg.machine_id,
                                             msg.type)

    # Send back the reply.
    response = dm.DeviceManagementResponse()
    response.register_response.device_management_token = (
        token_info['device_token'])
    response.register_response.machine_name = token_info['machine_name']

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
    response.unregister_response.CopyFrom(dm.DeviceUnregisterResponse())

    self.DumpMessage('Response', response)

    return (200, response.SerializeToString())

  def ProcessPolicy(self, msg, request_type):
    """Handles a policy request.

    Checks for authorization, encodes the policy into protobuf representation
    and constructs the response.

    Args:
      msg: The DevicePolicyRequest message received from the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """
    for request in msg.request:
      if (request.policy_type in
             ('google/chromeos/user', 'google/chromeos/device')):
        if request_type != 'policy':
          return (400, 'Invalid request type')
        else:
          return self.ProcessCloudPolicy(request)
      else:
        return (400, 'Invalid policy_type')

  def SetProtobufMessageField(self, group_message, field, field_value):
    '''Sets a field in a protobuf message.

    Args:
      group_message: The protobuf message.
      field: The field of the message to set, it should be a member of
          group_message.DESCRIPTOR.fields.
      field_value: The value to set.
    '''
    if field.label == field.LABEL_REPEATED:
      assert type(field_value) == list
      entries = group_message.__getattribute__(field.name)
      for list_item in field_value:
        entries.append(list_item)
      return
    elif field.type == field.TYPE_BOOL:
      assert type(field_value) == bool
    elif field.type == field.TYPE_STRING:
      assert type(field_value) == str or type(field_value) == unicode
    elif field.type == field.TYPE_INT64:
      assert type(field_value) == int
    elif (field.type == field.TYPE_MESSAGE and
          field.message_type.name == 'StringList'):
      assert type(field_value) == list
      entries = group_message.__getattribute__(field.name).entries
      for list_item in field_value:
        entries.append(list_item)
      return
    else:
      raise Exception('Unknown field type %s' % field.type)
    group_message.__setattr__(field.name, field_value)

  def GatherDevicePolicySettings(self, settings, policies):
    '''Copies all the policies from a dictionary into a protobuf of type
    CloudDeviceSettingsProto.

    Args:
      settings: The destination ChromeDeviceSettingsProto protobuf.
      policies: The source dictionary containing policies in JSON format.
    '''
    for group in settings.DESCRIPTOR.fields:
      # Create protobuf message for group.
      group_message = eval('dp.' + group.message_type.name + '()')
      # Indicates if at least one field was set in |group_message|.
      got_fields = False
      # Iterate over fields of the message and feed them from the
      # policy config file.
      for field in group_message.DESCRIPTOR.fields:
        field_value = None
        if field.name in policies:
          got_fields = True
          field_value = policies[field.name]
          self.SetProtobufMessageField(group_message, field, field_value)
      if got_fields:
        settings.__getattribute__(group.name).CopyFrom(group_message)

  def GatherUserPolicySettings(self, settings, policies):
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

  def ProcessCloudPolicy(self, msg):
    """Handles a cloud policy request. (New protocol for policy requests.)

    Checks for authorization, encodes the policy into protobuf representation,
    signs it and constructs the repsonse.

    Args:
      msg: The CloudPolicyRequest message received from the client.

    Returns:
      A tuple of HTTP status code and response data to send to the client.
    """

    token_info, error = self.CheckToken()
    if not token_info:
      return error

    # Response is only given if the scope is specified in the config file.
    # Normally 'google/chromeos/device' and 'google/chromeos/user' should be
    # accepted.
    policy = self._server.GetPolicies()
    policy_value = ''
    if (msg.policy_type in token_info['allowed_policy_types'] and
        msg.policy_type in policy):
      if msg.policy_type == 'google/chromeos/user':
        settings = cp.CloudPolicySettings()
        self.GatherUserPolicySettings(settings,
                                      policy[msg.policy_type])
        policy_value = settings.SerializeToString()
      elif msg.policy_type == 'google/chromeos/device':
        settings = dp.ChromeDeviceSettingsProto()
        self.GatherDevicePolicySettings(settings,
                                        policy[msg.policy_type])
        policy_value = settings.SerializeToString()

    # Figure out the key we want to use. If multiple keys are configured, the
    # server will rotate through them in a round-robin fashion.
    signing_key = None
    req_key = None
    key_version = 1
    nkeys = len(self._server.keys)
    if msg.signature_type == dm.PolicyFetchRequest.SHA1_RSA and nkeys > 0:
      if msg.public_key_version in range(1, nkeys + 1):
        # requested key exists, use for signing and rotate.
        req_key = self._server.keys[msg.public_key_version - 1]['private_key']
        key_version = (msg.public_key_version % nkeys) + 1
      signing_key = self._server.keys[key_version - 1]

    # Fill the policy data protobuf.
    policy_data = dm.PolicyData()
    policy_data.policy_type = msg.policy_type
    policy_data.timestamp = int(time.time() * 1000)
    policy_data.request_token = token_info['device_token'];
    policy_data.policy_value = policy_value
    policy_data.machine_name = token_info['machine_name']
    if signing_key:
      policy_data.public_key_version = key_version
    policy_data.username = self._server.username
    policy_data.device_id = token_info['device_id']
    signed_data = policy_data.SerializeToString()

    response = dm.DeviceManagementResponse()
    fetch_response = response.policy_response.response.add()
    fetch_response.policy_data = signed_data
    if signing_key:
      fetch_response.policy_data_signature = (
          signing_key['private_key'].hashAndSign(signed_data).tostring())
      if msg.public_key_version != key_version:
        fetch_response.new_public_key = signing_key['public_key']
        if req_key:
          fetch_response.new_public_key_signature = (
              req_key.hashAndSign(fetch_response.new_public_key).tostring())

    self.DumpMessage('Response', response)

    return (200, response.SerializeToString())

  def CheckToken(self):
    """Helper for checking whether the client supplied a valid DM token.

    Extracts the token from the request and passed to the server in order to
    look up the client.

    Returns:
      A pair of token information record and error response. If the first
      element is None, then the second contains an error code to send back to
      the client. Otherwise the first element is the same structure that is
      returned by LookupToken().
    """
    error = 500
    dmtoken = None
    request_device_id = self.GetUniqueParam('deviceid')
    match = re.match('GoogleDMToken token=(\\w+)',
                     self._headers.getheader('Authorization', ''))
    if match:
      dmtoken = match.group(1)
    if not dmtoken:
      error = 401
    else:
      token_info = self._server.LookupToken(dmtoken)
      if (not token_info or
          not request_device_id or
          token_info['device_id'] != request_device_id):
        error = 410
      else:
        return (token_info, None)

    logging.debug('Token check failed with error %d' % error)

    return (None, (error, 'Server error %d' % error))

  def DumpMessage(self, label, msg):
    """Helper for logging an ASCII dump of a protobuf message."""
    logging.debug('%s\n%s' % (label, str(msg)))

class TestServer(object):
  """Handles requests and keeps global service state."""

  def __init__(self, policy_path, private_key_paths, policy_user):
    """Initializes the server.

    Args:
      policy_path: Names the file to read JSON-formatted policy from.
      private_key_paths: List of paths to read private keys from.
    """
    self._registered_tokens = {}
    self.policy_path = policy_path

    # There is no way to for the testserver to know the user name belonging to
    # the GAIA auth token we received (short of actually talking to GAIA). To
    # address this, we have a command line parameter to set the username that
    # the server should report to the client.
    self.username = policy_user

    self.keys = []
    if private_key_paths:
      # Load specified keys from the filesystem.
      for key_path in private_key_paths:
        try:
          key = tlslite.api.parsePEMKey(open(key_path).read(), private=True)
        except IOError:
          print 'Failed to load private key from %s' % key_path
          continue

        assert key != None
        self.keys.append({ 'private_key' : key })
    else:
      # Generate a key if none were specified.
      key = tlslite.api.generateRSAKey(1024)
      assert key != None
      self.keys.append({ 'private_key' : key })

    # Derive the public keys from the loaded private keys.
    for entry in self.keys:
      key = entry['private_key']

      algorithm = asn1der.Sequence(
          [ asn1der.Data(asn1der.OBJECT_IDENTIFIER, PKCS1_RSA_OID),
            asn1der.Data(asn1der.NULL, '') ])
      rsa_pubkey = asn1der.Sequence([ asn1der.Integer(key.n),
                                      asn1der.Integer(key.e) ])
      pubkey = asn1der.Sequence([ algorithm, asn1der.Bitstring(rsa_pubkey) ])
      entry['public_key'] = pubkey;

  def GetPolicies(self):
    """Returns the policies to be used, reloaded form the backend file every
       time this is called.
    """
    policy = {}
    if json is None:
      print 'No JSON module, cannot parse policy information'
    else :
      try:
        policy = json.loads(open(self.policy_path).read())
      except IOError:
        print 'Failed to load policy from %s' % self.policy_path
    return policy

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

  def RegisterDevice(self, device_id, machine_id, type):
    """Registers a device or user and generates a DM token for it.

    Args:
      device_id: The device identifier provided by the client.

    Returns:
      The newly generated device token for the device.
    """
    dmtoken_chars = []
    while len(dmtoken_chars) < 32:
      dmtoken_chars.append(random.choice('0123456789abcdef'))
    dmtoken = ''.join(dmtoken_chars)
    allowed_policy_types = {
      dm.DeviceRegisterRequest.USER: ['google/chromeos/user'],
      dm.DeviceRegisterRequest.DEVICE: ['google/chromeos/device'],
      dm.DeviceRegisterRequest.TT: ['google/chromeos/user'],
    }
    self._registered_tokens[dmtoken] = {
      'device_id': device_id,
      'device_token': dmtoken,
      'allowed_policy_types': allowed_policy_types[type],
      'machine_name': 'chromeos-' + machine_id,
    }
    return self._registered_tokens[dmtoken]

  def LookupToken(self, dmtoken):
    """Looks up a device or a user by DM token.

    Args:
      dmtoken: The device management token provided by the client.

    Returns:
      A dictionary with information about a device or user that is registered by
      dmtoken, or None if the token is not found.
    """
    return self._registered_tokens.get(dmtoken, None)

  def UnregisterDevice(self, dmtoken):
    """Unregisters a device identified by the given DM token.

    Args:
      dmtoken: The device management token provided by the client.
    """
    if dmtoken in self._registered_tokens.keys():
      del self._registered_tokens[dmtoken]
