// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_database_helper.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "storage/common/database/database_identifier.h"

using content::BrowserContext;
using content::BrowserThread;
using storage::DatabaseIdentifier;

BrowsingDataDatabaseHelper::DatabaseInfo::DatabaseInfo(
    const DatabaseIdentifier& identifier,
    const std::string& database_name,
    const std::string& description,
    int64 size,
    base::Time last_modified)
    : identifier(identifier),
      database_name(database_name),
      description(description),
      size(size),
      last_modified(last_modified) {
}

BrowsingDataDatabaseHelper::DatabaseInfo::~DatabaseInfo() {}

BrowsingDataDatabaseHelper::BrowsingDataDatabaseHelper(Profile* profile)
    : tracker_(BrowserContext::GetDefaultStoragePartition(profile)
                   ->GetDatabaseTracker()) {}

BrowsingDataDatabaseHelper::~BrowsingDataDatabaseHelper() {
}

void BrowsingDataDatabaseHelper::StartFetching(
    const base::Callback<void(const std::list<DatabaseInfo>&)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_fetching_);
  DCHECK(!callback.is_null());

  is_fetching_ = true;
  database_info_.clear();
  completion_callback_ = callback;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&BrowsingDataDatabaseHelper::FetchDatabaseInfoOnFileThread,
                 this));
}

void BrowsingDataDatabaseHelper::DeleteDatabase(const std::string& origin,
                                                const std::string& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&BrowsingDataDatabaseHelper::DeleteDatabaseOnFileThread, this,
                 origin, name));
}

void BrowsingDataDatabaseHelper::FetchDatabaseInfoOnFileThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  std::vector<storage::OriginInfo> origins_info;
  if (tracker_.get() && tracker_->GetAllOriginsInfo(&origins_info)) {
    for (const storage::OriginInfo& ori : origins_info) {
      DatabaseIdentifier identifier =
          DatabaseIdentifier::Parse(ori.GetOriginIdentifier());
      if (!BrowsingDataHelper::HasWebScheme(identifier.ToOrigin())) {
        // Non-websafe state is not considered browsing data.
        continue;
      }
      std::vector<base::string16> databases;
      ori.GetAllDatabaseNames(&databases);
      for (const base::string16& db : databases) {
        base::FilePath file_path =
            tracker_->GetFullDBFilePath(ori.GetOriginIdentifier(), db);
        base::File::Info file_info;
        if (base::GetFileInfo(file_path, &file_info)) {
          database_info_.push_back(
              DatabaseInfo(identifier, base::UTF16ToUTF8(db),
                           base::UTF16ToUTF8(ori.GetDatabaseDescription(db)),
                           file_info.size, file_info.last_modified));
        }
      }
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataDatabaseHelper::NotifyInUIThread, this));
}

void BrowsingDataDatabaseHelper::NotifyInUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(is_fetching_);
  completion_callback_.Run(database_info_);
  completion_callback_.Reset();
  is_fetching_ = false;
  database_info_.clear();
}

void BrowsingDataDatabaseHelper::DeleteDatabaseOnFileThread(
    const std::string& origin,
    const std::string& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (!tracker_.get())
    return;
  tracker_->DeleteDatabase(origin, base::UTF8ToUTF16(name),
                           net::CompletionCallback());
}

CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::PendingDatabaseInfo(
    const GURL& origin,
    const std::string& name,
    const std::string& description)
    : origin(origin),
      name(name),
      description(description) {
}

CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::~PendingDatabaseInfo() {}

bool CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::operator<(
    const PendingDatabaseInfo& other) const {
  if (origin == other.origin)
    return name < other.name;
  return origin < other.origin;
}

CannedBrowsingDataDatabaseHelper::CannedBrowsingDataDatabaseHelper(
    Profile* profile)
    : BrowsingDataDatabaseHelper(profile) {
}

void CannedBrowsingDataDatabaseHelper::AddDatabase(
    const GURL& origin,
    const std::string& name,
    const std::string& description) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BrowsingDataHelper::HasWebScheme(origin)) {
    pending_database_info_.insert(PendingDatabaseInfo(
          origin, name, description));
  }
}

void CannedBrowsingDataDatabaseHelper::Reset() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  pending_database_info_.clear();
}

bool CannedBrowsingDataDatabaseHelper::empty() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pending_database_info_.empty();
}

size_t CannedBrowsingDataDatabaseHelper::GetDatabaseCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pending_database_info_.size();
}

const std::set<CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo>&
CannedBrowsingDataDatabaseHelper::GetPendingDatabaseInfo() {
  return pending_database_info_;
}

void CannedBrowsingDataDatabaseHelper::StartFetching(
    const base::Callback<void(const std::list<DatabaseInfo>&)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<DatabaseInfo> result;
  for (const PendingDatabaseInfo& info : pending_database_info_) {
    DatabaseIdentifier identifier =
        DatabaseIdentifier::CreateFromOrigin(info.origin);

    result.push_back(
        DatabaseInfo(identifier, info.name, info.description, 0, base::Time()));
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, result));
}

void CannedBrowsingDataDatabaseHelper::DeleteDatabase(
    const std::string& origin_identifier,
    const std::string& name) {
  GURL origin =
      storage::DatabaseIdentifier::Parse(origin_identifier).ToOrigin();
  for (std::set<PendingDatabaseInfo>::iterator it =
           pending_database_info_.begin();
       it != pending_database_info_.end();
       ++it) {
    if (it->origin == origin && it->name == name) {
      pending_database_info_.erase(it);
      break;
    }
  }
  BrowsingDataDatabaseHelper::DeleteDatabase(origin_identifier, name);
}

CannedBrowsingDataDatabaseHelper::~CannedBrowsingDataDatabaseHelper() {}
