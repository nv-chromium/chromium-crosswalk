// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/profile_sync_components_factory_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_android.h"
#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/sync_driver/data_type_controller.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "sync/internal_api/public/base/model_type.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileSyncServiceAndroidTest : public testing::Test {
 public:
  ProfileSyncServiceAndroidTest()
      : command_line_(base::CommandLine::NO_PROGRAM) {}
  ~ProfileSyncServiceAndroidTest() override {}

  void SetUp() override {
    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(&profile_);
    ProfileSyncComponentsFactory* factory =
        new ProfileSyncComponentsFactoryImpl(
            &profile_,
            &command_line_,
            GURL(),
            token_service,
            profile_.GetRequestContext());
    sync_service_.reset(new ProfileSyncService(
        make_scoped_ptr(factory),
        &profile_,
        scoped_ptr<SupervisedUserSigninManagerWrapper>(),
        token_service,
        browser_sync::MANUAL_START));
    factory->RegisterDataTypes(sync_service_.get());
  }

 protected:
  syncer::ModelTypeSet GetRegisteredDataTypes() {
    sync_driver::DataTypeController::StateMap controller_states;
    sync_service_->GetDataTypeControllerStates(&controller_states);
    syncer::ModelTypeSet model_types;
    for (sync_driver::DataTypeController::StateMap::const_iterator it =
             controller_states.begin();
         it != controller_states.end(); ++it) {
      model_types.Put(it->first);
    }
    return model_types;
  }

 private:
  base::CommandLine command_line_;
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  scoped_ptr<ProfileSyncService> sync_service_;
};
