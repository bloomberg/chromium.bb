# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The metaclasses used by the mojo python bindings."""

import itertools
import logging
import sys

# pylint: disable=F0401
import mojo_bindings.messaging as messaging
import mojo_bindings.promise as promise
import mojo_bindings.serialization as serialization


class MojoEnumType(type):
  """Meta class for enumerations.

  Usage:
    class MyEnum(object):
      __metaclass__ = MojoEnumType
      VALUES = [
        ('A', 0),
        'B',
        ('C', 5),
      ]

      This will define a enum with 3 values, 'A' = 0, 'B' = 1 and 'C' = 5.
  """

  def __new__(mcs, name, bases, dictionary):
    dictionary['__slots__'] = ()
    dictionary['__new__'] = None
    for value in dictionary.pop('VALUES', []):
      if not isinstance(value, tuple):
        raise ValueError('incorrect value: %r' % value)
      key, enum_value = value
      if isinstance(key, str) and isinstance(enum_value, int):
        dictionary[key] = enum_value
      else:
        raise ValueError('incorrect value: %r' % value)
    return type.__new__(mcs, name, bases, dictionary)

  def __setattr__(cls, key, value):
    raise AttributeError('can\'t set attribute')

  def __delattr__(cls, key):
    raise AttributeError('can\'t delete attribute')


class MojoStructType(type):
  """Meta class for structs.

  Usage:
    class MyStruct(object):
      __metaclass__ = MojoStructType
      DESCRIPTOR = {
        'constants': {
          'C1': 1,
          'C2': 2,
        },
        'enums': {
          'ENUM1': [
            ('V1', 1),
            'V2',
          ],
          'ENUM2': [
            ('V1', 1),
            'V2',
          ],
        },
        'fields': [
           SingleFieldGroup('x', _descriptor.TYPE_INT32, 0, 0),
        ],
      }

      This will define an struct, with:
      - 2 constants 'C1' and 'C2';
      - 2 enums 'ENUM1' and 'ENUM2', each of those having 2 values, 'V1' and
        'V2';
      - 1 int32 field named 'x'.
  """

  def __new__(mcs, name, bases, dictionary):
    dictionary['__slots__'] = ('_fields')
    descriptor = dictionary.pop('DESCRIPTOR', {})

    # Add constants
    dictionary.update(descriptor.get('constants', {}))

    # Add enums
    enums = descriptor.get('enums', {})
    for key in enums:
      dictionary[key] = MojoEnumType(key, (object,), { 'VALUES': enums[key] })

    # Add fields
    groups = descriptor.get('fields', [])

    fields = list(
        itertools.chain.from_iterable([group.descriptors for group in groups]))
    fields.sort(key=lambda f: f.index)
    for field in fields:
      dictionary[field.name] = _BuildProperty(field)

    # Add init
    dictionary['__init__'] = _StructInit(fields)

    # Add serialization method
    serialization_object = serialization.Serialization(groups)
    def Serialize(self, handle_offset=0):
      return serialization_object.Serialize(self, handle_offset)
    dictionary['Serialize'] = Serialize

    # pylint: disable=W0212
    def AsDict(self):
      return self._fields
    dictionary['AsDict'] = AsDict

    def Deserialize(cls, context):
      result = cls.__new__(cls)
      fields = {}
      serialization_object.Deserialize(fields, context)
      result._fields = fields
      return result
    dictionary['Deserialize'] = classmethod(Deserialize)

    dictionary['__eq__'] = _StructEq(fields)
    dictionary['__ne__'] = _StructNe

    return type.__new__(mcs, name, bases, dictionary)

  # Prevent adding new attributes, or mutating constants.
  def __setattr__(cls, key, value):
    raise AttributeError('can\'t set attribute')

  # Prevent deleting constants.
  def __delattr__(cls, key):
    raise AttributeError('can\'t delete attribute')


