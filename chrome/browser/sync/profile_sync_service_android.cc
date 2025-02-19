// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/about_sync_util.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/pref_names.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "google/cacheinvalidation/types.pb.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "jni/ProfileSyncService_jni.h"
#include "sync/internal_api/public/network_resources.h"
#include "sync/internal_api/public/read_transaction.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace {

// Native callback for the JNI GetAllNodes method. When
// ProfileSyncService::GetAllNodes completes, this method is called and the
// results are sent to the Java callback.
void NativeGetAllNodesCallback(
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    scoped_ptr<base::ListValue> result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string json_string;
  if (!result.get() || !base::JSONWriter::Write(*result, &json_string)) {
    DVLOG(1) << "Writing as JSON failed. Passing empty string to Java code.";
    json_string = std::string();
  }

  ScopedJavaLocalRef<jstring> java_json_string =
      ConvertUTF8ToJavaString(env, json_string);
  Java_ProfileSyncService_onGetAllNodesResult(env,
                                              callback.obj(),
                                              java_json_string.obj());
}

}  // namespace

ProfileSyncServiceAndroid::ProfileSyncServiceAndroid(JNIEnv* env, jobject obj)
    : profile_(NULL),
      sync_service_(NULL),
      weak_java_profile_sync_service_(env, obj) {
  if (g_browser_process == NULL ||
      g_browser_process->profile_manager() == NULL) {
    NOTREACHED() << "Browser process or profile manager not initialized";
    return;
  }

  profile_ = ProfileManager::GetActiveUserProfile();
  if (profile_ == NULL) {
    NOTREACHED() << "Sync Init: Profile not found.";
    return;
  }

  sync_prefs_.reset(new sync_driver::SyncPrefs(profile_->GetPrefs()));

  sync_service_ =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  DCHECK(sync_service_);
}

void ProfileSyncServiceAndroid::Init() {
  sync_service_->AddObserver(this);
}

void ProfileSyncServiceAndroid::RemoveObserver() {
  if (sync_service_->HasObserver(this)) {
    sync_service_->RemoveObserver(this);
  }
}

ProfileSyncServiceAndroid::~ProfileSyncServiceAndroid() {
  RemoveObserver();
}

void ProfileSyncServiceAndroid::OnStateChanged() {
  // Notify the java world that our sync state has changed.
  JNIEnv* env = AttachCurrentThread();
  Java_ProfileSyncService_syncStateChanged(
      env, weak_java_profile_sync_service_.get(env).obj());
}

jboolean ProfileSyncServiceAndroid::IsPassphrasePrompted(JNIEnv* env,
                                                         jobject obj) {
  const std::string group_name =
      base::FieldTrialList::FindFullName("LimitSyncPassphrasePrompt");
  if (group_name != "Enabled")
    return false;
  return sync_prefs_->IsPassphrasePrompted();
}

void ProfileSyncServiceAndroid::SetPassphrasePrompted(JNIEnv* env,
                                                      jobject obj,
                                                      jboolean prompted) {
  sync_prefs_->SetPassphrasePrompted(prompted);
}

void ProfileSyncServiceAndroid::RequestStart(JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->RequestStart();
}

void ProfileSyncServiceAndroid::RequestStop(JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->RequestStop(ProfileSyncService::KEEP_DATA);
}

void ProfileSyncServiceAndroid::SignOutSync(JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile_);
  sync_service_->RequestStop(ProfileSyncService::CLEAR_DATA);
}

void ProfileSyncServiceAndroid::FlushDirectory(JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->FlushDirectory();
}

ScopedJavaLocalRef<jstring> ProfileSyncServiceAndroid::QuerySyncStatusSummary(
    JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile_);
  std::string status(sync_service_->QuerySyncStatusSummaryString());
  return ConvertUTF8ToJavaString(env, status);
}

void ProfileSyncServiceAndroid::GetAllNodes(JNIEnv* env,
                                            jobject obj,
                                            jobject callback) {
  base::android::ScopedJavaGlobalRef<jobject> java_callback;
  java_callback.Reset(env, callback);

  base::Callback<void(scoped_ptr<base::ListValue>)> native_callback =
      base::Bind(&NativeGetAllNodesCallback, java_callback);
  sync_service_->GetAllNodes(native_callback);
}

void ProfileSyncServiceAndroid::SetSyncSessionsId(
    JNIEnv* env, jobject obj, jstring tag) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile_);
  std::string machine_tag = ConvertJavaStringToUTF8(env, tag);
  sync_prefs_->SetSyncSessionsGUID(machine_tag);
}

jint ProfileSyncServiceAndroid::GetAuthError(JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetAuthError().state();
}

jboolean ProfileSyncServiceAndroid::IsEncryptEverythingEnabled(
    JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->EncryptEverythingEnabled();
}

jboolean ProfileSyncServiceAndroid::IsSyncInitialized(JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->backend_initialized();
}

