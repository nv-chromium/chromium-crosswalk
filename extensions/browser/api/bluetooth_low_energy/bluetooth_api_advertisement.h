// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_API_ADVERTISEMENT_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_API_ADVERTISEMENT_H_

#include <string>

#include "device/bluetooth/bluetooth_advertisement.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/bluetooth_low_energy/bluetooth_low_energy_event_router.h"

namespace device {
class BluetoothAdvertisement;
}  // namespace device

namespace apibtle = extensions::api::bluetooth_low_energy;

namespace extensions {

// Representation of advertisement instances from the "bluetooth" namespace,
// abstracting the underlying BluetoothAdvertisementXxx class. All methods
// must be called on the |kThreadId| thread.
class BluetoothApiAdvertisement : public ApiResource {
 public:
  BluetoothApiAdvertisement(const std::string& owner_extension_id,
                            scoped_refptr<device::BluetoothAdvertisement>);
  ~BluetoothApiAdvertisement() override;

  device::BluetoothAdvertisement* advertisement() {
    return advertisement_.get();
  }

  // Implementations of |BluetoothAdvertisement| require being called on the
  // UI thread.
  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::UI;

 private:
  friend class ApiResourceManager<BluetoothApiAdvertisement>;

  static const char* service_name() {
    return "BluetoothApiAdvertisementManager";
  }

  // The underlying advertisement instance.
  scoped_refptr<device::BluetoothAdvertisement> advertisement_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothApiAdvertisement);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_API_ADVERTISEMENT_H_