class MojoInterfaceType(type):
  """Meta class for interfaces.

  Usage:
    class MyInterface(object):
      __metaclass__ = MojoInterfaceType
      DESCRIPTOR = {
        'methods': [
          {
            'name': 'FireAndForget',
            'ordinal': 0,
            'parameters': [
              SingleFieldGroup('x', _descriptor.TYPE_INT32, 0, 0),
            ]
          },
          {
            'name': 'Ping',
            'ordinal': 1,
            'parameters': [
              SingleFieldGroup('x', _descriptor.TYPE_INT32, 0, 0),
            ],
            'responses': [
              SingleFieldGroup('x', _descriptor.TYPE_INT32, 0, 0),
            ],
          },
        ],
      }
  """

  def __new__(mcs, name, bases, dictionary):
    # If one of the base class is already an interface type, do not edit the
    # class.
    for base in bases:
      if isinstance(base, mcs):
        return type.__new__(mcs, name, bases, dictionary)

    descriptor = dictionary.pop('DESCRIPTOR', {})

    methods = [_MethodDescriptor(x) for x in descriptor.get('methods', [])]
    for method in methods:
      dictionary[method.name] = _NotImplemented
    fully_qualified_name = descriptor['fully_qualified_name']

    interface_manager = InterfaceManager(fully_qualified_name, methods)
    dictionary.update({
        'manager': None,
        '_interface_manager': interface_manager,
    })

    interface_class = type.__new__(mcs, name, bases, dictionary)
    interface_manager.interface_class = interface_class
    return interface_class

  @property
  def manager(cls):
    return cls._interface_manager

  # Prevent adding new attributes, or mutating constants.
  def __setattr__(cls, key, value):
    raise AttributeError('can\'t set attribute')

  # Prevent deleting constants.
  def __delattr__(cls, key):
    raise AttributeError('can\'t delete attribute')


class InterfaceProxy(object):
  """
  A proxy allows to access a remote interface through a message pipe.
  """
  pass


class InterfaceRequest(object):
  """
  An interface request allows to send a request for an interface to a remote
  object and start using it immediately.
  """

  def __init__(self, handle):
    self._handle = handle

  def IsPending(self):
    return self._handle.IsValid()

  def PassMessagePipe(self):
    result = self._handle
    self._handle = None
    return result

  def Bind(self, impl):
    type(impl).manager.Bind(impl, self.PassMessagePipe())


class InterfaceManager(object):
  """
  Manager for an interface class. The manager contains the operation that allows
  to bind an implementation to a pipe, or to generate a proxy for an interface
  over a pipe.
  """

  def __init__(self, name, methods):
    self.name = name
    self.methods = methods
    self.interface_class = None
    self._proxy_class = None
    self._stub_class = None

  def Proxy(self, handle):
    router = messaging.Router(handle)
    error_handler = _ProxyErrorHandler()
    router.SetErrorHandler(error_handler)
    router.Start()
    return self._InternalProxy(router, error_handler)

  # pylint: disable=W0212
  def Bind(self, impl, handle):
    router = messaging.Router(handle)
    router.SetIncomingMessageReceiver(self._Stub(impl))
    error_handler = _ProxyErrorHandler()
    router.SetErrorHandler(error_handler)

    # Retain the router, until an error happen.
    retainer = _Retainer(router)
    def Cleanup(_):
      retainer.release()
    error_handler.AddCallback(Cleanup)

    # Give an instance manager to the implementation to allow it to close
    # the connection.
    impl.manager = InstanceManager(router, error_handler)

    router.Start()

  def _InternalProxy(self, router, error_handler):
    if error_handler is None:
      error_handler = _ProxyErrorHandler()

    if not self._proxy_class:
      dictionary = {
        '__module__': __name__,
        '__init__': _ProxyInit,
      }
      for method in self.methods:
        dictionary[method.name] = _ProxyMethodCall(method)
      self._proxy_class = type('%sProxy' % self.name,
                               (self.interface_class, InterfaceProxy),
                               dictionary)

    proxy = self._proxy_class(router, error_handler)
    # Give an instance manager to the proxy to allow to close the connection.
    proxy.manager = InstanceManager(router, error_handler)
    return proxy

  def _Stub(self, impl):
    if not self._stub_class:
      accept_method = _StubAccept(self.methods)
      dictionary = {
        '__module__': __name__,
        '__init__': _StubInit,
        'Accept': accept_method,
        'AcceptWithResponder': accept_method,
      }
      self._stub_class = type('%sStub' % self.name,
                              (messaging.MessageReceiverWithResponder,),
                              dictionary)
    return self._stub_class(impl)


