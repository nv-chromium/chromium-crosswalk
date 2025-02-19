// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class LoginUIService;
class Profile;

// Singleton that owns all LoginUIServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated LoginUIService.
class LoginUIServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of LoginUIService associated with this profile
  // (creating one if none exists). Returns NULL if this profile cannot have a
  // LoginUIService (for example, if |profile| is incognito).
  static LoginUIService* GetForProfile(Profile* profile);

  // Returns an instance of the LoginUIServiceFactory singleton.
  static LoginUIServiceFactory* GetInstance();

  // Helper method that returns a closure displaying the login popup for
  // |profile|.
  // This closure must not be called after the LoginUIService is destroyed.
  static base::Closure GetShowLoginPopupCallbackForProfile(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<LoginUIServiceFactory>;

  LoginUIServiceFactory();
  ~LoginUIServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(LoginUIServiceFactory);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_FACTORY_H_
