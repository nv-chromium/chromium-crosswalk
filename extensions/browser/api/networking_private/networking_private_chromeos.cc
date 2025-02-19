// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_chromeos.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_activation_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/onc/onc_constants.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/networking_private/networking_private_api.h"
#include "extensions/browser/extensions_browser_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using chromeos::DeviceState;
using chromeos::NetworkHandler;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;
using chromeos::ShillManagerClient;
using extensions::NetworkingPrivateDelegate;

namespace private_api = extensions::api::networking_private;

namespace {

chromeos::NetworkStateHandler* GetStateHandler() {
  return NetworkHandler::Get()->network_state_handler();
}

chromeos::ManagedNetworkConfigurationHandler* GetManagedConfigurationHandler() {
  return NetworkHandler::Get()->managed_network_configuration_handler();
}

bool GetServicePathFromGuid(const std::string& guid,
                            std::string* service_path,
                            std::string* error) {
  const chromeos::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network) {
    *error = extensions::networking_private::kErrorInvalidNetworkGuid;
    return false;
  }
  *service_path = network->path();
  return true;
}

bool GetUserIdHash(content::BrowserContext* browser_context,
                   std::string* user_hash,
                   std::string* error) {
  std::string context_user_hash =
      extensions::ExtensionsBrowserClient::Get()->GetUserIdHashFromContext(
          browser_context);

  // Currently Chrome OS only configures networks for the primary user.
  // Configuration attempts from other browser contexts should fail.
  if (context_user_hash != chromeos::LoginState::Get()->primary_user_hash()) {
    // Disallow class requiring a user id hash from a non-primary user context
    // to avoid complexities with the policy code.
    LOG(ERROR) << "networkingPrivate API call from non primary user: "
               << context_user_hash;
    *error = "Error.NonPrimaryUser";
    return false;
  }
  *user_hash = context_user_hash;
  return true;
}

void AppendDeviceState(
    const std::string& type,
    const DeviceState* device,
    NetworkingPrivateDelegate::DeviceStateList* device_state_list) {
  DCHECK(!type.empty());
  NetworkTypePattern pattern =
      chromeos::onc::NetworkTypePatternFromOncType(type);
  NetworkStateHandler::TechnologyState technology_state =
      GetStateHandler()->GetTechnologyState(pattern);
  private_api::DeviceStateType state = private_api::DEVICE_STATE_TYPE_NONE;
  switch (technology_state) {
    case NetworkStateHandler::TECHNOLOGY_UNAVAILABLE:
      if (!device)
        return;
      // If we have a DeviceState entry but the technology is not available,
      // assume the technology is not initialized.
      state = private_api::DEVICE_STATE_TYPE_UNINITIALIZED;
      break;
    case NetworkStateHandler::TECHNOLOGY_AVAILABLE:
      state = private_api::DEVICE_STATE_TYPE_DISABLED;
      break;
    case NetworkStateHandler::TECHNOLOGY_UNINITIALIZED:
      state = private_api::DEVICE_STATE_TYPE_UNINITIALIZED;
      break;
    case NetworkStateHandler::TECHNOLOGY_ENABLING:
      state = private_api::DEVICE_STATE_TYPE_ENABLING;
      break;
    case NetworkStateHandler::TECHNOLOGY_ENABLED:
      state = private_api::DEVICE_STATE_TYPE_ENABLED;
      break;
  }
  DCHECK_NE(private_api::DEVICE_STATE_TYPE_NONE, state);
  scoped_ptr<private_api::DeviceStateProperties> properties(
      new private_api::DeviceStateProperties);
  properties->type = private_api::ParseNetworkType(type);
  properties->state = state;
  if (device && state == private_api::DEVICE_STATE_TYPE_ENABLED)
    properties->scanning.reset(new bool(device->scanning()));
  device_state_list->push_back(properties.Pass());
}