class InstanceManager(object):
  """
  Manager for the implementation of an interface or a proxy. The manager allows
  to control the connection over the pipe.
  """
  def __init__(self, router, error_handler):
    self._router = router
    self._error_handler = error_handler
    assert self._error_handler is not None

  def Close(self):
    self._error_handler.OnClose()
    self._router.Close()

  def PassMessagePipe(self):
    self._error_handler.OnClose()
    return self._router.PassMessagePipe()

  def AddOnErrorCallback(self, callback):
    self._error_handler.AddCallback(lambda _: callback(), False)


class _MethodDescriptor(object):
  def __init__(self, descriptor):
    self.name = descriptor['name']
    self.ordinal = descriptor['ordinal']
    self.parameters_struct = _ConstructParameterStruct(
        descriptor['parameters'], self.name, "Parameters")
    self.response_struct = _ConstructParameterStruct(
        descriptor.get('responses'), self.name, "Responses")


def _ConstructParameterStruct(descriptor, name, suffix):
  if descriptor is None:
    return None
  parameter_dictionary = {
    '__metaclass__': MojoStructType,
    '__module__': __name__,
    'DESCRIPTOR': descriptor,
  }
  return MojoStructType(
      '%s%s' % (name, suffix),
      (object,),
      parameter_dictionary)


class _ProxyErrorHandler(messaging.ConnectionErrorHandler):
  def __init__(self):
    messaging.ConnectionErrorHandler.__init__(self)
    self._callbacks = dict()

  def OnError(self, result):
    if self._callbacks is None:
      return
    exception = messaging.MessagingException('Mojo error: %d' % result)
    for (callback, _) in self._callbacks.iteritems():
      callback(exception)
    self._callbacks = None

  def OnClose(self):
    if self._callbacks is None:
      return
    exception = messaging.MessagingException('Router has been closed.')
    for (callback, call_on_close) in self._callbacks.iteritems():
      if call_on_close:
        callback(exception)
    self._callbacks = None

  def AddCallback(self, callback, call_on_close=True):
    if self._callbacks is not None:
      self._callbacks[callback] = call_on_close

  def RemoveCallback(self, callback):
    if self._callbacks:
      del self._callbacks[callback]


class _Retainer(object):

  # Set to force instances to be retained.
  _RETAINED = set()

  def __init__(self, retained):
    self._retained = retained
    _Retainer._RETAINED.add(self)

  def release(self):
    self._retained = None
    _Retainer._RETAINED.remove(self)


def _StructInit(fields):
  def _Init(self, *args, **kwargs):
    if len(args) + len(kwargs) > len(fields):
      raise TypeError('__init__() takes %d argument (%d given)' %
                      (len(fields), len(args) + len(kwargs)))
    self._fields = {}
    for f, a in zip(fields, args):
      self.__setattr__(f.name, a)
    remaining_fields = set(x.name for x in fields[len(args):])
    for name in kwargs:
      if not name in remaining_fields:
        if name in (x.name for x in fields[:len(args)]):
          raise TypeError(
              '__init__() got multiple values for keyword argument %r' % name)
        raise TypeError('__init__() got an unexpected keyword argument %r' %
                        name)
      self.__setattr__(name, kwargs[name])
  return _Init


def _BuildProperty(field):
  """Build the property for the given field."""

  # pylint: disable=W0212
  def Get(self):
    if field.name not in self._fields:
      self._fields[field.name] = field.GetDefaultValue()
    return self._fields[field.name]

  # pylint: disable=W0212
  def Set(self, value):
    self._fields[field.name] = field.field_type.Convert(value)

  return property(Get, Set)


