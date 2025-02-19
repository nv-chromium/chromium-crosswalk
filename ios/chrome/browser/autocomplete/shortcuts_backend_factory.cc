// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autocomplete/shortcuts_backend_factory.h"

#include "base/memory/singleton.h"
#include "base/prefs/pref_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/omnibox/browser/shortcuts_constants.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/web/public/web_thread.h"

namespace ios {
namespace {

scoped_refptr<ShortcutsBackend> CreateShortcutsBackend(
    ios::ChromeBrowserState* browser_state,
    bool suppress_db) {
  scoped_refptr<ShortcutsBackend> shortcuts_backend(new ShortcutsBackend(
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state),
      make_scoped_ptr(new ios::UIThreadSearchTermsData(browser_state)),
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::DB),
      browser_state->GetStatePath().Append(kShortcutsDatabaseName),
      suppress_db));
  return shortcuts_backend->Init() ? shortcuts_backend : nullptr;
}

}  // namespace

// static
scoped_refptr<ShortcutsBackend> ShortcutsBackendFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return make_scoped_refptr(static_cast<ShortcutsBackend*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true).get()));
}

// static
scoped_refptr<ShortcutsBackend>
ShortcutsBackendFactory::GetForBrowserStateIfExists(
    ios::ChromeBrowserState* browser_state) {
  return make_scoped_refptr(static_cast<ShortcutsBackend*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false).get()));
}

// static
ShortcutsBackendFactory* ShortcutsBackendFactory::GetInstance() {
  return Singleton<ShortcutsBackendFactory>::get();
}

ShortcutsBackendFactory::ShortcutsBackendFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "ShortcutsBackend",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
}

ShortcutsBackendFactory::~ShortcutsBackendFactory() {}

scoped_refptr<RefcountedKeyedService>
ShortcutsBackendFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return CreateShortcutsBackend(browser_state, false /* suppress_db */);
}

bool ShortcutsBackendFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
