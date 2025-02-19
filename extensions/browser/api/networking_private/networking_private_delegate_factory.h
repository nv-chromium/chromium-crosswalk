// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_FACTORY_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/api/networking_private/networking_private_delegate.h"

namespace context {
class BrowserContext;
}

namespace extensions {

#if defined(OS_WIN) || defined(OS_MACOSX)
class NetworkingPrivateServiceClient;
#endif

// Factory for creating NetworkingPrivateDelegate instances as a keyed service.
// NetworkingPrivateDelegate supports the networkingPrivate API.
class NetworkingPrivateDelegateFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // There needs to be a way to allow the application (e.g. Chrome) to provide
  // additional delegates to the API (in src/extensions). Since this factory is
  // already a singleton, it provides a good place to hold these delegate
  // factories. See NetworkingPrivateDelegate for the delegate declarations.

  class VerifyDelegateFactory {
   public:
    VerifyDelegateFactory();
    virtual ~VerifyDelegateFactory();

    virtual scoped_ptr<NetworkingPrivateDelegate::VerifyDelegate>
    CreateDelegate() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(VerifyDelegateFactory);
  };

  class UIDelegateFactory {
   public:
    UIDelegateFactory();
    virtual ~UIDelegateFactory();

    virtual scoped_ptr<NetworkingPrivateDelegate::UIDelegate>
    CreateDelegate() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(UIDelegateFactory);
  };

  // Provide optional factories for creating delegate instances.
  void SetVerifyDelegateFactory(scoped_ptr<VerifyDelegateFactory> factory);
  void SetUIDelegateFactory(scoped_ptr<UIDelegateFactory> factory);

  static NetworkingPrivateDelegate* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static NetworkingPrivateDelegateFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<NetworkingPrivateDelegateFactory>;

  NetworkingPrivateDelegateFactory();
  ~NetworkingPrivateDelegateFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  scoped_ptr<VerifyDelegateFactory> verify_factory_;
  scoped_ptr<UIDelegateFactory> ui_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateDelegateFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_FACTORY_H_
