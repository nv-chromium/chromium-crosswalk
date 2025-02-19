// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "extensions/common/permissions/usb_device_permission_data.h"
#include "extensions/common/test_util.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

// Tests that ChromePermissionMessageProvider provides not only correct, but
// meaningful permission messages that coalesce correctly where appropriate.
// There are 3 types of permission messages that need to be tested:
//  1. The combined list of active permissions, displayed at install time (or
//     when the app has been disabled automatically and needs to be re-enabled)
//  2. The split list of active permissions, displayed in the App Info dialog,
//     where the optional permissions are individually revokable
//  3. The list of requested optional permissions, displayed in a prompt to the
//     user when the app requests these during runtime
// Some of these tests are prefixed AntiTest_, since they demonstrate existing
// problematic functionality. These tests are prefixed with AntiTest_ and will
// be changed as the correct behaviour is implemented. TODOs in the test explain
// the currently problematic behaviour.
class PermissionMessagesUnittest : public testing::Test {
 public:
  PermissionMessagesUnittest()
      : message_provider_(new ChromePermissionMessageProvider()) {}
  ~PermissionMessagesUnittest() override {}

 protected:
  void CreateAndInstallAppWithPermissions(ListBuilder& required_permissions,
                                          ListBuilder& optional_permissions) {
    app_ = test_util::BuildApp(ExtensionBuilder().Pass())
               .MergeManifest(
                    DictionaryBuilder()
                        .Set("permissions", required_permissions)
                        .Set("optional_permissions", optional_permissions))
               .SetID(crx_file::id_util::GenerateId("app"))
               .SetLocation(Manifest::INTERNAL)
               .Build();
    env_.GetExtensionService()->AddExtension(app_.get());
  }

  void CreateAndInstallExtensionWithPermissions(
      ListBuilder& required_permissions,
      ListBuilder& optional_permissions) {
    app_ = test_util::BuildExtension(ExtensionBuilder().Pass())
               .MergeManifest(
                    DictionaryBuilder()
                        .Set("permissions", required_permissions)
                        .Set("optional_permissions", optional_permissions))
               .SetID(crx_file::id_util::GenerateId("extension"))
               .SetLocation(Manifest::INTERNAL)
               .Build();
    env_.GetExtensionService()->AddExtension(app_.get());
  }

  // Returns the permission messages that would display in the prompt that
  // requests all the optional permissions for the current |app_|.
  std::vector<base::string16> GetOptionalPermissionMessages() {
    scoped_refptr<const PermissionSet> granted_permissions =
        env_.GetExtensionPrefs()->GetGrantedPermissions(app_->id());
    scoped_refptr<const PermissionSet> optional_permissions =
        PermissionsParser::GetOptionalPermissions(app_.get());
    scoped_refptr<const PermissionSet> requested_permissions =
        PermissionSet::CreateDifference(optional_permissions.get(),
                                        granted_permissions.get());
    return GetMessages(requested_permissions);
  }

  void GrantOptionalPermissions() {
    PermissionsUpdater perms_updater(env_.profile());
    perms_updater.AddPermissions(
        app_.get(),
        PermissionsParser::GetOptionalPermissions(app_.get()).get());
  }

  std::vector<base::string16> active_permissions() {
    return GetMessages(app_->permissions_data()->active_permissions());
  }

  std::vector<base::string16> required_permissions() {
    return GetMessages(PermissionsParser::GetRequiredPermissions(app_.get()));
  }

  std::vector<base::string16> optional_permissions() {
    return GetMessages(PermissionsParser::GetOptionalPermissions(app_.get()));
  }

 private:
  std::vector<base::string16> GetMessages(
      scoped_refptr<const PermissionSet> permissions) {
    std::vector<base::string16> messages;
    for (const CoalescedPermissionMessage& msg :
         message_provider_->GetPermissionMessages(
             message_provider_->GetAllPermissionIDs(permissions.get(),
                                                    app_->GetType()))) {
      messages.push_back(msg.message());
    }
    return messages;
  }

  extensions::TestExtensionEnvironment env_;
  scoped_ptr<ChromePermissionMessageProvider> message_provider_;
  scoped_refptr<const Extension> app_;