jboolean ProfileSyncServiceAndroid::IsFirstSetupInProgress(
    JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->FirstSetupInProgress();
}

jboolean ProfileSyncServiceAndroid::IsEncryptEverythingAllowed(
    JNIEnv* env, jobject obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->EncryptEverythingAllowed();
}

jboolean ProfileSyncServiceAndroid::IsPassphraseRequired(JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsPassphraseRequired();
}

jboolean ProfileSyncServiceAndroid::IsPassphraseRequiredForDecryption(
    JNIEnv* env, jobject obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // In case of CUSTOM_PASSPHRASE we always sync passwords. Prompt the user for
  // a passphrase if cryptographer has any pending keys.
  if (sync_service_->GetPassphraseType() == syncer::CUSTOM_PASSPHRASE) {
    return !IsCryptographerReady(env, obj);
  }
  if (sync_service_->IsPassphraseRequiredForDecryption()) {
    // Passwords datatype should never prompt for a passphrase, except when
    // user is using a custom passphrase. Do not prompt for a passphrase if
    // passwords are the only encrypted datatype. This prevents a temporary
    // notification for passphrase  when PSS has not completed configuring
    // DataTypeManager, after configuration password datatype shall be disabled.
    const syncer::ModelTypeSet encrypted_types =
        sync_service_->GetEncryptedDataTypes();
    return !encrypted_types.Equals(syncer::ModelTypeSet(syncer::PASSWORDS));
  }
  return false;
}

jboolean ProfileSyncServiceAndroid::IsPassphraseRequiredForExternalType(
    JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return
      sync_service_->passphrase_required_reason() == syncer::REASON_DECRYPTION;
}

jboolean ProfileSyncServiceAndroid::IsUsingSecondaryPassphrase(
    JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsUsingSecondaryPassphrase();
}

jboolean ProfileSyncServiceAndroid::SetDecryptionPassphrase(
    JNIEnv* env, jobject obj, jstring passphrase) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string key = ConvertJavaStringToUTF8(env, passphrase);
  return sync_service_->SetDecryptionPassphrase(key);
}

void ProfileSyncServiceAndroid::SetEncryptionPassphrase(
    JNIEnv* env, jobject obj, jstring passphrase) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string key = ConvertJavaStringToUTF8(env, passphrase);
  sync_service_->SetEncryptionPassphrase(key, ProfileSyncService::EXPLICIT);
}

jboolean ProfileSyncServiceAndroid::IsCryptographerReady(JNIEnv* env, jobject) {
  syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  return sync_service_->IsCryptographerReady(&trans);
}

jint ProfileSyncServiceAndroid::GetPassphraseType(JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetPassphraseType();
}

jboolean ProfileSyncServiceAndroid::HasExplicitPassphraseTime(
    JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  return !passphrase_time.is_null();
}

jlong ProfileSyncServiceAndroid::GetExplicitPassphraseTime(
        JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  return passphrase_time.ToJavaTime();
}

ScopedJavaLocalRef<jstring>
    ProfileSyncServiceAndroid::GetSyncEnterGooglePassphraseBodyWithDateText(
        JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  base::string16 passphrase_time_str =
      base::TimeFormatShortDate(passphrase_time);
  return base::android::ConvertUTF16ToJavaString(env,
      l10n_util::GetStringFUTF16(
        IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY_WITH_DATE,
        passphrase_time_str));
}

ScopedJavaLocalRef<jstring>
    ProfileSyncServiceAndroid::GetSyncEnterCustomPassphraseBodyWithDateText(
        JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  base::string16 passphrase_time_str =
      base::TimeFormatShortDate(passphrase_time);
  return base::android::ConvertUTF16ToJavaString(env,
      l10n_util::GetStringFUTF16(IDS_SYNC_ENTER_PASSPHRASE_BODY_WITH_DATE,
        passphrase_time_str));
}

ScopedJavaLocalRef<jstring>
    ProfileSyncServiceAndroid::GetCurrentSignedInAccountText(
        JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const std::string& sync_username =
      SigninManagerFactory::GetForProfile(profile_)->GetAuthenticatedUsername();
  return base::android::ConvertUTF16ToJavaString(env,
      l10n_util::GetStringFUTF16(
          IDS_SYNC_ACCOUNT_SYNCING_TO_USER,
          base::ASCIIToUTF16(sync_username)));
}

ScopedJavaLocalRef<jstring>
    ProfileSyncServiceAndroid::GetSyncEnterCustomPassphraseBodyText(
        JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_SYNC_ENTER_PASSPHRASE_BODY));
}

jboolean ProfileSyncServiceAndroid::IsSyncKeystoreMigrationDone(
      JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  syncer::SyncStatus status;
  bool is_status_valid = sync_service_->QueryDetailedSyncStatus(&status);
  return is_status_valid && !status.keystore_migration_time.is_null();
}

