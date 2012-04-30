// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PEPPER_TRANSPORT_FACTORY_H_
#define REMOTING_PROTOCOL_PEPPER_TRANSPORT_FACTORY_H_

#include "remoting/protocol/transport.h"
#include "remoting/protocol/transport_config.h"

namespace pp {
class Instance;
}  // namespace pp

namespace remoting {
namespace protocol {

class PepperTransportFactory : public TransportFactory {
 public:
  PepperTransportFactory(pp::Instance* pp_instance);
  virtual ~PepperTransportFactory();

  // TransportFactory interface.
  virtual void SetTransportConfig(const TransportConfig& config) OVERRIDE;
  virtual scoped_ptr<StreamTransport> CreateStreamTransport() OVERRIDE;
  virtual scoped_ptr<DatagramTransport> CreateDatagramTransport() OVERRIDE;

 private:
  pp::Instance* pp_instance_;
  TransportConfig config_;

  DISALLOW_COPY_AND_ASSIGN(PepperTransportFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PEPPER_TRANSPORT_FACTORY_H_