  DISALLOW_COPY_AND_ASSIGN(PermissionMessagesUnittest);
};

// If an app has both the 'history' and 'tabs' permission, one should hide the
// other (the 'history' permission has superset permissions).
TEST_F(PermissionMessagesUnittest, HistoryHidesTabsMessage) {
  CreateAndInstallExtensionWithPermissions(
      ListBuilder().Append("tabs").Append("history").Pass(),
      ListBuilder().Pass());

  ASSERT_EQ(1U, required_permissions().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE),
      required_permissions()[0]);

  ASSERT_EQ(0U, optional_permissions().size());
}

// If an app requests the 'history' permission, but already has the 'tabs'
// permission, only the new coalesced message is displayed.
TEST_F(PermissionMessagesUnittest, MixedPermissionMessagesCoalesceOnceGranted) {
  CreateAndInstallExtensionWithPermissions(
      ListBuilder().Append("tabs").Pass(),
      ListBuilder().Append("history").Pass());

  ASSERT_EQ(1U, required_permissions().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ),
      required_permissions()[0]);

  ASSERT_EQ(1U, optional_permissions().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE),
      optional_permissions()[0]);

  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ),
      active_permissions()[0]);

  ASSERT_EQ(1U, GetOptionalPermissionMessages().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE),
      GetOptionalPermissionMessages()[0]);

  GrantOptionalPermissions();

  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE),
      active_permissions()[0]);
}

// AntiTest: This behavior should be changed and improved.
// If an app requests the 'tabs' permission but already has the 'history'
// permission, a prompt is displayed. However, no prompt should appear at all,
// since 'tabs' is a subset of 'history' and the final list of permissions are
// not affected by this grant.
TEST_F(PermissionMessagesUnittest,
       AntiTest_PromptCanRequestSubsetOfAlreadyGrantedPermissions) {
  CreateAndInstallExtensionWithPermissions(
      ListBuilder().Append("history").Pass(),
      ListBuilder().Append("tabs").Pass());

  ASSERT_EQ(1U, required_permissions().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE),
      required_permissions()[0]);

  ASSERT_EQ(1U, optional_permissions().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ),
      optional_permissions()[0]);

  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE),
      active_permissions()[0]);

  // TODO(sashab): This prompt should display no permissions, since READ is a
  // subset permission of WRITE.
  ASSERT_EQ(1U, GetOptionalPermissionMessages().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ),
      GetOptionalPermissionMessages()[0]);

  GrantOptionalPermissions();

  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE),
      active_permissions()[0]);
}

// AntiTest: This behavior should be changed and improved.
// If an app requests the 'sessions' permission, nothing is displayed in the
// permission request prompt. However, the required permissions for the app are
// actually modified, so the prompt *should* display a message to prevent this
// permission from being granted for free.
TEST_F(PermissionMessagesUnittest,
       AntiTest_PromptCanBeEmptyButCausesChangeInPermissions) {
  CreateAndInstallExtensionWithPermissions(
      ListBuilder().Append("tabs").Pass(),
      ListBuilder().Append("sessions").Pass());

  ASSERT_EQ(1U, required_permissions().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ),
      required_permissions()[0]);

  ASSERT_EQ(0U, optional_permissions().size());

  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ),
      active_permissions()[0]);

  // TODO(sashab): This prompt should display the sessions permission message,
  // as well as warn the user that it can affect the existing 'tab' permission.
  ASSERT_EQ(0U, GetOptionalPermissionMessages().size());

  GrantOptionalPermissions();

  ASSERT_EQ(1U, active_permissions().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ_AND_SESSIONS),
            active_permissions()[0]);
}

class USBDevicePermissionMessagesTest : public testing::Test {
 public:
  USBDevicePermissionMessagesTest()
      : message_provider_(new ChromePermissionMessageProvider()) {}
  ~USBDevicePermissionMessagesTest() override {}

  CoalescedPermissionMessages GetMessages(const PermissionIDSet& permissions) {
    return message_provider_->GetPermissionMessages(permissions);
  }

 private:
  scoped_ptr<ChromePermissionMessageProvider> message_provider_;
};

