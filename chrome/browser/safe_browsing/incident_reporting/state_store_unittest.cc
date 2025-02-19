// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/state_store.h"

#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prefs/pref_service_syncable_factory.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

// A test fixture with a testing profile that writes its user prefs to a json
// file.
class StateStoreTest : public ::testing::Test {
 protected:
  struct TestData {
    IncidentType type;
    const char* key;
    uint32_t digest;
  };

  StateStoreTest()
      : profile_(nullptr),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(profile_manager_.SetUp());
    CreateProfile();
  }

  void DeleteProfile() {
    if (profile_) {
      profile_ = nullptr;
      profile_manager_.DeleteTestingProfile(kProfileName_);
    }
  }

  // Removes the safebrowsing.incidents_sent preference from the profile's pref
  // store.
  void TrimPref() {
    ASSERT_EQ(nullptr, profile_);
    scoped_ptr<base::Value> prefs(JSONFileValueDeserializer(GetPrefsPath())
                                      .Deserialize(nullptr, nullptr));
    ASSERT_NE(nullptr, prefs.get());
    base::DictionaryValue* dict = nullptr;
    ASSERT_TRUE(prefs->GetAsDictionary(&dict));
    ASSERT_TRUE(dict->Remove(prefs::kSafeBrowsingIncidentsSent, nullptr));
    ASSERT_TRUE(JSONFileValueSerializer(GetPrefsPath()).Serialize(*dict));
  }

  void CreateProfile() {
    ASSERT_EQ(nullptr, profile_);
    // Create the testing profile with a file-backed user pref store.
    PrefServiceSyncableFactory factory;
    factory.SetUserPrefsFile(GetPrefsPath(), task_runner_.get());
    user_prefs::PrefRegistrySyncable* pref_registry =
        new user_prefs::PrefRegistrySyncable();
    chrome::RegisterUserProfilePrefs(pref_registry);
    profile_ = profile_manager_.CreateTestingProfile(
        kProfileName_, factory.CreateSyncable(pref_registry).Pass(),
        base::UTF8ToUTF16(kProfileName_), 0, std::string(),
        TestingProfile::TestingFactories());
  }

  static const char kProfileName_[];
  static const TestData kTestData_[];
  TestingProfile* profile_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

 private:
  base::FilePath GetPrefsPath() {
    return temp_dir_.path().AppendASCII("prefs");
  }

  base::ScopedTempDir temp_dir_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;
  TestingProfileManager profile_manager_;

  DISALLOW_COPY_AND_ASSIGN(StateStoreTest);
};

// static
const char StateStoreTest::kProfileName_[] = "test_profile";
const StateStoreTest::TestData StateStoreTest::kTestData_[] = {
    {IncidentType::TRACKED_PREFERENCE, "tp_one", 1},
    {IncidentType::TRACKED_PREFERENCE, "tp_two", 2},
    {IncidentType::TRACKED_PREFERENCE, "tp_three", 3},
    {IncidentType::BINARY_INTEGRITY, "bi", 0},
    {IncidentType::BLACKLIST_LOAD, "bl", 0x47},
};

TEST_F(StateStoreTest, MarkAsAndHasBeenReported) {
  StateStore state_store(profile_);

  for (const auto& data : kTestData_)
    ASSERT_FALSE(state_store.HasBeenReported(data.type, data.key, data.digest));

  {
    StateStore::Transaction transaction(&state_store);
    for (const auto& data : kTestData_) {
      transaction.MarkAsReported(data.type, data.key, data.digest);
      ASSERT_TRUE(
          state_store.HasBeenReported(data.type, data.key, data.digest));
    }
  }

  for (const auto& data : kTestData_)
    ASSERT_TRUE(state_store.HasBeenReported(data.type, data.key, data.digest));
}

TEST_F(StateStoreTest, ClearForType) {
  StateStore state_store(profile_);

  {
    StateStore::Transaction transaction(&state_store);
    for (const auto& data : kTestData_)
      transaction.MarkAsReported(data.type, data.key, data.digest);
  }

  for (const auto& data : kTestData_)
    ASSERT_TRUE(state_store.HasBeenReported(data.type, data.key, data.digest));

  const IncidentType removed_type = IncidentType::TRACKED_PREFERENCE;
  StateStore::Transaction(&state_store).ClearForType(removed_type);

  for (const auto& data : kTestData_) {
    if (data.type == removed_type) {
      ASSERT_FALSE(
          state_store.HasBeenReported(data.type, data.key, data.digest));
    } else {
      ASSERT_TRUE(
          state_store.HasBeenReported(data.type, data.key, data.digest));
    }
  }
}

TEST_F(StateStoreTest, Persistence) {
  // Write some state to the store.
  {
    StateStore state_store(profile_);
    StateStore::Transaction transaction(&state_store);
    for (const auto& data : kTestData_)
      transaction.MarkAsReported(data.type, data.key, data.digest);
  }

  // Run tasks to write prefs out to the JsonPrefStore.
  task_runner_->RunUntilIdle();

  // Delete the profile.
  DeleteProfile();

  // Recreate the profile.
  CreateProfile();

  // Verify that the state survived.
  StateStore state_store(profile_);
  for (const auto& data : kTestData_)
    ASSERT_TRUE(state_store.HasBeenReported(data.type, data.key, data.digest));
}

TEST_F(StateStoreTest, PersistenceWithStoreDelete) {
  // Write some state to the store.
  {
    StateStore state_store(profile_);
    StateStore::Transaction transaction(&state_store);
    for (const auto& data : kTestData_)
      transaction.MarkAsReported(data.type, data.key, data.digest);
  }

  // Run tasks to write prefs out to the JsonPrefStore.
  task_runner_->RunUntilIdle();

  // Delete the profile.
  DeleteProfile();

  // Delete the state pref.
  TrimPref();

  // Recreate the profile.
  CreateProfile();

  // Verify that the state did not survive.
  StateStore state_store(profile_);
  for (const auto& data : kTestData_)
    ASSERT_FALSE(state_store.HasBeenReported(data.type, data.key, data.digest));
}

}  // namespace safe_browsing
