/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>
#include "talk/base/basicdefs.h"
#include "talk/xmllite/xmlconstants.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmllite/qname.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppconstants.h"
namespace buzz {

const Jid JID_EMPTY(STR_EMPTY);

const std::string & Constants::ns_client() {
  static const std::string ns_client_("jabber:client");
  return ns_client_;
}

const std::string & Constants::ns_server() {
  static const std::string ns_server_("jabber:server");
  return ns_server_;
}

const std::string & Constants::ns_stream() {
  static const std::string ns_stream_("http://etherx.jabber.org/streams");
  return ns_stream_;
}

const std::string & Constants::ns_xstream() {
  static const std::string ns_xstream_("urn:ietf:params:xml:ns:xmpp-streams");
  return ns_xstream_;
}

const std::string & Constants::ns_tls() {
  static const std::string ns_tls_("urn:ietf:params:xml:ns:xmpp-tls");
  return ns_tls_;
}

const std::string & Constants::ns_sasl() {
  static const std::string ns_sasl_("urn:ietf:params:xml:ns:xmpp-sasl");
  return ns_sasl_;
}

const std::string & Constants::ns_bind() {
  static const std::string ns_bind_("urn:ietf:params:xml:ns:xmpp-bind");
  return ns_bind_;
}

const std::string & Constants::ns_dialback() {
  static const std::string ns_dialback_("jabber:server:dialback");
  return ns_dialback_;
}

const std::string & Constants::ns_session() {
  static const std::string ns_session_("urn:ietf:params:xml:ns:xmpp-session");
  return ns_session_;
}

const std::string & Constants::ns_stanza() {
  static const std::string ns_stanza_("urn:ietf:params:xml:ns:xmpp-stanzas");
  return ns_stanza_;
}

const std::string & Constants::ns_privacy() {
  static const std::string ns_privacy_("jabber:iq:privacy");
  return ns_privacy_;
}

const std::string & Constants::ns_roster() {
  static const std::string ns_roster_("jabber:iq:roster");
  return ns_roster_;
}

const std::string & Constants::ns_vcard() {
  static const std::string ns_vcard_("vcard-temp");
  return ns_vcard_;
}

const std::string & Constants::ns_avatar_hash() {
  static const std::string ns_avatar_hash_("google:avatar");
  return ns_avatar_hash_;
}

const std::string & Constants::ns_vcard_update() {
  static const std::string ns_vcard_update_("vcard-temp:x:update");
  return ns_vcard_update_;
}

const std::string & Constants::str_client() {
  static const std::string str_client_("client");
  return str_client_;
}

const std::string & Constants::str_server() {
  static const std::string str_server_("server");
  return str_server_;
}

const std::string & Constants::str_stream() {
  static const std::string str_stream_("stream");
  return str_stream_;
}

const std::string STR_GET("get");
const std::string STR_SET("set");
const std::string STR_RESULT("result");
const std::string STR_ERROR("error");


const std::string STR_FROM("from");
const std::string STR_TO("to");
const std::string STR_BOTH("both");
const std::string STR_REMOVE("remove");

const std::string STR_UNAVAILABLE("unavailable");

const std::string STR_GOOGLE_COM("google.com");
const std::string STR_GMAIL_COM("gmail.com");
const std::string STR_GOOGLEMAIL_COM("googlemail.com");
const std::string STR_DEFAULT_DOMAIN("default.talk.google.com");
const std::string STR_TALK_GOOGLE_COM("talk.google.com");
const std::string STR_TALKX_L_GOOGLE_COM("talkx.l.google.com");

const std::string STR_X("x");

#ifdef FEATURE_ENABLE_VOICEMAIL
const std::string STR_VOICEMAIL("voicemail");
const std::string STR_OUTGOINGVOICEMAIL("outgoingvoicemail");
#endif

const QName QN_STREAM_STREAM(NS_STREAM, STR_STREAM);
const QName QN_STREAM_FEATURES(NS_STREAM, "features");
const QName QN_STREAM_ERROR(NS_STREAM, "error");

const QName QN_XSTREAM_BAD_FORMAT(NS_XSTREAM, "bad-format");
const QName QN_XSTREAM_BAD_NAMESPACE_PREFIX(NS_XSTREAM, "bad-namespace-prefix");
const QName QN_XSTREAM_CONFLICT(NS_XSTREAM, "conflict");
const QName QN_XSTREAM_CONNECTION_TIMEOUT(NS_XSTREAM, "connection-timeout");
const QName QN_XSTREAM_HOST_GONE(NS_XSTREAM, "host-gone");
const QName QN_XSTREAM_HOST_UNKNOWN(NS_XSTREAM, "host-unknown");
const QName QN_XSTREAM_IMPROPER_ADDRESSIING(NS_XSTREAM, "improper-addressing");
const QName QN_XSTREAM_INTERNAL_SERVER_ERROR(NS_XSTREAM, "internal-server-error");
const QName QN_XSTREAM_INVALID_FROM(NS_XSTREAM, "invalid-from");
const QName QN_XSTREAM_INVALID_ID(NS_XSTREAM, "invalid-id");
const QName QN_XSTREAM_INVALID_NAMESPACE(NS_XSTREAM, "invalid-namespace");
const QName QN_XSTREAM_INVALID_XML(NS_XSTREAM, "invalid-xml");
const QName QN_XSTREAM_NOT_AUTHORIZED(NS_XSTREAM, "not-authorized");
const QName QN_XSTREAM_POLICY_VIOLATION(NS_XSTREAM, "policy-violation");
const QName QN_XSTREAM_REMOTE_CONNECTION_FAILED(NS_XSTREAM, "remote-connection-failed");
const QName QN_XSTREAM_RESOURCE_CONSTRAINT(NS_XSTREAM, "resource-constraint");
const QName QN_XSTREAM_RESTRICTED_XML(NS_XSTREAM, "restricted-xml");
const QName QN_XSTREAM_SEE_OTHER_HOST(NS_XSTREAM, "see-other-host");
const QName QN_XSTREAM_SYSTEM_SHUTDOWN(NS_XSTREAM, "system-shutdown");
const QName QN_XSTREAM_UNDEFINED_CONDITION(NS_XSTREAM, "undefined-condition");
const QName QN_XSTREAM_UNSUPPORTED_ENCODING(NS_XSTREAM, "unsupported-encoding");
const QName QN_XSTREAM_UNSUPPORTED_STANZA_TYPE(NS_XSTREAM, "unsupported-stanza-type");
const QName QN_XSTREAM_UNSUPPORTED_VERSION(NS_XSTREAM, "unsupported-version");
const QName QN_XSTREAM_XML_NOT_WELL_FORMED(NS_XSTREAM, "xml-not-well-formed");
const QName QN_XSTREAM_TEXT(NS_XSTREAM, "text");

const QName QN_TLS_STARTTLS(NS_TLS, "starttls");
const QName QN_TLS_REQUIRED(NS_TLS, "required");
const QName QN_TLS_PROCEED(NS_TLS, "proceed");
const QName QN_TLS_FAILURE(NS_TLS, "failure");

const QName QN_SASL_MECHANISMS(NS_SASL, "mechanisms");
const QName QN_SASL_MECHANISM(NS_SASL, "mechanism");
const QName QN_SASL_AUTH(NS_SASL, "auth");
const QName QN_SASL_CHALLENGE(NS_SASL, "challenge");
const QName QN_SASL_RESPONSE(NS_SASL, "response");
const QName QN_SASL_ABORT(NS_SASL, "abort");
const QName QN_SASL_SUCCESS(NS_SASL, "success");
const QName QN_SASL_FAILURE(NS_SASL, "failure");
const QName QN_SASL_ABORTED(NS_SASL, "aborted");
const QName QN_SASL_INCORRECT_ENCODING(NS_SASL, "incorrect-encoding");
const QName QN_SASL_INVALID_AUTHZID(NS_SASL, "invalid-authzid");
const QName QN_SASL_INVALID_MECHANISM(NS_SASL, "invalid-mechanism");
const QName QN_SASL_MECHANISM_TOO_WEAK(NS_SASL, "mechanism-too-weak");
const QName QN_SASL_NOT_AUTHORIZED(NS_SASL, "not-authorized");
const QName QN_SASL_TEMPORARY_AUTH_FAILURE(NS_SASL, "temporary-auth-failure");

const std::string NS_GOOGLE_AUTH_PROTOCOL("http://www.google.com/talk/protocol/auth");
const QName QN_GOOGLE_AUTH_CLIENT_USES_FULL_BIND_RESULT(NS_GOOGLE_AUTH_PROTOCOL, "client-uses-full-bind-result");

const std::string NS_GOOGLE_AUTH("google:auth");
const QName QN_MISSING_USERNAME(NS_GOOGLE_AUTH, "missing-username");
const QName QN_GOOGLE_ALLOW_GENERATED_JID_XMPP_LOGIN(NS_GOOGLE_AUTH_PROTOCOL, "allow-generated-jid");

const QName QN_DIALBACK_RESULT(NS_DIALBACK, "result");
const QName QN_DIALBACK_VERIFY(NS_DIALBACK, "verify");

const QName QN_STANZA_BAD_REQUEST(NS_STANZA, "bad-request");
const QName QN_STANZA_CONFLICT(NS_STANZA, "conflict");
const QName QN_STANZA_FEATURE_NOT_IMPLEMENTED(NS_STANZA, "feature-not-implemented");
const QName QN_STANZA_FORBIDDEN(NS_STANZA, "forbidden");
const QName QN_STANZA_GONE(NS_STANZA, "gone");
const QName QN_STANZA_INTERNAL_SERVER_ERROR(NS_STANZA, "internal-server-error");
const QName QN_STANZA_ITEM_NOT_FOUND(NS_STANZA, "item-not-found");
const QName QN_STANZA_JID_MALFORMED(NS_STANZA, "jid-malformed");
const QName QN_STANZA_NOT_ACCEPTABLE(NS_STANZA, "not-acceptable");
const QName QN_STANZA_NOT_ALLOWED(NS_STANZA, "not-allowed");
const QName QN_STANZA_PAYMENT_REQUIRED(NS_STANZA, "payment-required");
const QName QN_STANZA_RECIPIENT_UNAVAILABLE(NS_STANZA, "recipient-unavailable");
const QName QN_STANZA_REDIRECT(NS_STANZA, "redirect");
const QName QN_STANZA_REGISTRATION_REQUIRED(NS_STANZA, "registration-required");
const QName QN_STANZA_REMOTE_SERVER_NOT_FOUND(NS_STANZA, "remote-server-not-found");
const QName QN_STANZA_REMOTE_SERVER_TIMEOUT(NS_STANZA, "remote-server-timeout");
const QName QN_STANZA_RESOURCE_CONSTRAINT(NS_STANZA, "resource-constraint");
const QName QN_STANZA_SERVICE_UNAVAILABLE(NS_STANZA, "service-unavailable");
const QName QN_STANZA_SUBSCRIPTION_REQUIRED(NS_STANZA, "subscription-required");
const QName QN_STANZA_UNDEFINED_CONDITION(NS_STANZA, "undefined-condition");
const QName QN_STANZA_UNEXPECTED_REQUEST(NS_STANZA, "unexpected-request");
const QName QN_STANZA_TEXT(NS_STANZA, "text");

const QName QN_BIND_BIND(NS_BIND, "bind");
const QName QN_BIND_RESOURCE(NS_BIND, "resource");
const QName QN_BIND_JID(NS_BIND, "jid");

const QName QN_MESSAGE(NS_CLIENT, "message");
const QName QN_BODY(NS_CLIENT, "body");
const QName QN_SUBJECT(NS_CLIENT, "subject");
const QName QN_THREAD(NS_CLIENT, "thread");
const QName QN_PRESENCE(NS_CLIENT, "presence");
const QName QN_SHOW(NS_CLIENT, "show");
const QName QN_STATUS(NS_CLIENT, "status");
const QName QN_LANG(NS_CLIENT, "lang");
const QName QN_PRIORITY(NS_CLIENT, "priority");
const QName QN_IQ(NS_CLIENT, "iq");
const QName QN_ERROR(NS_CLIENT, "error");

const QName QN_SERVER_MESSAGE(NS_SERVER, "message");
const QName QN_SERVER_BODY(NS_SERVER, "body");
const QName QN_SERVER_SUBJECT(NS_SERVER, "subject");
const QName QN_SERVER_THREAD(NS_SERVER, "thread");
const QName QN_SERVER_PRESENCE(NS_SERVER, "presence");
const QName QN_SERVER_SHOW(NS_SERVER, "show");
const QName QN_SERVER_STATUS(NS_SERVER, "status");
const QName QN_SERVER_LANG(NS_SERVER, "lang");
const QName QN_SERVER_PRIORITY(NS_SERVER, "priority");
const QName QN_SERVER_IQ(NS_SERVER, "iq");
const QName QN_SERVER_ERROR(NS_SERVER, "error");

const QName QN_SESSION_SESSION(NS_SESSION, "session");

const QName QN_PRIVACY_QUERY(NS_PRIVACY, "query");
const QName QN_PRIVACY_ACTIVE(NS_PRIVACY, "active");
const QName QN_PRIVACY_DEFAULT(NS_PRIVACY, "default");
const QName QN_PRIVACY_LIST(NS_PRIVACY, "list");
const QName QN_PRIVACY_ITEM(NS_PRIVACY, "item");
const QName QN_PRIVACY_IQ(NS_PRIVACY, "iq");
const QName QN_PRIVACY_MESSAGE(NS_PRIVACY, "message");
const QName QN_PRIVACY_PRESENCE_IN(NS_PRIVACY, "presence-in");
const QName QN_PRIVACY_PRESENCE_OUT(NS_PRIVACY, "presence-out");

const QName QN_ROSTER_QUERY(NS_ROSTER, "query");
const QName QN_ROSTER_ITEM(NS_ROSTER, "item");
const QName QN_ROSTER_GROUP(NS_ROSTER, "group");

const QName QN_VCARD(NS_VCARD, "vCard");
const QName QN_VCARD_FN(NS_VCARD, "FN");
const QName QN_VCARD_PHOTO(NS_VCARD, "PHOTO");
const QName QN_VCARD_PHOTO_BINVAL(NS_VCARD, "BINVAL");
const QName QN_VCARD_AVATAR_HASH(NS_AVATAR_HASH, "hash");
const QName QN_VCARD_AVATAR_HASH_MODIFIED(NS_AVATAR_HASH, "modified");

const buzz::QName QN_NAME(STR_EMPTY, "name");
const QName QN_XML_LANG(NS_XML, "lang");

const std::string STR_TYPE("type");
const std::string STR_ID("id");
const std::string STR_NAME("name");
const std::string STR_JID("jid");
const std::string STR_SUBSCRIPTION("subscription");
const std::string STR_ASK("ask");

const QName QN_ENCODING(STR_EMPTY, STR_ENCODING);
const QName QN_VERSION(STR_EMPTY, STR_VERSION);
const QName QN_TO(STR_EMPTY, "to");
const QName QN_FROM(STR_EMPTY, "from");
const QName QN_TYPE(STR_EMPTY, "type");
const QName QN_ID(STR_EMPTY, "id");
const QName QN_CODE(STR_EMPTY, "code");

const QName QN_VALUE(STR_EMPTY, "value");
const QName QN_ACTION(STR_EMPTY, "action");
const QName QN_ORDER(STR_EMPTY, "order");
const QName QN_MECHANISM(STR_EMPTY, "mechanism");
const QName QN_ASK(STR_EMPTY, "ask");
const QName QN_JID(STR_EMPTY, "jid");
const QName QN_SUBSCRIPTION(STR_EMPTY, "subscription");
const QName QN_TITLE1(STR_EMPTY, "title1");
const QName QN_TITLE2(STR_EMPTY, "title2");
const QName QN_SOURCE(STR_EMPTY, "source");

const QName QN_XMLNS_CLIENT(NS_XMLNS, STR_CLIENT);
const QName QN_XMLNS_SERVER(NS_XMLNS, STR_SERVER);
const QName QN_XMLNS_STREAM(NS_XMLNS, STR_STREAM);



// Presence
const std::string STR_SHOW_AWAY("away");
const std::string STR_SHOW_CHAT("chat");
const std::string STR_SHOW_DND("dnd");
const std::string STR_SHOW_XA("xa");
const std::string STR_SHOW_OFFLINE("offline");

// Subscription
const std::string STR_SUBSCRIBE("subscribe");
const std::string STR_SUBSCRIBED("subscribed");
const std::string STR_UNSUBSCRIBE("unsubscribe");
const std::string STR_UNSUBSCRIBED("unsubscribed");


// JEP 0030
const QName QN_NODE(STR_EMPTY, "node");
const QName QN_CATEGORY(STR_EMPTY, "category");
const QName QN_VAR(STR_EMPTY, "var");
const std::string NS_DISCO_INFO("http://jabber.org/protocol/disco#info");
const std::string NS_DISCO_ITEMS("http://jabber.org/protocol/disco#items");
const QName QN_DISCO_INFO_QUERY(NS_DISCO_INFO, "query");
const QName QN_DISCO_IDENTITY(NS_DISCO_INFO, "identity");
const QName QN_DISCO_FEATURE(NS_DISCO_INFO, "feature");

const QName QN_DISCO_ITEMS_QUERY(NS_DISCO_ITEMS, "query");
const QName QN_DISCO_ITEM(NS_DISCO_ITEMS, "item");


// JEP 0115
const std::string NS_CAPS("http://jabber.org/protocol/caps");
const QName QN_CAPS_C(NS_CAPS, "c");
const QName QN_VER(STR_EMPTY, "ver");
const QName QN_EXT(STR_EMPTY, "ext");

// JEP 0153
const std::string kNSVCard("vcard-temp:x:update");
const QName kQnVCardX(kNSVCard, "x");
const QName kQnVCardPhoto(kNSVCard, "photo");

// JEP 0172 User Nickname
const std::string kNSNickname("http://jabber.org/protocol/nick");
const QName kQnNickname(kNSNickname, "nick");


// JEP 0085 chat state
const std::string NS_CHATSTATE("http://jabber.org/protocol/chatstates");
const QName QN_CS_ACTIVE(NS_CHATSTATE, "active");
const QName QN_CS_COMPOSING(NS_CHATSTATE, "composing");
const QName QN_CS_PAUSED(NS_CHATSTATE, "paused");
const QName QN_CS_INACTIVE(NS_CHATSTATE, "inactive");
const QName QN_CS_GONE(NS_CHATSTATE, "gone");

// JEP 0091 Delayed Delivery
const std::string kNSDelay("jabber:x:delay");
const QName kQnDelayX(kNSDelay, "x");
const QName kQnStamp(STR_EMPTY, "stamp");

// Google time stamping (higher resolution)
const std::string kNSTimestamp("google:timestamp");
const QName kQnTime(kNSTimestamp, "time");
const QName kQnMilliseconds(STR_EMPTY, "ms");



// Jingle Info
const std::string NS_JINGLE_INFO("google:jingleinfo");
const QName QN_JINGLE_INFO_QUERY(NS_JINGLE_INFO, "query");
const QName QN_JINGLE_INFO_STUN(NS_JINGLE_INFO, "stun");
const QName QN_JINGLE_INFO_RELAY(NS_JINGLE_INFO, "relay");
const QName QN_JINGLE_INFO_SERVER(NS_JINGLE_INFO, "server");
const QName QN_JINGLE_INFO_TOKEN(NS_JINGLE_INFO, "token");
const QName QN_JINGLE_INFO_HOST(STR_EMPTY, "host");
const QName QN_JINGLE_INFO_TCP(STR_EMPTY, "tcp");
const QName QN_JINGLE_INFO_UDP(STR_EMPTY, "udp");
const QName QN_JINGLE_INFO_TCPSSL(STR_EMPTY, "tcpssl");

}
