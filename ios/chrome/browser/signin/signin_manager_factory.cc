// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/signin_manager_factory.h"

#include "base/memory/singleton.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/time/time.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#include "ios/chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "ios/chrome/browser/signin/oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory_observer.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"

namespace ios {

SigninManagerFactory::SigninManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "SigninManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(SigninClientFactory::GetInstance());
  DependsOn(ios::GaiaCookieManagerServiceFactory::GetInstance());
  DependsOn(OAuth2TokenServiceFactory::GetInstance());
  DependsOn(ios::AccountTrackerServiceFactory::GetInstance());
}

SigninManagerFactory::~SigninManagerFactory() {}

// static
SigninManager* SigninManagerFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<SigninManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
SigninManager* SigninManagerFactory::GetForBrowserStateIfExists(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<SigninManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
SigninManagerFactory* SigninManagerFactory::GetInstance() {
  return Singleton<SigninManagerFactory>::get();
}

void SigninManagerFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SigninManagerBase::RegisterProfilePrefs(registry);
}

// static
void SigninManagerFactory::RegisterPrefs(PrefRegistrySimple* registry) {
  SigninManagerBase::RegisterPrefs(registry);
}

void SigninManagerFactory::AddObserver(SigninManagerFactoryObserver* observer) {
  observer_list_.AddObserver(observer);
}

void SigninManagerFactory::RemoveObserver(
    SigninManagerFactoryObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void SigninManagerFactory::NotifyObserversOfSigninManagerCreationForTesting(
    SigninManager* manager) {
  FOR_EACH_OBSERVER(SigninManagerFactoryObserver, observer_list_,
                    SigninManagerCreated(manager));
}

scoped_ptr<KeyedService> SigninManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  scoped_ptr<SigninManager> service(new SigninManager(
      SigninClientFactory::GetForBrowserState(chrome_browser_state),
      OAuth2TokenServiceFactory::GetForBrowserState(chrome_browser_state),
      ios::AccountTrackerServiceFactory::GetForBrowserState(
          chrome_browser_state),
      ios::GaiaCookieManagerServiceFactory::GetForBrowserState(
          chrome_browser_state)));
  service->Initialize(GetApplicationContext()->GetLocalState());
  FOR_EACH_OBSERVER(SigninManagerFactoryObserver, observer_list_,
                    SigninManagerCreated(service.get()));
  return service.Pass();
}

void SigninManagerFactory::BrowserStateShutdown(web::BrowserState* context) {
  SigninManager* manager =
      static_cast<SigninManager*>(GetServiceForBrowserState(context, false));
  if (manager)
    FOR_EACH_OBSERVER(SigninManagerFactoryObserver, observer_list_,
                      SigninManagerShutdown(manager));
  BrowserStateKeyedServiceFactory::BrowserStateShutdown(context);
}

}  // namespace ios
