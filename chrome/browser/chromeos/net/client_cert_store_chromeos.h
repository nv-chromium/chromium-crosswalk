// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_CLIENT_CERT_STORE_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_NET_CLIENT_CERT_STORE_CHROMEOS_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "net/ssl/client_cert_store_nss.h"

namespace net {
class X509Certificate;
}

namespace chromeos {

class ClientCertStoreChromeOS : public net::ClientCertStoreNSS {
 public:
  class CertFilter {
   public:
    virtual ~CertFilter() {}

    // Initializes this filter. Returns true if it finished initialization,
    // otherwise returns false and calls |callback| once the initialization is
    // completed.
    // Must be called at most once.
    virtual bool Init(const base::Closure& callback) = 0;

    // Returns true if |cert| is allowed to be used as a client certificate
    // (e.g. for a certain browser context or user).
    // This is only called once initialization is finished, see Init().
    virtual bool IsCertAllowed(
        const scoped_refptr<net::X509Certificate>& cert) const = 0;
  };

  // This ClientCertStore will return only client certs that pass the filter
  // |cert_filter|.
  ClientCertStoreChromeOS(
      scoped_ptr<CertFilter> cert_filter,
      const PasswordDelegateFactory& password_delegate_factory);
  ~ClientCertStoreChromeOS() override;

  // net::ClientCertStoreNSS:
  void GetClientCerts(const net::SSLCertRequestInfo& cert_request_info,
                      net::CertificateList* selected_certs,
                      const base::Closure& callback) override;

 protected:
  // net::ClientCertStoreNSS:
  void GetClientCertsImpl(CERTCertList* cert_list,
                          const net::SSLCertRequestInfo& request,
                          bool query_nssdb,
                          net::CertificateList* selected_certs) override;

 private:
  void CertFilterInitialized(const net::SSLCertRequestInfo* request,
                             net::CertificateList* selected_certs,
                             const base::Closure& callback);

  scoped_ptr<CertFilter> cert_filter_;

  DISALLOW_COPY_AND_ASSIGN(ClientCertStoreChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_CLIENT_CERT_STORE_CHROMEOS_H_
