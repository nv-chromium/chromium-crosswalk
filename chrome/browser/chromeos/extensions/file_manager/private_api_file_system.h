// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides file system related API functions.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_

#include <string>

#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "components/drive/file_errors.h"
#include "device/media_transfer_protocol/mtp_storage_info.pb.h"
#include "extensions/browser/extension_function.h"
#include "storage/browser/fileapi/file_system_url.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace storage {
class FileSystemContext;
class FileSystemURL;
}  // namespace storage

namespace file_manager {
namespace util {
struct EntryDefinition;
typedef std::vector<EntryDefinition> EntryDefinitionList;
}  // namespace util
}  // namespace file_manager

namespace drive {
namespace util {
class FileStreamMd5Digester;
}  // namespace util
struct HashAndFilePath;
}  // namespace drive

namespace extensions {

// Grant permission to request externalfile scheme. The permission is needed to
// start drag for external file URL.
class FileManagerPrivateEnableExternalFileSchemeFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.enableExternalFileScheme",
                             FILEMANAGERPRIVATE_ENABLEEXTERNALFILESCHEME);

 protected:
  ~FileManagerPrivateEnableExternalFileSchemeFunction() override {}

 private:
  ExtensionFunction::ResponseAction Run() override;
};

// Grants R/W permissions to profile-specific directories (Drive, Downloads)
// from other profiles.
class FileManagerPrivateGrantAccessFunction : public UIThreadExtensionFunction {
 public:
  FileManagerPrivateGrantAccessFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.grantAccess",
                             FILEMANAGERPRIVATE_GRANTACCESS)

 protected:
  ~FileManagerPrivateGrantAccessFunction() override {}

 private:
  ExtensionFunction::ResponseAction Run() override;
  const ChromeExtensionFunctionDetails chrome_details_;
  DISALLOW_COPY_AND_ASSIGN(FileManagerPrivateGrantAccessFunction);
};

// Base class for FileManagerPrivateInternalAddFileWatchFunction and
// FileManagerPrivateInternalRemoveFileWatchFunction. Although it's called
// "FileWatch",
// the class and its sub classes are used only for watching changes in
// directories.
class FileWatchFunctionBase : public LoggedAsyncExtensionFunction {
 public:
  // Calls SendResponse() with |success| converted to base::Value.
  void Respond(bool success);

 protected:
  ~FileWatchFunctionBase() override {}

  // Performs a file watch operation (ex. adds or removes a file watch).
  virtual void PerformFileWatchOperation(
      scoped_refptr<storage::FileSystemContext> file_system_context,
      const storage::FileSystemURL& file_system_url,
      const std::string& extension_id) = 0;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;
};

// Implements the chrome.fileManagerPrivate.addFileWatch method.
// Starts watching changes in directories.
class FileManagerPrivateInternalAddFileWatchFunction
    : public FileWatchFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.addFileWatch",
                             FILEMANAGERPRIVATEINTERNAL_ADDFILEWATCH)

 protected:
  ~FileManagerPrivateInternalAddFileWatchFunction() override {}

  // FileWatchFunctionBase override.
  void PerformFileWatchOperation(
      scoped_refptr<storage::FileSystemContext> file_system_context,
      const storage::FileSystemURL& file_system_url,
      const std::string& extension_id) override;
};


// Implements the chrome.fileManagerPrivate.removeFileWatch method.
// Stops watching changes in directories.
class FileManagerPrivateInternalRemoveFileWatchFunction
    : public FileWatchFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.removeFileWatch",
                             FILEMANAGERPRIVATEINTERNAL_REMOVEFILEWATCH)

 protected:
  ~FileManagerPrivateInternalRemoveFileWatchFunction() override {}

  // FileWatchFunctionBase override.
  void PerformFileWatchOperation(
      scoped_refptr<storage::FileSystemContext> file_system_context,
      const storage::FileSystemURL& file_system_url,
      const std::string& extension_id) override;
};

// Implements the chrome.fileManagerPrivate.getSizeStats method.
class FileManagerPrivateGetSizeStatsFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getSizeStats",
                             FILEMANAGERPRIVATE_GETSIZESTATS)

 protected:
  ~FileManagerPrivateGetSizeStatsFunction() override {}

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void GetDriveAvailableSpaceCallback(drive::FileError error,
                                      int64 bytes_total,
                                      int64 bytes_used);

  void GetMtpAvailableSpaceCallback(const MtpStorageInfo& mtp_storage_info,
                                    const bool error);

  void GetSizeStatsCallback(const uint64* total_size,
                            const uint64* remaining_size);
};

