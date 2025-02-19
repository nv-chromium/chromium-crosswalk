// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/oauth2_token_service.h"

#if !defined(OS_CHROMEOS)
#include "net/base/network_change_notifier.h"
#endif

namespace content_settings {
class CookieSettings;
}

class Profile;

class ChromeSigninClient
    : public SigninClient,
#if !defined(OS_CHROMEOS)
      public net::NetworkChangeNotifier::NetworkChangeObserver,
#endif
      public SigninErrorController::Observer,
      public gaia::GaiaOAuthClient::Delegate,
      public OAuth2TokenService::Consumer {
 public:
  explicit ChromeSigninClient(
      Profile* profile, SigninErrorController* signin_error_controller);
  ~ChromeSigninClient() override;
  void Shutdown() override;
  void DoFinalInit() override;

  // Utility method.
  static bool ProfileAllowsSigninCookies(Profile* profile);

  // SigninClient implementation.
  PrefService* GetPrefs() override;
  scoped_refptr<TokenWebData> GetDatabase() override;
  bool CanRevokeCredentials() override;
  std::string GetSigninScopedDeviceId() override;
  void OnSignedOut() override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  bool ShouldMergeSigninCredentialsIntoCookieJar() override;
  bool IsFirstRun() const override;
  base::Time GetInstallDate() override;
  bool AreSigninCookiesAllowed() override;
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;
  void DelayNetworkCall(const base::Closure& callback) override;
  GaiaAuthFetcher* CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      const std::string& source,
      net::URLRequestContextGetter* getter) override;

  // Returns a string describing the chrome version environment. Version format:
  // <Build Info> <OS> <Version number> (<Last change>)<channel or "-devel">
  // If version information is unavailable, returns "invalid."
  std::string GetProductVersion() override;
  scoped_ptr<CookieChangedSubscription> AddCookieChangedCallback(
      const GURL& url,
      const std::string& name,
      const net::CookieStore::CookieChangedCallback& callback) override;
  void OnSignedIn(const std::string& account_id,
                  const std::string& gaia_id,
                  const std::string& username,
                  const std::string& password) override;
  void PostSignedIn(const std::string& account_id,
                    const std::string& username,
                    const std::string& password) override;
  bool UpdateAccountInfo(
      AccountTrackerService::AccountInfo* out_account_info) override;

  // SigninErrorController::Observer implementation.
  void OnErrorChanged() override;

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnGetTokenInfoResponse(
      scoped_ptr<base::DictionaryValue> token_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  // OAuth2TokenService::Consumer implementation
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

#if !defined(OS_CHROMEOS)
  // net::NetworkChangeController::NetworkChangeObserver implementation.
  void OnNetworkChanged(net::NetworkChangeNotifier::ConnectionType type)
      override;
#endif

 private:
  void MaybeFetchSigninTokenHandle();

  Profile* profile_;

  SigninErrorController* signin_error_controller_;
#if !defined(OS_CHROMEOS)
  std::list<base::Closure> delayed_callbacks_;
#endif

  scoped_ptr<gaia::GaiaOAuthClient> oauth_client_;
  scoped_ptr<OAuth2TokenService::Request> oauth_request_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSigninClient);
};

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_
