// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_GAIA_COOKIE_MANAGER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_GAIA_COOKIE_MANAGER_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

template <typename T>
struct DefaultSingletonTraits;
class GaiaCookieManagerService;

namespace ios {

class ChromeBrowserState;

// Singleton that owns the GaiaCookieManagerService(s) and associates them with
// browser states.
class GaiaCookieManagerServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the instance of GaiaCookieManagerService associated with this
  // browser state (creating one if none exists). Returns null if this browser
  // state cannot have an GaiaCookieManagerService (for example, if it is
  // incognito).
  static GaiaCookieManagerService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  // Returns an instance of the factory singleton.
  static GaiaCookieManagerServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<GaiaCookieManagerServiceFactory>;

  GaiaCookieManagerServiceFactory();
  ~GaiaCookieManagerServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  scoped_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(GaiaCookieManagerServiceFactory);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_GAIA_COOKIE_MANAGER_SERVICE_FACTORY_H_