void NetworkHandlerDictionaryCallback(
    const NetworkingPrivateDelegate::DictionaryCallback& callback,
    const std::string& service_path,
    const base::DictionaryValue& dictionary) {
  scoped_ptr<base::DictionaryValue> dictionary_copy(dictionary.DeepCopy());
  callback.Run(dictionary_copy.Pass());
}

void NetworkHandlerFailureCallback(
    const NetworkingPrivateDelegate::FailureCallback& callback,
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  callback.Run(error_name);
}

void RequirePinSuccess(
    const std::string& device_path,
    const std::string& current_pin,
    const std::string& new_pin,
    const extensions::NetworkingPrivateChromeOS::VoidCallback& success_callback,
    const extensions::NetworkingPrivateChromeOS::FailureCallback&
        failure_callback) {
  // After RequirePin succeeds, call ChangePIN iff a different new_pin is
  // provided.
  if (new_pin.empty() || new_pin == current_pin) {
    success_callback.Run();
    return;
  }
  NetworkHandler::Get()->network_device_handler()->ChangePin(
      device_path, current_pin, new_pin, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

namespace extensions {

NetworkingPrivateChromeOS::NetworkingPrivateChromeOS(
    content::BrowserContext* browser_context,
    scoped_ptr<VerifyDelegate> verify_delegate)
    : NetworkingPrivateDelegate(verify_delegate.Pass()),
      browser_context_(browser_context),
      weak_ptr_factory_(this) {}

NetworkingPrivateChromeOS::~NetworkingPrivateChromeOS() {
}

void NetworkingPrivateChromeOS::GetProperties(
    const std::string& guid,
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  std::string user_id_hash;
  if (!GetUserIdHash(browser_context_, &user_id_hash, &error)) {
    failure_callback.Run(error);
    return;
  }

  GetManagedConfigurationHandler()->GetProperties(
      user_id_hash, service_path,
      base::Bind(&NetworkHandlerDictionaryCallback, success_callback),
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::GetManagedProperties(
    const std::string& guid,
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  std::string user_id_hash;
  if (!GetUserIdHash(browser_context_, &user_id_hash, &error)) {
    failure_callback.Run(error);
    return;
  }

  GetManagedConfigurationHandler()->GetManagedProperties(
      user_id_hash, service_path,
      base::Bind(&NetworkHandlerDictionaryCallback, success_callback),
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::GetState(
    const std::string& guid,
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  const chromeos::NetworkState* network_state =
      GetStateHandler()->GetNetworkStateFromServicePath(
          service_path, false /* configured_only */);
  if (!network_state) {
    failure_callback.Run(networking_private::kErrorNetworkUnavailable);
    return;
  }

  scoped_ptr<base::DictionaryValue> network_properties =
      chromeos::network_util::TranslateNetworkStateToONC(network_state);

  success_callback.Run(network_properties.Pass());
}

void NetworkingPrivateChromeOS::SetProperties(
    const std::string& guid,
    scoped_ptr<base::DictionaryValue> properties,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  GetManagedConfigurationHandler()->SetProperties(
      service_path, *properties, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::CreateNetwork(
    bool shared,
    scoped_ptr<base::DictionaryValue> properties,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string user_id_hash, error;
  // Do not allow configuring a non-shared network from a non-primary user.
  if (!shared && !GetUserIdHash(browser_context_, &user_id_hash, &error)) {
    failure_callback.Run(error);
    return;
  }

  GetManagedConfigurationHandler()->CreateConfiguration(
      user_id_hash, *properties, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::ForgetNetwork(
    const std::string& guid,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  GetManagedConfigurationHandler()->RemoveConfiguration(
      service_path, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::GetNetworks(
    const std::string& network_type,
    bool configured_only,
    bool visible_only,
    int limit,
    const NetworkListCallback& success_callback,
    const FailureCallback& failure_callback) {
  NetworkTypePattern pattern =
      chromeos::onc::NetworkTypePatternFromOncType(network_type);
  scoped_ptr<base::ListValue> network_properties_list =
      chromeos::network_util::TranslateNetworkListToONC(
          pattern, configured_only, visible_only, limit);
  success_callback.Run(network_properties_list.Pass());
}

void NetworkingPrivateChromeOS::StartConnect(
    const std::string& guid,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  const bool check_error_state = false;
  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      service_path, success_callback,
      base::Bind(&NetworkingPrivateChromeOS::ConnectFailureCallback,
                 weak_ptr_factory_.GetWeakPtr(), guid, success_callback,
                 failure_callback),
      check_error_state);
}

void NetworkingPrivateChromeOS::ConnectFailureCallback(
    const std::string& guid,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback,
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  // TODO(stevenjb): Temporary workaround to show the configuration UI.
  // Eventually the caller (e.g. Settings) should handle any failures and
  // show its own configuration UI. crbug.com/380937.
  if (ui_delegate()->HandleConnectFailed(guid, error_name)) {
    success_callback.Run();
    return;
  }
  failure_callback.Run(error_name);
}

void NetworkingPrivateChromeOS::StartDisconnect(
    const std::string& guid,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
      service_path, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::StartActivate(
    const std::string& guid,
    const std::string& specified_carrier,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  const chromeos::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network) {
    failure_callback.Run(
        extensions::networking_private::kErrorInvalidNetworkGuid);
    return;
  }

  std::string carrier(specified_carrier);
  if (carrier.empty()) {
    const chromeos::DeviceState* device =
        GetStateHandler()->GetDeviceState(network->device_path());
    if (device)
      carrier = device->carrier();
  }
  if (carrier != shill::kCarrierSprint) {
    // Only Sprint is directly activated. For other carriers, show the
    // account details page.
    if (ui_delegate())
      ui_delegate()->ShowAccountDetails(guid);
    success_callback.Run();
    return;
  }

  if (!network->RequiresActivation()) {
    // If no activation is required, show the account details page.
    if (ui_delegate())
      ui_delegate()->ShowAccountDetails(guid);
    success_callback.Run();
    return;
  }

  NetworkHandler::Get()->network_activation_handler()->Activate(
      network->path(), carrier, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::SetWifiTDLSEnabledState(
    const std::string& ip_or_mac_address,
    bool enabled,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  NetworkHandler::Get()->network_device_handler()->SetWifiTDLSEnabled(
      ip_or_mac_address, enabled, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::GetWifiTDLSStatus(
    const std::string& ip_or_mac_address,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  NetworkHandler::Get()->network_device_handler()->GetWifiTDLSStatus(
      ip_or_mac_address, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::GetCaptivePortalStatus(
    const std::string& guid,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  if (!chromeos::NetworkPortalDetector::IsInitialized()) {
    failure_callback.Run(networking_private::kErrorNotReady);
    return;
  }

  chromeos::NetworkPortalDetector::CaptivePortalState state =
      chromeos::NetworkPortalDetector::Get()->GetCaptivePortalState(guid);
  success_callback.Run(
      chromeos::NetworkPortalDetector::CaptivePortalStatusString(state.status));
}

void NetworkingPrivateChromeOS::UnlockCellularSim(
    const std::string& guid,
    const std::string& pin,
    const std::string& puk,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  const chromeos::NetworkState* network_state =
      GetStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network_state) {
    failure_callback.Run(networking_private::kErrorNetworkUnavailable);
    return;
  }
  const chromeos::DeviceState* device_state =
      GetStateHandler()->GetDeviceState(network_state->device_path());
  if (!device_state) {
    failure_callback.Run(networking_private::kErrorNetworkUnavailable);
    return;
  }
  std::string lock_type = device_state->sim_lock_type();
  if (lock_type.empty()) {
    // Sim is already unlocked.
    failure_callback.Run(networking_private::kErrorInvalidNetworkOperation);
    return;
  }

  // Unblock or unlock the SIM.
  if (lock_type == shill::kSIMLockPuk) {
    NetworkHandler::Get()->network_device_handler()->UnblockPin(
        device_state->path(), puk, pin, success_callback,
        base::Bind(&NetworkHandlerFailureCallback, failure_callback));
  } else {
    NetworkHandler::Get()->network_device_handler()->EnterPin(
        device_state->path(), pin, success_callback,
        base::Bind(&NetworkHandlerFailureCallback, failure_callback));
  }
}

void NetworkingPrivateChromeOS::SetCellularSimState(
    const std::string& guid,
    bool require_pin,
    const std::string& current_pin,
    const std::string& new_pin,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  const chromeos::NetworkState* network_state =
      GetStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network_state) {
    failure_callback.Run(networking_private::kErrorNetworkUnavailable);
    return;
  }
  const chromeos::DeviceState* device_state =
      GetStateHandler()->GetDeviceState(network_state->device_path());
  if (!device_state) {
    failure_callback.Run(networking_private::kErrorNetworkUnavailable);
    return;
  }
  if (!device_state->sim_lock_type().empty()) {
    // The SIM needs to be unlocked before the state can be changed.
    failure_callback.Run(networking_private::kErrorSimLocked);
    return;
  }

  // Only set a new pin if require_pin is true.
  std::string set_new_pin = require_pin ? new_pin : "";
  NetworkHandler::Get()->network_device_handler()->RequirePin(
      device_state->path(), require_pin, current_pin,
      base::Bind(&RequirePinSuccess, device_state->path(), current_pin,
                 set_new_pin, success_callback, failure_callback),
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

scoped_ptr<base::ListValue>
NetworkingPrivateChromeOS::GetEnabledNetworkTypes() {
  chromeos::NetworkStateHandler* state_handler = GetStateHandler();

  scoped_ptr<base::ListValue> network_list(new base::ListValue);

  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Ethernet()))
    network_list->AppendString(::onc::network_type::kEthernet);
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::WiFi()))
    network_list->AppendString(::onc::network_type::kWiFi);
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Wimax()))
    network_list->AppendString(::onc::network_type::kWimax);
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Cellular()))
    network_list->AppendString(::onc::network_type::kCellular);

  return network_list.Pass();
}

scoped_ptr<NetworkingPrivateDelegate::DeviceStateList>
NetworkingPrivateChromeOS::GetDeviceStateList() {
  std::set<std::string> technologies_found;
  NetworkStateHandler::DeviceStateList devices;
  NetworkHandler::Get()->network_state_handler()->GetDeviceList(&devices);

  scoped_ptr<DeviceStateList> device_state_list(new DeviceStateList);
  for (const DeviceState* device : devices) {
    std::string onc_type =
        chromeos::network_util::TranslateShillTypeToONC(device->type());
    AppendDeviceState(onc_type, device, device_state_list.get());
    technologies_found.insert(onc_type);
  }

  // For any technologies that we do not have a DeviceState entry for, append
  // an entry if the technolog is available.
  const char* technology_types[] = {::onc::network_type::kEthernet,
                                    ::onc::network_type::kWiFi,
                                    ::onc::network_type::kWimax,
                                    ::onc::network_type::kCellular};
  for (const char* technology : technology_types) {
    if (ContainsValue(technologies_found, technology))
      continue;
    AppendDeviceState(technology, nullptr /* device */,
                      device_state_list.get());
  }
  return device_state_list.Pass();
}

bool NetworkingPrivateChromeOS::EnableNetworkType(const std::string& type) {
  NetworkTypePattern pattern =
      chromeos::onc::NetworkTypePatternFromOncType(type);

  GetStateHandler()->SetTechnologyEnabled(
      pattern, true, chromeos::network_handler::ErrorCallback());

  return true;
}

bool NetworkingPrivateChromeOS::DisableNetworkType(const std::string& type) {
  NetworkTypePattern pattern =
      chromeos::onc::NetworkTypePatternFromOncType(type);

  GetStateHandler()->SetTechnologyEnabled(
      pattern, false, chromeos::network_handler::ErrorCallback());

  return true;
}

bool NetworkingPrivateChromeOS::RequestScan() {
  GetStateHandler()->RequestScan();
  return true;
}

}  // namespace extensions