// Implements the chrome.fileManagerPrivate.validatePathNameLength method.
class FileManagerPrivateInternalValidatePathNameLengthFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivateInternal.validatePathNameLength",
      FILEMANAGERPRIVATEINTERNAL_VALIDATEPATHNAMELENGTH)

 protected:
  ~FileManagerPrivateInternalValidatePathNameLengthFunction() override {}

  void OnFilePathLimitRetrieved(size_t current_length, size_t max_length);

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;
};

// Implements the chrome.fileManagerPrivate.formatVolume method.
// Formats Volume given its mount path.
class FileManagerPrivateFormatVolumeFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.formatVolume",
                             FILEMANAGERPRIVATE_FORMATVOLUME)

 protected:
  ~FileManagerPrivateFormatVolumeFunction() override {}

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;
};

// Implements the chrome.fileManagerPrivate.startCopy method.
class FileManagerPrivateInternalStartCopyFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.startCopy",
                             FILEMANAGERPRIVATEINTERNAL_STARTCOPY)

 protected:
  ~FileManagerPrivateInternalStartCopyFunction() override {}

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void RunAfterGetFileMetadata(base::File::Error result,
                               const base::File::Info& file_info);

  // Part of RunAsync(). Called after FreeDiskSpaceIfNeededFor() is completed on
  // IO thread.
  void RunAfterFreeDiskSpace(bool available);

  // Part of RunAsync(). Called after Copy() is started on IO thread.
  void RunAfterStartCopy(int operation_id);

  storage::FileSystemURL source_url_;
  storage::FileSystemURL destination_url_;
};

// Implements the chrome.fileManagerPrivate.cancelCopy method.
class FileManagerPrivateCancelCopyFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.cancelCopy",
                             FILEMANAGERPRIVATE_CANCELCOPY)

 protected:
  ~FileManagerPrivateCancelCopyFunction() override {}

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;
};

// Implements the chrome.fileManagerPrivateInternal.resolveIsolatedEntries
// method.
class FileManagerPrivateInternalResolveIsolatedEntriesFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivateInternal.resolveIsolatedEntries",
      FILEMANAGERPRIVATE_RESOLVEISOLATEDENTRIES)

 protected:
  ~FileManagerPrivateInternalResolveIsolatedEntriesFunction() override {}

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void RunAsyncAfterConvertFileDefinitionListToEntryDefinitionList(scoped_ptr<
      file_manager::util::EntryDefinitionList> entry_definition_list);
};

class FileManagerPrivateInternalComputeChecksumFunction
    : public LoggedAsyncExtensionFunction {
 public:
  FileManagerPrivateInternalComputeChecksumFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.computeChecksum",
                             FILEMANAGERPRIVATEINTERNAL_COMPUTECHECKSUM)

 protected:
  ~FileManagerPrivateInternalComputeChecksumFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  scoped_ptr<drive::util::FileStreamMd5Digester> digester_;

  void Respond(const std::string& hash);
};

// Implements the chrome.fileManagerPrivate.searchFilesByHashes method.
class FileManagerPrivateSearchFilesByHashesFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.searchFilesByHashes",
                             FILEMANAGERPRIVATE_SEARCHFILESBYHASHES)

 protected:
  ~FileManagerPrivateSearchFilesByHashesFunction() override {}

 private:
  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

  // Sends a response with |results| to the extension.
  void OnSearchByHashes(const std::set<std::string>& hashes,
                        drive::FileError error,
                        const std::vector<drive::HashAndFilePath>& results);
};

// Implements the chrome.fileManagerPrivate.isUMAEnabled method.
class FileManagerPrivateIsUMAEnabledFunction
    : public UIThreadExtensionFunction {
 public:
  FileManagerPrivateIsUMAEnabledFunction() {}
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.isUMAEnabled",
                             FILEMANAGERPRIVATE_ISUMAENABLED)
 protected:
  ~FileManagerPrivateIsUMAEnabledFunction() override {}

 private:
  ExtensionFunction::ResponseAction Run() override;
  DISALLOW_COPY_AND_ASSIGN(FileManagerPrivateIsUMAEnabledFunction);
};

// Implements the chrome.fileManagerPrivate.setEntryTag method.
class FileManagerPrivateInternalSetEntryTagFunction
    : public UIThreadExtensionFunction {
 public:
  FileManagerPrivateInternalSetEntryTagFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.setEntryTag",
                             FILEMANAGERPRIVATEINTERNAL_SETENTRYTAG)
 protected:
  ~FileManagerPrivateInternalSetEntryTagFunction() override {}

 private:
  const ChromeExtensionFunctionDetails chrome_details_;

  // Called when setting a tag is completed with either a success or an error.
  void OnSetEntryPropertyCompleted(drive::FileError result);

  ExtensionFunction::ResponseAction Run() override;
  DISALLOW_COPY_AND_ASSIGN(FileManagerPrivateInternalSetEntryTagFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_
