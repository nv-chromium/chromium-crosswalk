// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_client.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_form_manager.h"

namespace password_manager {

namespace {

// This filter does not filter out anything, it is a dummy implementation of
// the filter interface.
class PassThroughCredentialsFilter : public CredentialsFilter {
 public:
  ScopedVector<autofill::PasswordForm> FilterResults(
      ScopedVector<autofill::PasswordForm> results) const override {
    return results.Pass();
  }
};
}

StubPasswordManagerClient::StubPasswordManagerClient() {
}

StubPasswordManagerClient::~StubPasswordManagerClient() {
}

std::string StubPasswordManagerClient::GetSyncUsername() const {
  return std::string();
}

bool StubPasswordManagerClient::IsSyncAccountCredential(
    const std::string& username,
    const std::string& realm) const {
  return false;
}

bool StubPasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    scoped_ptr<PasswordFormManager> form_to_save,
    password_manager::CredentialSourceType type,
    bool update_password) {
  return false;
}

bool StubPasswordManagerClient::PromptUserToChooseCredentials(
    ScopedVector<autofill::PasswordForm> local_forms,
    ScopedVector<autofill::PasswordForm> federated_forms,
    const GURL& origin,
    base::Callback<void(const password_manager::CredentialInfo&)> callback) {
  return false;
}

void StubPasswordManagerClient::NotifyUserAutoSignin(
    ScopedVector<autofill::PasswordForm> local_forms) {
}

void StubPasswordManagerClient::AutomaticPasswordSave(
    scoped_ptr<PasswordFormManager> saved_manager) {
}

PrefService* StubPasswordManagerClient::GetPrefs() {
  return nullptr;
}

PasswordStore* StubPasswordManagerClient::GetPasswordStore() const {
  return nullptr;
}

const GURL& StubPasswordManagerClient::GetLastCommittedEntryURL() const {
  return GURL::EmptyGURL();
}

scoped_ptr<CredentialsFilter>
StubPasswordManagerClient::CreateStoreResultFilter() const {
  return make_scoped_ptr(new PassThroughCredentialsFilter);
}

}  // namespace password_manager
