// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_TRANSACTION_H_
#define NET_DNS_DNS_TRANSACTION_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "net/base/request_priority.h"
#include "net/dns/dns_util.h"
#include "net/dns/record_rdata.h"
#include "url/gurl.h"

namespace net {

class DnsResponse;
class DnsSession;
class NetLogWithSource;
class URLRequestContext;

// DnsTransaction implements a stub DNS resolver as defined in RFC 1034.
// The DnsTransaction takes care of retransmissions, name server fallback (or
// round-robin), suffix search, and simple response validation ("does it match
// the query") to fight poisoning.
//
// Destroying DnsTransaction cancels the underlying network effort.
class NET_EXPORT_PRIVATE DnsTransaction {
 public:
  virtual ~DnsTransaction() {}

  // Returns the original |hostname|.
  virtual const std::string& GetHostname() const = 0;

  // Returns the |qtype|.
  virtual uint16_t GetType() const = 0;

  // Starts the transaction.  Always completes asynchronously.
  virtual void Start() = 0;

  virtual void SetRequestContext(URLRequestContext*) = 0;

  virtual void SetRequestPriority(RequestPriority priority) = 0;
};

// Creates DnsTransaction which performs asynchronous DNS search.
// It does NOT perform caching, aggregation or prioritization of transactions.
//
// Destroying the factory does NOT affect any already created DnsTransactions.
class NET_EXPORT_PRIVATE DnsTransactionFactory {
 public:
  // Called with the response or NULL if no matching response was received.
  // Note that the |GetDottedName()| of the response may be different than the
  // original |hostname| as a result of suffix search. |secure| is true if the
  // response was obtained using secure DNS.
  typedef base::OnceCallback<void(DnsTransaction* transaction,
                                  int neterror,
                                  const DnsResponse* response,
                                  bool secure)>
      CallbackType;

  virtual ~DnsTransactionFactory() {}

  // Creates DnsTransaction for the given |hostname| and |qtype| (assuming
  // QCLASS is IN). |hostname| should be in the dotted form. A dot at the end
  // implies the domain name is fully-qualified and will be exempt from suffix
  // search. |hostname| should not be an IP literal.
  //
  // The transaction will run |callback| upon asynchronous completion.
  // The |net_log| is used as the parent log.
  //
  // The |secure_dns_mode| specifies the order in which secure and/or insecure
  // DNS lookups will be performed. In SECURE mode, only secure lookups will be
  // perfomed. In AUTOMATIC mode, secure lookups will be performed first when
  // possible, and insecure lookups will be performed as a fallback. In OFF
  // mode, only insecure lookups will be performed.
  virtual std::unique_ptr<DnsTransaction> CreateTransaction(
      const std::string& hostname,
      uint16_t qtype,
      CallbackType callback,
      const NetLogWithSource& net_log,
      SecureDnsMode secure_dns_mode) WARN_UNUSED_RESULT = 0;

  // The given EDNS0 option will be included in all DNS queries performed by
  // transactions from this factory.
  virtual void AddEDNSOption(const OptRecordRdata::Opt& opt) = 0;

  // Creates a DnsTransactionFactory which creates DnsTransactionImpl using the
  // |session|.
  static std::unique_ptr<DnsTransactionFactory> CreateFactory(
      DnsSession* session) WARN_UNUSED_RESULT;
};

}  // namespace net

#endif  // NET_DNS_DNS_TRANSACTION_H_
