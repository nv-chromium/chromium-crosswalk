// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_integration_service.h"

#include "chrome/browser/chromeos/drive/dummy_file_system.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/drive/service/dummy_drive_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

namespace{
const char kTestProfileName[] = "test-profile";
}

class DriveIntegrationServiceTest : public testing::Test {
 public:
  DriveIntegrationServiceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  // DriveIntegrationService depends on DriveNotificationManager which depends
  // on InvalidationService. On Chrome OS, the InvalidationServiceFactory
  // uses chromeos::ProfileHelper, which needs the ProfileManager or a
  // TestProfileManager to be running.
  TestingProfileManager profile_manager_;
};

TEST_F(DriveIntegrationServiceTest, InitializeAndShutdown) {
  scoped_ptr<DriveIntegrationService> integration_service(
      new DriveIntegrationService(
          profile_manager_.CreateTestingProfile(kTestProfileName),
          NULL,
          new DummyDriveService,
          std::string(),
          base::FilePath(),
          new DummyFileSystem));
  integration_service->SetEnabled(true);
  content::RunAllBlockingPoolTasksUntilIdle();
  integration_service->Shutdown();
}

TEST_F(DriveIntegrationServiceTest, ServiceInstanceIdentity) {
  TestingProfile* user1 = profile_manager_.CreateTestingProfile("user1");

  // Integration Service is created as a profile keyed service.
  EXPECT_TRUE(DriveIntegrationServiceFactory::GetForProfile(user1));

  // Shares the same instance with the incognito mode profile.
  Profile* user1_incognito = user1->GetOffTheRecordProfile();
  EXPECT_EQ(DriveIntegrationServiceFactory::GetForProfile(user1),
            DriveIntegrationServiceFactory::GetForProfile(user1_incognito));

  // For different profiles, different services are running.
  TestingProfile* user2 = profile_manager_.CreateTestingProfile("user2");
  EXPECT_NE(DriveIntegrationServiceFactory::GetForProfile(user1),
            DriveIntegrationServiceFactory::GetForProfile(user2));
}

}  // namespace drive
