// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system_util.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/write_on_cache_file.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/profiles/profile_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chromeos/chromeos_constants.h"
#include "components/drive/drive.pb.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/file_system_core_util.h"
#include "components/drive/job_list.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "storage/browser/fileapi/file_system_url.h"

using content::BrowserThread;

namespace drive {
namespace util {

namespace {

// Returns DriveIntegrationService instance, if Drive is enabled.
// Otherwise, NULL.
DriveIntegrationService* GetIntegrationServiceByProfile(Profile* profile) {
  DriveIntegrationService* service =
      DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!service || !service->IsMounted())
    return NULL;
  return service;
}

}  // namespace

base::FilePath GetDriveMountPointPath(Profile* profile) {
  std::string id = chromeos::ProfileHelper::GetUserIdHashFromProfile(profile);
  if (id.empty() || id == chrome::kLegacyProfileDir) {
    // ProfileHelper::GetUserIdHashFromProfile works only when multi-profile is
    // enabled. In that case, we fall back to use UserManager (it basically just
    // returns currently active users's hash in such a case.) I still try
    // ProfileHelper first because it works better in tests.
    const user_manager::User* const user =
        user_manager::UserManager::IsInitialized()
            ? chromeos::ProfileHelper::Get()->GetUserByProfile(
                  profile->GetOriginalProfile())
            : NULL;
    if (user)
      id = user->username_hash();
  }
  return GetDriveMountPointPathForUserIdHash(id);
}

base::FilePath GetDriveMountPointPathForUserIdHash(
    const std::string user_id_hash) {
  static const base::FilePath::CharType kSpecialMountPointRoot[] =
      FILE_PATH_LITERAL("/special");
  static const char kDriveMountPointNameBase[] = "drive";
  return base::FilePath(kSpecialMountPointRoot)
      .AppendASCII(net::EscapeQueryParamValue(
          kDriveMountPointNameBase +
              (user_id_hash.empty() ? "" : "-" + user_id_hash),
          false));
}

bool IsUnderDriveMountPoint(const base::FilePath& path) {
  return !ExtractDrivePath(path).empty();
}

base::FilePath ExtractDrivePath(const base::FilePath& path) {
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);
  if (components.size() < 3)
    return base::FilePath();
  if (components[0] != FILE_PATH_LITERAL("/"))
    return base::FilePath();
  if (components[1] != FILE_PATH_LITERAL("special"))
    return base::FilePath();
  static const base::FilePath::CharType kPrefix[] = FILE_PATH_LITERAL("drive");
  if (components[2].compare(0, arraysize(kPrefix) - 1, kPrefix) != 0)
    return base::FilePath();

  base::FilePath drive_path = GetDriveGrandRootPath();
  for (size_t i = 3; i < components.size(); ++i)
    drive_path = drive_path.Append(components[i]);
  return drive_path;
}

FileSystemInterface* GetFileSystemByProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DriveIntegrationService* integration_service =
      GetIntegrationServiceByProfile(profile);
  return integration_service ? integration_service->file_system() : NULL;
}

FileSystemInterface* GetFileSystemByProfileId(void* profile_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // |profile_id| needs to be checked with ProfileManager::IsValidProfile
  // before using it.
  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return NULL;
  return GetFileSystemByProfile(profile);
}

DriveAppRegistry* GetDriveAppRegistryByProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DriveIntegrationService* integration_service =
      GetIntegrationServiceByProfile(profile);
  return integration_service ? integration_service->drive_app_registry() : NULL;
}

DriveServiceInterface* GetDriveServiceByProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DriveIntegrationService* integration_service =
      GetIntegrationServiceByProfile(profile);
  return integration_service ? integration_service->drive_service() : NULL;
}

Profile* ExtractProfileFromPath(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const std::vector<Profile*>& profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (size_t i = 0; i < profiles.size(); ++i) {
    Profile* original_profile = profiles[i]->GetOriginalProfile();
    if (original_profile == profiles[i] &&
        !chromeos::ProfileHelper::IsSigninProfile(original_profile)) {
      const base::FilePath base = GetDriveMountPointPath(original_profile);
      if (base == path || base.IsParent(path))
        return original_profile;
    }
  }
  return NULL;
}

base::FilePath ExtractDrivePathFromFileSystemUrl(
    const storage::FileSystemURL& url) {
  if (!url.is_valid() || url.type() != storage::kFileSystemTypeDrive)
    return base::FilePath();
  return ExtractDrivePath(url.path());
}

base::FilePath GetCacheRootPath(Profile* profile) {
  base::FilePath cache_base_path;
  chrome::GetUserCacheDirectory(profile->GetPath(), &cache_base_path);
  base::FilePath cache_root_path =
      cache_base_path.Append(chromeos::kDriveCacheDirname);
  static const base::FilePath::CharType kFileCacheVersionDir[] =
      FILE_PATH_LITERAL("v1");
  return cache_root_path.Append(kFileCacheVersionDir);
}

void PrepareWritableFileAndRun(Profile* profile,
                               const base::FilePath& path,
                               const PrepareWritableFileCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  FileSystemInterface* file_system = GetFileSystemByProfile(profile);
  if (!file_system || !IsUnderDriveMountPoint(path)) {
    content::BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE, base::Bind(callback, FILE_ERROR_FAILED, base::FilePath()));
    return;
  }

  WriteOnCacheFile(file_system, ExtractDrivePath(path),
                   std::string(),  // mime_type
                   callback);
}

void EnsureDirectoryExists(Profile* profile,
                           const base::FilePath& directory,
                           const FileOperationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  if (IsUnderDriveMountPoint(directory)) {
    FileSystemInterface* file_system = GetFileSystemByProfile(profile);
    DCHECK(file_system);
    file_system->CreateDirectory(ExtractDrivePath(directory),
                                 true /* is_exclusive */,
                                 true /* is_recursive */, callback);
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, FILE_ERROR_OK));
  }
}

bool IsDriveEnabledForProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!chromeos::IsProfileAssociatedWithGaiaAccount(profile))
    return false;

  // Disable Drive if preference is set. This can happen with commandline flag
  // --disable-drive or enterprise policy, or with user settings.
  if (profile->GetPrefs()->GetBoolean(prefs::kDisableDrive))
    return false;

  return true;
}

ConnectionStatusType GetDriveConnectionStatus(Profile* profile) {
  drive::DriveServiceInterface* const drive_service =
      drive::util::GetDriveServiceByProfile(profile);

  if (!drive_service)
    return DRIVE_DISCONNECTED_NOSERVICE;
  if (net::NetworkChangeNotifier::IsOffline())
    return DRIVE_DISCONNECTED_NONETWORK;
  if (!drive_service->CanSendRequest())
    return DRIVE_DISCONNECTED_NOTREADY;

  const bool is_connection_cellular =
      net::NetworkChangeNotifier::IsConnectionCellular(
          net::NetworkChangeNotifier::GetConnectionType());
  const bool disable_sync_over_celluar =
      profile->GetPrefs()->GetBoolean(prefs::kDisableDriveOverCellular);

  if (is_connection_cellular && disable_sync_over_celluar)
    return DRIVE_CONNECTED_METERED;
  return DRIVE_CONNECTED;
}

}  // namespace util
}  // namespace drive
