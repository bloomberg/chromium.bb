// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included

// This file defines the internal messages which can be sent on a NodeLink
// between two ipcz nodes.

IPCZ_MSG_BEGIN_INTERFACE(Node)

// Initial greeting sent by a broker node when a ConnectNode() is issued without
// the IPCZ_CONNECT_NODE_TO_BROKER flag, implying that the receiving node is a
// non-broker.
IPCZ_MSG_BEGIN(ConnectFromBrokerToNonBroker,
               IPCZ_MSG_ID(0),
               IPCZ_MSG_VERSION(0))
  // The name of the broker node.
  IPCZ_MSG_PARAM(NodeName, broker_name)

  // The name of the receiving non-broker node, assigned randomly by the broker.
  IPCZ_MSG_PARAM(NodeName, receiver_name)

  // The highest protocol version known and desired by the broker.
  IPCZ_MSG_PARAM(uint32_t, protocol_version)

  // The number of initial portals assumed on the broker's end of the
  // connection. If there is a mismatch between the number sent by each node on
  // an initial connection, the node which sent the larger number should behave
  // as if its excess portals have observed peer closure. This may occur for
  // example as a result of version skew between one application node and
  // another, where one end tries to establish more initial portals than the
  // other supports.
  IPCZ_MSG_PARAM(uint32_t, num_initial_portals)

  // A driver memory object to serve as the new NodeLink's primary shared
  // buffer. That is, BufferId 0 within its NodeLinkMemory's BufferPool.
  IPCZ_MSG_PARAM_DRIVER_OBJECT(buffer)
IPCZ_MSG_END()

// Initial greeting sent by a non-broker node when ConnectNode() is invoked with
// IPCZ_CONNECT_NODE_TO_BROKER. The sending non-broker node expects to receive a
// corresponding ConnectFromBrokerToNonBroker
IPCZ_MSG_BEGIN(ConnectFromNonBrokerToBroker,
               IPCZ_MSG_ID(1),
               IPCZ_MSG_VERSION(0))
  // The highest protocol version known and desired by the sender.
  IPCZ_MSG_PARAM(uint32_t, protocol_version)

  // The number of initial portals assumed on the sender's end of the
  // connection. If there is a mismatch between the number sent by each node on
  // an initial connection, the node which sent the larger number should behave
  // as if its excess portals have observed peer closure.
  IPCZ_MSG_PARAM(uint32_t, num_initial_portals)
IPCZ_MSG_END()

// Conveys the contents of a parcel.
IPCZ_MSG_BEGIN(AcceptParcel, IPCZ_MSG_ID(20), IPCZ_MSG_VERSION(0))
  // The SublinkId linking the source and destination Routers along the
  // transmitting NodeLink.
  IPCZ_MSG_PARAM(SublinkId, sublink)

  // The SequenceNumber of this parcel within the transmitting portal's outbound
  // parcel sequence (and the receiving portal's inbound parcel sequence.)
  IPCZ_MSG_PARAM(SequenceNumber, sequence_number)

  // Free-form array of application-provided data bytes for this parcel.
  IPCZ_MSG_PARAM_ARRAY(uint8_t, parcel_data)

  // Array of handle types, with each corresponding to a single IpczHandle
  // attached to the parcel.
  IPCZ_MSG_PARAM_ARRAY(HandleType, handle_types)

  // For every portal handle attached, there is also a RouterDescriptor encoding
  // the details needed to construct a new Router at the parcel's destination
  // to extend the transmitted portal's route there.
  IPCZ_MSG_PARAM_ARRAY(RouterDescriptor, new_routers)

  // Every DriverObject boxed and attached to this parcel has an entry in this
  // array.
  IPCZ_MSG_PARAM_DRIVER_OBJECT_ARRAY(driver_objects)
IPCZ_MSG_END()

// Notifies a node that the route has been closed on one side. This message
// always pertains to the side of the route opposite of the router receiving it,
// guaranteed by the fact that the closed side of the route only transmits this
// message outward once its terminal router is adjacent to the central link.
IPCZ_MSG_BEGIN(RouteClosed, IPCZ_MSG_ID(22), IPCZ_MSG_VERSION(0))
  // In the context of the receiving NodeLink, this identifies the specific
  // Router to receive this message.
  IPCZ_MSG_PARAM(SublinkId, sublink)

  // The total number of parcels sent from the side of the route which closed,
  // before closing. Because parcels may arrive out-of-order from each other
  // and from messages like this one under various conditions (broker relays,
  // different transport mechanisms, etc.), parcels are tagged with strictly
  // increasing SequenceNumbers by the sender. This field informs the recipient
  // that the closed endpoint has transmitted exactly `sequence_length` parcels,
  // from SequenceNumber 0 to `sequence_length-1`. The recipient can use this
  // to know, for example, that it must still expect some additional parcels to
  // arrive before completely forgetting about the route's link(s).
  IPCZ_MSG_PARAM(SequenceNumber, sequence_length)
IPCZ_MSG_END()

IPCZ_MSG_END_INTERFACE()
