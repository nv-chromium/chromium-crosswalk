// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/one_shot_event.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace extensions {

// A simplified version of ExtensionSystem for app_shell. Allows
// app_shell to skip initialization of services it doesn't need.
class ShellExtensionSystem : public ExtensionSystem {
 public:
  explicit ShellExtensionSystem(content::BrowserContext* browser_context);
  ~ShellExtensionSystem() override;

  // Loads an unpacked application from a directory. Returns the extension on
  // success, or null otherwise.
  const Extension* LoadApp(const base::FilePath& app_dir);

  // Initializes the extension system.
  void Init();

  // Launch the app with id |extension_id|.
  void LaunchApp(const std::string& extension_id);

  // KeyedService implementation:
  void Shutdown() override;

  // ExtensionSystem implementation:
  void InitForRegularProfile(bool extensions_enabled) override;
  ExtensionService* extension_service() override;
  RuntimeData* runtime_data() override;
  ManagementPolicy* management_policy() override;
  SharedUserScriptMaster* shared_user_script_master() override;
  StateStore* state_store() override;
  StateStore* rules_store() override;
  InfoMap* info_map() override;
  QuotaService* quota_service() override;
  AppSorting* app_sorting() override;
  void RegisterExtensionWithRequestContexts(
      const Extension* extension,
      const base::Closure& callback) override;
  void UnregisterExtensionWithRequestContexts(
      const std::string& extension_id,
      const UnloadedExtensionInfo::Reason reason) override;
  const OneShotEvent& ready() const override;
  ContentVerifier* content_verifier() override;
  scoped_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) override;

 private:
  void OnExtensionRegisteredWithRequestContexts(
      scoped_refptr<Extension> extension);
  content::BrowserContext* browser_context_;  // Not owned.

  // Data to be accessed on the IO thread. Must outlive process_manager_.
  scoped_refptr<InfoMap> info_map_;

  scoped_ptr<RuntimeData> runtime_data_;
  scoped_ptr<QuotaService> quota_service_;
  scoped_ptr<AppSorting> app_sorting_;

  // Signaled when the extension system has completed its startup tasks.
  OneShotEvent ready_;

  base::WeakPtrFactory<ShellExtensionSystem> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionSystem);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_H_