ScopedJavaLocalRef<jintArray> ProfileSyncServiceAndroid::GetActiveDataTypes(
      JNIEnv* env, jobject obj) {
  syncer::ModelTypeSet types = sync_service_->GetActiveDataTypes();
  types.PutAll(syncer::ControlTypes());
  return ModelTypeSetToJavaIntArray(env, types);
}

ScopedJavaLocalRef<jintArray> ProfileSyncServiceAndroid::GetPreferredDataTypes(
      JNIEnv* env, jobject obj) {
  syncer::ModelTypeSet types = sync_service_->GetPreferredDataTypes();
  types.PutAll(syncer::ControlTypes());
  return ModelTypeSetToJavaIntArray(env, types);
}

void ProfileSyncServiceAndroid::SetPreferredDataTypes(
    JNIEnv* env, jobject obj,
    jboolean sync_everything,
    jintArray model_type_array) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<int> types_vector;
  base::android::JavaIntArrayToIntVector(env, model_type_array, &types_vector);
  syncer::ModelTypeSet types;
  for (size_t i = 0; i < types_vector.size(); i++) {
    types.Put(static_cast<syncer::ModelType>(types_vector[i]));
  }
  types.RetainAll(syncer::UserSelectableTypes());
  sync_service_->OnUserChoseDatatypes(sync_everything, types);
}

void ProfileSyncServiceAndroid::SetSetupInProgress(
    JNIEnv* env, jobject obj, jboolean in_progress) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->SetSetupInProgress(in_progress);
}

void ProfileSyncServiceAndroid::SetSyncSetupCompleted(
    JNIEnv* env, jobject obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->SetSyncSetupCompleted();
}

jboolean ProfileSyncServiceAndroid::HasSyncSetupCompleted(
    JNIEnv* env, jobject obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->HasSyncSetupCompleted();
}

jboolean ProfileSyncServiceAndroid::IsSyncRequested(
    JNIEnv* env, jobject obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsSyncRequested();
}

jboolean ProfileSyncServiceAndroid::IsSyncActive(JNIEnv* env, jobject obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsSyncActive();
}

void ProfileSyncServiceAndroid::EnableEncryptEverything(
    JNIEnv* env, jobject obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->EnableEncryptEverything();
}

jboolean ProfileSyncServiceAndroid::HasKeepEverythingSynced(
    JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_prefs_->HasKeepEverythingSynced();
}

jboolean ProfileSyncServiceAndroid::HasUnrecoverableError(
    JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->HasUnrecoverableError();
}

ScopedJavaLocalRef<jstring> ProfileSyncServiceAndroid::GetAboutInfoForTest(
    JNIEnv* env, jobject) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_ptr<base::DictionaryValue> about_info =
      sync_ui_util::ConstructAboutInformation(sync_service_);
  std::string about_info_json;
  base::JSONWriter::Write(*about_info, &about_info_json);

  return ConvertUTF8ToJavaString(env, about_info_json);
}

jlong ProfileSyncServiceAndroid::GetLastSyncedTimeForTest(
    JNIEnv* env, jobject obj) {
  // Use profile preferences here instead of SyncPrefs to avoid an extra
  // conversion, since SyncPrefs::GetLastSyncedTime() converts the stored value
  // to to base::Time.
  return static_cast<jlong>(
      profile_->GetPrefs()->GetInt64(sync_driver::prefs::kSyncLastSyncedTime));
}

void ProfileSyncServiceAndroid::OverrideNetworkResourcesForTest(
    JNIEnv* env,
    jobject obj,
    jlong network_resources) {
  syncer::NetworkResources* resources =
      reinterpret_cast<syncer::NetworkResources*>(network_resources);
  sync_service_->OverrideNetworkResourcesForTest(
      make_scoped_ptr<syncer::NetworkResources>(resources));
}

// static
ScopedJavaLocalRef<jintArray>
ProfileSyncServiceAndroid::ModelTypeSetToJavaIntArray(
    JNIEnv* env,
    syncer::ModelTypeSet types) {
  std::vector<int> type_vector;
  for (syncer::ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
    type_vector.push_back(it.Get());
  }
  return base::android::ToJavaIntArray(env, type_vector);
}

// static
ProfileSyncServiceAndroid*
    ProfileSyncServiceAndroid::GetProfileSyncServiceAndroid() {
  return reinterpret_cast<ProfileSyncServiceAndroid*>(
          Java_ProfileSyncService_getProfileSyncServiceAndroid(
      AttachCurrentThread(), base::android::GetApplicationContext()));
}

static jlong Init(JNIEnv* env, jobject obj) {
  ProfileSyncServiceAndroid* profile_sync_service_android =
      new ProfileSyncServiceAndroid(env, obj);
  profile_sync_service_android->Init();
  return reinterpret_cast<intptr_t>(profile_sync_service_android);
}

// static
bool ProfileSyncServiceAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
