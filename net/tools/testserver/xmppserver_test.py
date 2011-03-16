#!/usr/bin/python2.4
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests exercising the various classes in xmppserver.py."""

import unittest

import base64
import xmppserver

class XmlUtilsTest(unittest.TestCase):

  def testParseXml(self):
    xml_text = """<foo xmlns=""><bar xmlns=""><baz/></bar></foo>"""
    xml = xmppserver.ParseXml(xml_text)
    self.assertEqual(xml.toxml(), xml_text)

  def testCloneXml(self):
    xml = xmppserver.ParseXml('<foo/>')
    xml_clone = xmppserver.CloneXml(xml)
    xml_clone.setAttribute('bar', 'baz')
    self.assertEqual(xml, xml)
    self.assertEqual(xml_clone, xml_clone)
    self.assertNotEqual(xml, xml_clone)

  def testCloneXmlUnlink(self):
    xml_text = '<foo/>'
    xml = xmppserver.ParseXml(xml_text)
    xml_clone = xmppserver.CloneXml(xml)
    xml.unlink()
    self.assertEqual(xml.parentNode, None)
    self.assertNotEqual(xml_clone.parentNode, None)
    self.assertEqual(xml_clone.toxml(), xml_text)

class StanzaParserTest(unittest.TestCase):

  def setUp(self):
    self.stanzas = []

  def FeedStanza(self, stanza):
    # We can't append stanza directly because it is unlinked after
    # this callback.
    self.stanzas.append(stanza.toxml())

  def testBasic(self):
    parser = xmppserver.StanzaParser(self)
    parser.FeedString('<foo')
    self.assertEqual(len(self.stanzas), 0)
    parser.FeedString('/><bar></bar>')
    self.assertEqual(self.stanzas[0], '<foo/>')
    self.assertEqual(self.stanzas[1], '<bar/>')

  def testStream(self):
    parser = xmppserver.StanzaParser(self)
    parser.FeedString('<stream')
    self.assertEqual(len(self.stanzas), 0)
    parser.FeedString(':stream foo="bar" xmlns:stream="baz">')
    self.assertEqual(self.stanzas[0],
                     '<stream:stream foo="bar" xmlns:stream="baz"/>')

  def testNested(self):
    parser = xmppserver.StanzaParser(self)
    parser.FeedString('<foo')
    self.assertEqual(len(self.stanzas), 0)
    parser.FeedString(' bar="baz"')
    parser.FeedString('><baz/><blah>meh</blah></foo>')
    self.assertEqual(self.stanzas[0],
                     '<foo bar="baz"><baz/><blah>meh</blah></foo>')


class JidTest(unittest.TestCase):

  def testBasic(self):
    jid = xmppserver.Jid('foo', 'bar.com')
    self.assertEqual(str(jid), 'foo@bar.com')

  def testResource(self):
    jid = xmppserver.Jid('foo', 'bar.com', 'resource')
    self.assertEqual(str(jid), 'foo@bar.com/resource')

  def testGetBareJid(self):
    jid = xmppserver.Jid('foo', 'bar.com', 'resource')
    self.assertEqual(str(jid.GetBareJid()), 'foo@bar.com')


class IdGeneratorTest(unittest.TestCase):

  def testBasic(self):
    id_generator = xmppserver.IdGenerator('foo')
    for i in xrange(0, 100):
      self.assertEqual('foo.%d' % i, id_generator.GetNextId())


class HandshakeTaskTest(unittest.TestCase):

  def setUp(self):
    self.data_received = 0

  def SendData(self, _):
    self.data_received += 1

  def SendStanza(self, _, unused=True):
    self.data_received += 1

  def HandshakeDone(self, jid):
    self.jid = jid

  def DoHandshake(self, resource_prefix, resource, username,
                  initial_stream_domain, auth_domain, auth_stream_domain):
    self.data_received = 0
    handshake_task = (
      xmppserver.HandshakeTask(self, resource_prefix))
    stream_xml = xmppserver.ParseXml('<stream:stream xmlns:stream="foo"/>')
    stream_xml.setAttribute('to', initial_stream_domain)
    self.assertEqual(self.data_received, 0)
    handshake_task.FeedStanza(stream_xml)
    self.assertEqual(self.data_received, 2)

    if auth_domain:
      username_domain = '%s@%s' % (username, auth_domain)
    else:
      username_domain = username
    auth_string = base64.b64encode('\0%s\0bar' % username_domain)
    auth_xml = xmppserver.ParseXml('<auth>%s</auth>'% auth_string)
    handshake_task.FeedStanza(auth_xml)
    self.assertEqual(self.data_received, 3)

    stream_xml = xmppserver.ParseXml('<stream:stream xmlns:stream="foo"/>')
    stream_xml.setAttribute('to', auth_stream_domain)
    handshake_task.FeedStanza(stream_xml)
    self.assertEqual(self.data_received, 5)

    bind_xml = xmppserver.ParseXml(
      '<iq type="set"><bind><resource>%s</resource></bind></iq>' % resource)
    handshake_task.FeedStanza(bind_xml)
    self.assertEqual(self.data_received, 6)

    session_xml = xmppserver.ParseXml(
      '<iq type="set"><session></session></iq>')
    handshake_task.FeedStanza(session_xml)
    self.assertEqual(self.data_received, 7)

    self.assertEqual(self.jid.username, username)
    self.assertEqual(self.jid.domain,
                     auth_stream_domain or auth_domain or
                     initial_stream_domain)
    self.assertEqual(self.jid.resource,
                     '%s.%s' % (resource_prefix, resource))

  def testBasic(self):
    self.DoHandshake('resource_prefix', 'resource',
                     'foo', 'bar.com', 'baz.com', 'quux.com')

  def testDomainBehavior(self):
    self.DoHandshake('resource_prefix', 'resource',
                     'foo', 'bar.com', 'baz.com', 'quux.com')
    self.DoHandshake('resource_prefix', 'resource',
                     'foo', 'bar.com', 'baz.com', '')
    self.DoHandshake('resource_prefix', 'resource',
                     'foo', 'bar.com', '', '')
    self.DoHandshake('resource_prefix', 'resource',
                     'foo', '', '', '')


class XmppConnectionTest(unittest.TestCase):

  def setUp(self):
    self.connections = set()
    self.data = []

  # socket-like methods.
  def fileno(self):
    return 0

  def setblocking(self, int):
    pass

  def getpeername(self):
    return ('', 0)

  def send(self, data):
    self.data.append(data)
    pass

  def close(self):
    pass

  # XmppConnection delegate methods.
  def OnXmppHandshakeDone(self, xmpp_connection):
    self.connections.add(xmpp_connection)

  def OnXmppConnectionClosed(self, xmpp_connection):
    self.connections.discard(xmpp_connection)

  def ForwardNotification(self, unused_xmpp_connection, notification_stanza):
    for connection in self.connections:
      connection.ForwardNotification(notification_stanza)

  def testBasic(self):
    socket_map = {}
    xmpp_connection = xmppserver.XmppConnection(
      self, socket_map, self, ('', 0))
    self.assertEqual(len(socket_map), 1)
    self.assertEqual(len(self.connections), 0)
    xmpp_connection.HandshakeDone(xmppserver.Jid('foo', 'bar'))
    self.assertEqual(len(socket_map), 1)
    self.assertEqual(len(self.connections), 1)

    # Test subscription request.
    self.assertEqual(len(self.data), 0)
    xmpp_connection.collect_incoming_data(
      '<iq><subscribe xmlns="google:push"></subscribe></iq>')
    self.assertEqual(len(self.data), 1)

    # Test acks.
    xmpp_connection.collect_incoming_data('<iq type="result"/>')
    self.assertEqual(len(self.data), 1)

    # Test notification.
    xmpp_connection.collect_incoming_data(
      '<message><push xmlns="google:push"/></message>')
    self.assertEqual(len(self.data), 2)

    # Test unexpected stanza.
    def SendUnexpectedStanza():
      xmpp_connection.collect_incoming_data('<foo/>')
    self.assertRaises(xmppserver.UnexpectedXml, SendUnexpectedStanza)

    # Test unexpected notifier command.
    def SendUnexpectedNotifierCommand():
      xmpp_connection.collect_incoming_data(
        '<iq><foo xmlns="google:notifier"/></iq>')
    self.assertRaises(xmppserver.UnexpectedXml,
                      SendUnexpectedNotifierCommand)

    # Test close
    xmpp_connection.close()
    self.assertEqual(len(socket_map), 0)
    self.assertEqual(len(self.connections), 0)

class XmppServerTest(unittest.TestCase):

  # socket-like methods.
  def fileno(self):
    return 0

  def setblocking(self, int):
    pass

  def getpeername(self):
    return ('', 0)

  def close(self):
    pass

  def testBasic(self):
    class FakeXmppServer(xmppserver.XmppServer):
      def accept(self2):
        return (self, ('', 0))

    socket_map = {}
    self.assertEqual(len(socket_map), 0)
    xmpp_server = FakeXmppServer(socket_map, ('', 0))
    self.assertEqual(len(socket_map), 1)
    xmpp_server.handle_accept()
    self.assertEqual(len(socket_map), 2)
    xmpp_server.close()
    self.assertEqual(len(socket_map), 0)


if __name__ == '__main__':
  unittest.main()