TEST_F(USBDevicePermissionMessagesTest, SingleDevice) {
  {
    const char kMessage[] =
        "Access any PVR Mass Storage from HUMAX Co., Ltd. via USB";

    scoped_ptr<base::ListValue> permission_list(new base::ListValue());
    permission_list->Append(
        UsbDevicePermissionData(0x02ad, 0x138c, -1).ToValue().release());

    UsbDevicePermission permission(
        PermissionsInfo::GetInstance()->GetByID(APIPermission::kUsbDevice));
    ASSERT_TRUE(permission.FromValue(permission_list.get(), NULL, NULL));

    CoalescedPermissionMessages messages =
        GetMessages(permission.GetPermissions());
    ASSERT_EQ(1U, messages.size());
    EXPECT_EQ(base::ASCIIToUTF16(kMessage), messages.front().message());
  }
  {
    const char kMessage[] = "Access USB devices from HUMAX Co., Ltd.";

    scoped_ptr<base::ListValue> permission_list(new base::ListValue());
    permission_list->Append(
        UsbDevicePermissionData(0x02ad, 0x138d, -1).ToValue().release());

    UsbDevicePermission permission(
        PermissionsInfo::GetInstance()->GetByID(APIPermission::kUsbDevice));
    ASSERT_TRUE(permission.FromValue(permission_list.get(), NULL, NULL));

    CoalescedPermissionMessages messages =
        GetMessages(permission.GetPermissions());
    ASSERT_EQ(1U, messages.size());
    EXPECT_EQ(base::ASCIIToUTF16(kMessage), messages.front().message());
  }
  {
    const char kMessage[] = "Access USB devices from an unknown vendor";

    scoped_ptr<base::ListValue> permission_list(new base::ListValue());
    permission_list->Append(
        UsbDevicePermissionData(0x02ae, 0x138d, -1).ToValue().release());

    UsbDevicePermission permission(
        PermissionsInfo::GetInstance()->GetByID(APIPermission::kUsbDevice));
    ASSERT_TRUE(permission.FromValue(permission_list.get(), NULL, NULL));

    CoalescedPermissionMessages messages =
        GetMessages(permission.GetPermissions());
    ASSERT_EQ(1U, messages.size());
    EXPECT_EQ(base::ASCIIToUTF16(kMessage), messages.front().message());
  }
}

TEST_F(USBDevicePermissionMessagesTest, MultipleDevice) {
  const char kMessage[] = "Access any of these USB devices";
  const char* kDetails[] = {
      "PVR Mass Storage from HUMAX Co., Ltd.",
      "unknown devices from HUMAX Co., Ltd.",
      "devices from an unknown vendor"
  };

  // Prepare data set
  scoped_ptr<base::ListValue> permission_list(new base::ListValue());
  permission_list->Append(
      UsbDevicePermissionData(0x02ad, 0x138c, -1).ToValue().release());
  // This device's product ID is not in Chrome's database.
  permission_list->Append(
      UsbDevicePermissionData(0x02ad, 0x138d, -1).ToValue().release());
  // This additional unknown product will be collapsed into the entry above.
  permission_list->Append(
      UsbDevicePermissionData(0x02ad, 0x138e, -1).ToValue().release());
  // This device's vendor ID is not in Chrome's database.
  permission_list->Append(
      UsbDevicePermissionData(0x02ae, 0x138d, -1).ToValue().release());
  // This additional unknown vendor will be collapsed into the entry above.
  permission_list->Append(
      UsbDevicePermissionData(0x02af, 0x138d, -1).ToValue().release());

  UsbDevicePermission permission(
      PermissionsInfo::GetInstance()->GetByID(APIPermission::kUsbDevice));
  ASSERT_TRUE(permission.FromValue(permission_list.get(), NULL, NULL));

  CoalescedPermissionMessages messages =
      GetMessages(permission.GetPermissions());
  ASSERT_EQ(1U, messages.size());
  EXPECT_EQ(base::ASCIIToUTF16(kMessage), messages.front().message());
  const std::vector<base::string16>& submessages =
      messages.front().submessages();
  ASSERT_EQ(arraysize(kDetails), submessages.size());
  for (size_t i = 0; i < submessages.size(); i++)
    EXPECT_EQ(base::ASCIIToUTF16(kDetails[i]), submessages[i]);
}

}  // namespace extensions