def _StructEq(fields):
  def _Eq(self, other):
    if type(self) is not type(other):
      return False
    for field in fields:
      if getattr(self, field.name) != getattr(other, field.name):
        return False
    return True
  return _Eq

def _StructNe(self, other):
  return not self.__eq__(other)


def _ProxyInit(self, router, error_handler):
  self._router = router
  self._error_handler = error_handler


# pylint: disable=W0212
def _ProxyMethodCall(method):
  flags = messaging.NO_FLAG
  if method.response_struct:
    flags = messaging.MESSAGE_EXPECTS_RESPONSE_FLAG
  def _Call(self, *args, **kwargs):
    def GenerationMethod(resolve, reject):
      message = _GetMessage(method, flags, *args, **kwargs)
      if method.response_struct:
        def Accept(message):
          try:
            assert message.header.message_type == method.ordinal
            payload = message.payload
            response = method.response_struct.Deserialize(
                serialization.RootDeserializationContext(payload.data,
                                                         payload.handles))
            as_dict = response.AsDict()
            if len(as_dict) == 1:
              value = as_dict.values()[0]
              if not isinstance(value, dict):
                response = value
            resolve(response)
            return True
          except Exception as e:
            # Adding traceback similarly to python 3.0 (pep-3134)
            e.__traceback__ = sys.exc_info()[2]
            reject(e)
            return False
          finally:
            self._error_handler.RemoveCallback(reject)

        self._error_handler.AddCallback(reject)
        if not self._router.AcceptWithResponder(
            message, messaging.ForwardingMessageReceiver(Accept)):
          self._error_handler.RemoveCallback(reject)
          reject(messaging.MessagingException("Unable to send message."))
      else:
        if (self._router.Accept(message)):
          resolve(None)
        else:
          reject(messaging.MessagingException("Unable to send message."))
    return promise.Promise(GenerationMethod)
  return _Call


def _GetMessage(method, flags, *args, **kwargs):
  if flags == messaging.MESSAGE_IS_RESPONSE_FLAG:
    struct = method.response_struct(*args, **kwargs)
  else:
    struct = method.parameters_struct(*args, **kwargs)
  header = messaging.MessageHeader(method.ordinal, flags)
  data = header.Serialize()
  (payload, handles) = struct.Serialize()
  data.extend(payload)
  return messaging.Message(data, handles, header)


def _StubInit(self, impl):
  self.impl = impl


def _StubAccept(methods):
  methods_by_ordinal = dict((m.ordinal, m) for m in methods)
  def Accept(self, message, responder=None):
    try:
      header = message.header
      assert header.expects_response == bool(responder)
      assert header.message_type in methods_by_ordinal
      method = methods_by_ordinal[header.message_type]
      payload = message.payload
      parameters = method.parameters_struct.Deserialize(
          serialization.RootDeserializationContext(
              payload.data, payload.handles)).AsDict()
      response = getattr(self.impl, method.name)(**parameters)
      if header.expects_response:
        def SendResponse(response):
          if isinstance(response, dict):
            response_message = _GetMessage(method,
                                           messaging.MESSAGE_IS_RESPONSE_FLAG,
                                           **response)
          else:
            response_message = _GetMessage(method,
                                           messaging.MESSAGE_IS_RESPONSE_FLAG,
                                           response)
          response_message.header.request_id = header.request_id
          responder.Accept(response_message)
        p = promise.Promise.Resolve(response).Then(SendResponse)
        if self.impl.manager:
          # Close the connection in case of error.
          p.Catch(lambda _: self.impl.manager.Close())
      return True
    # pylint: disable=W0702
    except:
      # Close the connection in case of error.
      logging.warning(
          'Error occured in accept method. Connection will be closed.')
      if self.impl.manager:
        self.impl.manager.Close()
      return False
  return Accept


def _NotImplemented(*_1, **_2):
  raise NotImplementedError()
