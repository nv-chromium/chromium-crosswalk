// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The expectations in this test for the Chrome OS implementation. See
// networking_private_chromeos_apitest.cc for more info.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;
var assertTrue = chrome.test.assertTrue;
var assertFalse = chrome.test.assertFalse;
var assertEq = chrome.test.assertEq;

var ActivationStateType = chrome.networkingPrivate.ActivationStateType;
var ConnectionStateType = chrome.networkingPrivate.ConnectionStateType;
var NetworkType = chrome.networkingPrivate.NetworkType;

var kCellularGuid = 'stub_cellular1_guid';
var kDefaultPin = '1111';
var kDefaultPuk = '12345678';

// Test properties for the verification API.
var verificationProperties = {
  certificate: 'certificate',
  intermediateCertificates: ['ica1', 'ica2', 'ica3'],
  publicKey: 'cHVibGljX2tleQ==',  // Base64('public_key')
  nonce: 'nonce',
  signedData: 'c2lnbmVkX2RhdGE=',  // Base64('signed_data')
  deviceSerial: 'device_serial',
  deviceSsid: 'Device 0123',
  deviceBssid: '00:01:02:03:04:05'
};

var privateHelpers = {
  // Watches for the states |expectedStates| in reverse order. If all states
  // were observed in the right order, succeeds and calls |done|. If any
  // unexpected state is observed, fails.
  watchForStateChanges: function(network, expectedStates, done) {
    var self = this;
    var collectProperties = function(properties) {
      var finishTest = function() {
        chrome.networkingPrivate.onNetworksChanged.removeListener(
            self.onNetworkChange);
        done();
      };
      if (expectedStates.length > 0) {
        var expectedState = expectedStates.pop();
        assertEq(expectedState, properties.ConnectionState);
        if (expectedStates.length == 0)
          finishTest();
      }
    };
    this.onNetworkChange = function(changes) {
      assertEq([network], changes);
      chrome.networkingPrivate.getProperties(
          network,
          callbackPass(collectProperties));
    };
    chrome.networkingPrivate.onNetworksChanged.addListener(
        this.onNetworkChange);
  },
  listListener: function(expected, done) {
    var self = this;
    this.listenForChanges = function(list) {
      assertEq(expected, list);
      chrome.networkingPrivate.onNetworkListChanged.removeListener(
          self.listenForChanges);
      done();
    };
  },
  watchForCaptivePortalState: function(expectedGuid,
                                       expectedState,
                                       done) {
    var self = this;
    this.onPortalDetectionCompleted = function(guid, state) {
      assertEq(expectedGuid, guid);
      assertEq(expectedState, state);
      chrome.networkingPrivate.onPortalDetectionCompleted.removeListener(
          self.onPortalDetectionCompleted);
      done();
    };
    chrome.networkingPrivate.onPortalDetectionCompleted.addListener(
        self.onPortalDetectionCompleted);
  }
};

var kFailure = 'Failure';

function networkCallbackPass() {
  var callbackCompleted = chrome.test.callbackAdded();
  return function(result) {
    chrome.test.assertNoLastError();
    if (result === false || result === kFailure)
      chrome.test.fail('Failed: ' + result);
    callbackCompleted();
  };
}

var availableTests = [
  function startConnect() {
    chrome.networkingPrivate.startConnect('stub_wifi2_guid',
                                          networkCallbackPass());
  },
  function startDisconnect() {
    // Must connect to a network before we can disconnect from it.
    chrome.networkingPrivate.startConnect(
        'stub_wifi2_guid',
        callbackPass(function() {
          chrome.networkingPrivate.startDisconnect(
              'stub_wifi2_guid', networkCallbackPass());
        }));
  },
  function startActivate() {
    // Must connect to a network before we can activate it.
    chrome.networkingPrivate.startConnect(
        kCellularGuid, callbackPass(function() {
          chrome.networkingPrivate.startActivate(
              kCellularGuid, callbackPass(function() {
                // For non Sprint networks, startActivate will delegate
                // showing the activation UI to the browser host and not
                // immediately activate the network.
                chrome.networkingPrivate.getState(
                    kCellularGuid, callbackPass(function(state) {
                      assertEq(ActivationStateType.NOT_ACTIVATED,
                               state.Cellular.ActivationState);
                    }));
              }));
        }));
  },
  function startActivateSprint() {
    // Must connect to a network before we can activate it.
    chrome.networkingPrivate.startConnect(
        kCellularGuid, callbackPass(function() {
          chrome.networkingPrivate.startActivate(
              kCellularGuid, callbackPass(function() {
                chrome.networkingPrivate.getState(
                    kCellularGuid, callbackPass(function(state) {
                      assertEq(ActivationStateType.ACTIVATED,
                               state.Cellular.ActivationState);
                    }));
              }));
        }));
  },
  function startConnectNonexistent() {
    chrome.networkingPrivate.startConnect(
      'nonexistent_path',
      callbackFail('Error.InvalidNetworkGuid'));
  },
  function startDisconnectNonexistent() {
    chrome.networkingPrivate.startDisconnect(
      'nonexistent_path',
      callbackFail('Error.InvalidNetworkGuid'));
  },
  function startGetPropertiesNonexistent() {
    chrome.networkingPrivate.getProperties(
      'nonexistent_path',
      callbackFail('Error.InvalidNetworkGuid'));
  },
  function createNetwork() {
    chrome.networkingPrivate.createNetwork(
      false,  // shared
      { Type: NetworkType.WI_FI,
        GUID: 'ignored_guid',
        WiFi: {
          SSID: 'wifi_created',
          Security: 'WEP-PSK'
        }
      },
      callbackPass(function(guid) {
        assertFalse(guid == '');
        assertFalse(guid == 'ignored_guid');
        chrome.networkingPrivate.getProperties(
            guid, callbackPass(function(properties) {
              assertEq(NetworkType.WI_FI, properties.Type);
              assertEq(guid, properties.GUID);
              assertEq('wifi_created', properties.WiFi.SSID);
              assertEq('WEP-PSK', properties.WiFi.Security);
            }));
      }));
  },
  function forgetNetwork() {
    var kNumNetworks = 2;
    var kTestNetworkGuid = 'stub_wifi1_guid';
    function guidExists(networks, guid) {
      for (var n of networks) {
        if (n.GUID == kTestNetworkGuid)
          return true;
      }
      return false;
    }
    var filter = {
      networkType: NetworkType.WI_FI,
      visible: true,
      configured: true
    };
    chrome.networkingPrivate.getNetworks(
        filter, callbackPass(function(networks) {
          assertEq(kNumNetworks, networks.length);
          assertTrue(guidExists(networks, kTestNetworkGuid));
          chrome.networkingPrivate.forgetNetwork(
              kTestNetworkGuid, callbackPass(function() {
                chrome.networkingPrivate.getNetworks(
                    filter, callbackPass(function(networks) {
                      assertEq(kNumNetworks - 1, networks.length);
                      assertFalse(guidExists(networks, kTestNetworkGuid));
                    }));
              }));
        }));
  },
  function getNetworks() {
    // Test 'type' and 'configured'.
    var filter = {
      networkType: NetworkType.WI_FI,
      configured: true
    };
    chrome.networkingPrivate.getNetworks(
      filter,
      callbackPass(function(result) {
        assertEq([{
          Connectable: true,
          ConnectionState: ConnectionStateType.CONNECTED,
          GUID: 'stub_wifi1_guid',
          Name: 'wifi1',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_FI,
          WiFi: {
            Security: 'WEP-PSK',
            SignalStrength: 40
          }
        }, {
          GUID: 'stub_wifi2_guid',
          Name: 'wifi2_PSK',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_FI,
          WiFi: {
            Security: 'WPA-PSK',
          }
        }], result);

        // Test 'visible' (and 'configured').
        filter.visible = true;
        chrome.networkingPrivate.getNetworks(
          filter,
          callbackPass(function(result) {
            assertEq([{
              Connectable: true,
              ConnectionState: ConnectionStateType.CONNECTED,
              GUID: 'stub_wifi1_guid',
              Name: 'wifi1',
              Priority: 0,
              Source: 'User',
              Type: NetworkType.WI_FI,
              WiFi: {
                Security: 'WEP-PSK',
                SignalStrength: 40
              }
            }], result);

            // Test 'limit'.
            filter = {
              networkType: NetworkType.ALL,
              limit: 1
            };
            chrome.networkingPrivate.getNetworks(
              filter,
              callbackPass(function(result) {
                assertEq([{
                  ConnectionState: ConnectionStateType.CONNECTED,
                  Ethernet: {
                    Authentication: 'None'
                  },
                  GUID: 'stub_ethernet_guid',
                  Name: 'eth0',
                  Priority: 0,
                  Source: 'Device',
                  Type: NetworkType.ETHERNET
                }], result);
              }));
          }));
      }));
  },
  function getVisibleNetworks() {
    chrome.networkingPrivate.getVisibleNetworks(
      NetworkType.ALL,
      callbackPass(function(result) {
        assertEq([{
          ConnectionState: ConnectionStateType.CONNECTED,
          Ethernet: {
            Authentication: 'None'
          },
          GUID: 'stub_ethernet_guid',
          Name: 'eth0',
          Priority: 0,
          Source: 'Device',
          Type: NetworkType.ETHERNET
        }, {
          Connectable: true,
          ConnectionState: ConnectionStateType.CONNECTED,
          GUID: 'stub_wifi1_guid',
          Name: 'wifi1',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_FI,
          WiFi: {
            Security: 'WEP-PSK',
            SignalStrength: 40
          }
        }, {
          Connectable: true,
          ConnectionState: ConnectionStateType.CONNECTED,
          GUID: 'stub_wimax_guid',
          Name: 'wimax',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_MAX,
          WiMAX: {
            SignalStrength: 40
          }
        }, {
          ConnectionState: ConnectionStateType.CONNECTED,
          GUID: 'stub_vpn1_guid',
          Name: 'vpn1',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.VPN,
          VPN: {
            Type: 'OpenVPN'
          }
        }, {
          ConnectionState: ConnectionStateType.NOT_CONNECTED,
          GUID: 'stub_vpn2_guid',
          Name: 'vpn2',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.VPN,
          VPN: {
            ThirdPartyVPN: {
              ExtensionID: 'third_party_provider_extension_id'
            },
            Type: 'ThirdPartyVPN'
          }
        }, {
          Connectable: true,
          ConnectionState: ConnectionStateType.NOT_CONNECTED,
          GUID: 'stub_wifi2_guid',
          Name: 'wifi2_PSK',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_FI,
          WiFi: {
            Security: 'WPA-PSK',
            SignalStrength: 80
          }
        }], result);
      }));
  },
  function getVisibleNetworksWifi() {
    chrome.networkingPrivate.getVisibleNetworks(
      NetworkType.WI_FI,
      callbackPass(function(result) {
        assertEq([{
          Connectable: true,
          ConnectionState: ConnectionStateType.CONNECTED,
          GUID: 'stub_wifi1_guid',
          Name: 'wifi1',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_FI,
          WiFi: {
            Security: 'WEP-PSK',
            SignalStrength: 40
          }
        }, {
          Connectable: true,
          ConnectionState: ConnectionStateType.NOT_CONNECTED,
          GUID: 'stub_wifi2_guid',
          Name: 'wifi2_PSK',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_FI,
          WiFi: {
            Security: 'WPA-PSK',
            SignalStrength: 80
          }
        }], result);
      }));
  },
  function enabledNetworkTypes() {
    // Note: We call getEnabledNetworkTypes twice after each enable/dsiable
    // to ensure that Chrome has processed the command (since enable/disable
    // are 'synchronous' even though the action of enabling/disabling is not).
    chrome.networkingPrivate.getEnabledNetworkTypes(
        callbackPass(function(types) {
          assertTrue(types.indexOf('WiFi') >= 0);
          chrome.networkingPrivate.disableNetworkType('WiFi');
          chrome.networkingPrivate.getEnabledNetworkTypes(
              callbackPass(function(types) {
                chrome.networkingPrivate.getEnabledNetworkTypes(
                    callbackPass(function(types) {
                      assertFalse(types.indexOf('WiFi') >= 0);
                      chrome.networkingPrivate.enableNetworkType('WiFi');
                      chrome.networkingPrivate.getEnabledNetworkTypes(
                          callbackPass(function(types) {
                            chrome.networkingPrivate.getEnabledNetworkTypes(
                                callbackPass(function(types) {
                                  assertTrue(types.indexOf('WiFi') >= 0);
                                }));
                          }));
                    }));
              }));
        }));
  },
  function getDeviceStates() {
    chrome.networkingPrivate.getDeviceStates(callbackPass(function(result) {
      assertEq([
        {Scanning: false, State: 'Enabled', Type: 'Ethernet'},
        {Scanning: false, State: 'Enabled', Type: 'WiFi'},
        {State: 'Uninitialized', Type: 'Cellular'},
        {State: 'Disabled', Type: 'WiMAX'},
      ],
               result);
    }));
  },
  function requestNetworkScan() {
    // Connected or Connecting networks should be listed first, sorted by type.
    var expected = ['stub_ethernet_guid',
                    'stub_wifi1_guid',
                    'stub_wimax_guid',
                    'stub_vpn1_guid',
                    'stub_vpn2_guid',
                    'stub_wifi2_guid'];
    var done = chrome.test.callbackAdded();
    var listener = new privateHelpers.listListener(expected, done);
    chrome.networkingPrivate.onNetworkListChanged.addListener(
      listener.listenForChanges);
    chrome.networkingPrivate.requestNetworkScan();
  },
  function getProperties() {
    chrome.networkingPrivate.getProperties(
      'stub_wifi1_guid',
      callbackPass(function(result) {
        assertEq({
          Connectable: true,
          ConnectionState: ConnectionStateType.CONNECTED,
          GUID: 'stub_wifi1_guid',
          IPAddressConfigType: chrome.networkingPrivate.IPConfigType.STATIC,
          IPConfigs: [{
            Gateway: '0.0.0.1',
            IPAddress: '0.0.0.0',
            RoutingPrefix: 0,
            Type: 'IPv4'
          }],
          MacAddress: '00:11:22:AA:BB:CC',
          Name: 'wifi1',
          Source: 'User',
          StaticIPConfig: {
            IPAddress: '1.2.3.4',
            Type: 'IPv4'
          },
          Type: NetworkType.WI_FI,
          WiFi: {
            HexSSID: '7769666931', // 'wifi1'
            Frequency: 2400,
            FrequencyList: [2400],
            SSID: 'wifi1',
            Security: 'WEP-PSK',
            SignalStrength: 40
          }
        }, result);
      }));
  },
  function getPropertiesCellular() {
    chrome.networkingPrivate.getProperties(
      kCellularGuid,
      callbackPass(function(result) {
        assertEq({
          Cellular: {
            ActivationState: ActivationStateType.NOT_ACTIVATED,
            AllowRoaming: false,
            AutoConnect: true,
            Carrier: 'Cellular1_Carrier',
            Family: 'GSM',
            HomeProvider: {
              Country: 'us',
              Name: 'Cellular1_Provider'
            },
            NetworkTechnology: 'GSM',
            RoamingState: 'Home',
            SIMLockStatus: {LockEnabled: true, LockType: '', RetriesLeft: 3}
          },
          ConnectionState: ConnectionStateType.NOT_CONNECTED,
          GUID: kCellularGuid,
          Name: 'cellular1',
          Source: 'User',
          Type: NetworkType.CELLULAR,
        }, result);
      }));
  },
  function getManagedProperties() {
    chrome.networkingPrivate.getManagedProperties(
      'stub_wifi2',
      callbackPass(function(result) {
        assertEq({
          Connectable: true,
          ConnectionState: ConnectionStateType.NOT_CONNECTED,
          GUID: 'stub_wifi2',
          Name: {
            Active: 'wifi2_PSK',
            Effective: 'UserPolicy',
            UserPolicy: 'My WiFi Network'
          },
          Source: 'UserPolicy',
          Type: NetworkType.WI_FI,
          WiFi: {
            AutoConnect: {
              Active: false,
              UserEditable: true
            },
            HexSSID: {
              Active: '77696669325F50534B', // 'wifi2_PSK'
              Effective: 'UserPolicy',
              UserPolicy: '77696669325F50534B'
            },
            Frequency: 5000,
            FrequencyList: [2400, 5000],
            Passphrase: {
              Effective: 'UserSetting',
              UserEditable: true,
              UserSetting: 'FAKE_CREDENTIAL_VPaJDV9x'
            },
            SSID: {
              Active: 'wifi2_PSK',
              Effective: 'UserPolicy',
            },
            Security: {
              Active: 'WPA-PSK',
              Effective: 'UserPolicy',
              UserPolicy: 'WPA-PSK'
            },
            SignalStrength: 80,
          }
        }, result);
      }));
  },
  function setCellularProperties() {
    var network_guid = kCellularGuid;
    // Make sure we test Cellular.Carrier since it requires a special call
    // to Shill.Device.SetCarrier.
    var newCarrier = 'new_carrier';
    chrome.networkingPrivate.getProperties(
        network_guid,
        callbackPass(function(result) {
          assertEq(network_guid, result.GUID);
          assertTrue(!result.Cellular || result.Cellular.Carrier != newCarrier);
          var new_properties = {
            Priority: 1,
            Cellular: {
              Carrier: newCarrier,
            },
          };
          chrome.networkingPrivate.setProperties(
              network_guid,
              new_properties,
              callbackPass(function() {
                chrome.networkingPrivate.getProperties(
                    network_guid,
                    callbackPass(function(result) {
                      // Ensure that the GUID doesn't change.
                      assertEq(network_guid, result.GUID);
                      // Ensure that the properties were set.
                      assertEq(1, result['Priority']);
                      assertTrue('Cellular' in result);
                      assertEq(newCarrier, result.Cellular.Carrier);
                    }));
              }));
        }));
  },
  function setWiFiProperties() {
    var network_guid = 'stub_wifi1_guid';
    chrome.networkingPrivate.getProperties(
        network_guid,
        callbackPass(function(result) {
          assertEq(network_guid, result.GUID);
          var new_properties = {
            Priority: 1,
            WiFi: {
              AutoConnect: true
            },
            IPAddressConfigType: 'Static',
            StaticIPConfig: {
              IPAddress: '1.2.3.4'
            }
          };
          chrome.networkingPrivate.setProperties(
              network_guid,
              new_properties,
              callbackPass(function() {
                chrome.networkingPrivate.getProperties(
                    network_guid,
                    callbackPass(function(result) {
                      // Ensure that the GUID doesn't change.
                      assertEq(network_guid, result.GUID);
                      // Ensure that the properties were set.
                      assertEq(1, result['Priority']);
                      assertTrue('WiFi' in result);
                      assertTrue('AutoConnect' in result['WiFi']);
                      assertEq(true, result['WiFi']['AutoConnect']);
                      assertTrue('StaticIPConfig' in result);
                      assertEq('1.2.3.4',
                               result['StaticIPConfig']['IPAddress']);
                    }));
              }));
        }));
  },
  function setVPNProperties() {
    var network_guid = 'stub_vpn1_guid';
    chrome.networkingPrivate.getProperties(
        network_guid,
        callbackPass(function(result) {
          assertEq(network_guid, result.GUID);
          var new_properties = {
            Priority: 1,
            VPN: {
              Host: 'vpn.host1'
            }
          };
          chrome.networkingPrivate.setProperties(
              network_guid,
              new_properties,
              callbackPass(function() {
                chrome.networkingPrivate.getProperties(
                    network_guid,
                    callbackPass(function(result) {
                      // Ensure that the properties were set.
                      assertEq(1, result['Priority']);
                      assertTrue('VPN' in result);
                      assertTrue('Host' in result['VPN']);
                      assertEq('vpn.host1', result['VPN']['Host']);
                      // Ensure that the GUID doesn't change.
                      assertEq(network_guid, result.GUID);
                    }));
              }));
        }));
  },
  function getState() {
    chrome.networkingPrivate.getState(
      'stub_wifi2_guid',
      callbackPass(function(result) {
        assertEq({
          Connectable: true,
          ConnectionState: ConnectionStateType.NOT_CONNECTED,
          GUID: 'stub_wifi2_guid',
          Name: 'wifi2_PSK',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_FI,
          WiFi: {
            Security: 'WPA-PSK',
            SignalStrength: 80
          }
        }, result);
      }));
  },
  function getStateNonExistent() {
    chrome.networkingPrivate.getState(
      'non_existent',
      callbackFail('Error.InvalidNetworkGuid'));
  },
  function getErrorState() {
    // Both getState and getProperties should have ErrorState set.
    chrome.networkingPrivate.getState(
        'stub_wifi1_guid', callbackPass(function(result) {
          assertEq('TestErrorState', result.ErrorState);
          chrome.networkingPrivate.getProperties(
              'stub_wifi1_guid', callbackPass(function(result2) {
                assertEq('TestErrorState', result2.ErrorState);
              }));
        }));
  },
  function onNetworksChangedEventConnect() {
    var network = 'stub_wifi2_guid';
    var done = chrome.test.callbackAdded();
    var expectedStates = [ConnectionStateType.CONNECTED];
    var listener =
        new privateHelpers.watchForStateChanges(network, expectedStates, done);
    chrome.networkingPrivate.startConnect(network,
                                          networkCallbackPass());
  },
  function onNetworksChangedEventDisconnect() {
    var network = 'stub_wifi1_guid';
    var done = chrome.test.callbackAdded();
    var expectedStates = [ConnectionStateType.NOT_CONNECTED];
    var listener =
        new privateHelpers.watchForStateChanges(network, expectedStates, done);
    chrome.networkingPrivate.startDisconnect(network,
                                             networkCallbackPass());
  },
  function onNetworkListChangedEvent() {
    // Connecting to wifi2 should set wifi1 to offline. Connected or Connecting
    // networks should be listed first, sorted by type.
    var expected = ['stub_ethernet_guid',
                    'stub_wifi2_guid',
                    'stub_wimax_guid',
                    'stub_vpn1_guid',
                    'stub_wifi1_guid',
                    'stub_vpn2_guid'];
    var done = chrome.test.callbackAdded();
    var listener = new privateHelpers.listListener(expected, done);
    chrome.networkingPrivate.onNetworkListChanged.addListener(
      listener.listenForChanges);
    var network = 'stub_wifi2_guid';
    chrome.networkingPrivate.startConnect(network,
                                          networkCallbackPass());
  },
  function onDeviceStateListChangedEvent() {
    var listener = callbackPass(function() {
      chrome.networkingPrivate.onDeviceStateListChanged.removeListener(
          listener);
    });
    chrome.networkingPrivate.onDeviceStateListChanged.addListener(listener);
    chrome.networkingPrivate.disableNetworkType('WiFi');
  },
  function verifyDestination() {
    chrome.networkingPrivate.verifyDestination(
      verificationProperties,
      callbackPass(function(isValid) {
        assertTrue(isValid);
      }));
  },
  function verifyAndEncryptCredentials() {
    var network_guid = 'stub_wifi2_guid';
    chrome.networkingPrivate.verifyAndEncryptCredentials(
      verificationProperties,
      network_guid,
      callbackPass(function(result) {
        assertEq('encrypted_credentials', result);
      }));
  },
  function verifyAndEncryptData() {
    chrome.networkingPrivate.verifyAndEncryptData(
      verificationProperties,
      'data',
      callbackPass(function(result) {
        assertEq('encrypted_data', result);
      }));
  },
  function setWifiTDLSEnabledState() {
    chrome.networkingPrivate.setWifiTDLSEnabledState(
        'aa:bb:cc:dd:ee:ff', true, callbackPass(function(result) {
          assertEq(ConnectionStateType.CONNECTED, result);
        }));
  },
  function getWifiTDLSStatus() {
    chrome.networkingPrivate.getWifiTDLSStatus(
        'aa:bb:cc:dd:ee:ff', callbackPass(function(result) {
          assertEq(ConnectionStateType.CONNECTED, result);
        }));
  },
  function getCaptivePortalStatus() {
    var networks = [['stub_ethernet_guid', 'Online'],
                    ['stub_wifi1_guid', 'Offline'],
                    ['stub_wifi2_guid', 'Portal'],
                    ['stub_cellular1_guid', 'ProxyAuthRequired'],
                    ['stub_vpn1_guid', 'Unknown']];
    networks.forEach(function(network) {
      var guid = network[0];
      var expectedStatus = network[1];
      chrome.networkingPrivate.getCaptivePortalStatus(
        guid,
        callbackPass(function(status) {
          assertEq(expectedStatus, status);
        }));
    });
  },
  function captivePortalNotification() {
    var done = chrome.test.callbackAdded();
    var listener =
        new privateHelpers.watchForCaptivePortalState(
            'wifi_guid', 'Online', done);
    chrome.test.sendMessage('notifyPortalDetectorObservers');
  },
  function unlockCellularSim() {
    var incorrectPin = '2222';
    // Try with incorrect PIN, expect failure.
    chrome.networkingPrivate.unlockCellularSim(
        kCellularGuid, incorrectPin, '',
        callbackFail('incorrect-pin', function() {
          // Try with correct PIN, expect success.
          chrome.networkingPrivate.unlockCellularSim(
              kCellularGuid, kDefaultPin, '', networkCallbackPass());
        }));
  },
  function setCellularSimState() {
    var newPin = '6666';
    var simState = {requirePin: true, currentPin: kDefaultPin, newPin: newPin};
    // Test setting 'requirePin' and 'newPin'.
    chrome.networkingPrivate.getProperties(
        kCellularGuid, callbackPass(function(result) {
          // Ensure the SIM is initially unlocked.
          assertTrue(result.Cellular.SIMLockStatus == undefined ||
                     result.Cellular.SIMLockStatus.LockType == '');
          chrome.networkingPrivate.setCellularSimState(
              kCellularGuid, simState, callbackPass(function() {
                chrome.networkingPrivate.getProperties(
                    kCellularGuid, callbackPass(function(result) {
                      // The SIM should still be unlocked.
                      assertEq('', result.Cellular.SIMLockStatus.LockType);
                      // Ensure SIM locking is enabled.
                      assertTrue(result.Cellular.SIMLockStatus.LockEnabled);
                      // Ensure the new pin is set by using the new PIN
                      // to change the PIN back.
                      simState.currentPin = newPin;
                      simState.newPin = kDefaultPin;
                      chrome.networkingPrivate.setCellularSimState(
                          kCellularGuid, simState, networkCallbackPass());
                    }));
              }));
        }));
  },
  function cellularSimPuk() {
    var newPin = '6666';
    var incorrectPin = '2222';
    var incorrectPuk = '22222222';
    var unlockFailFunc = function(nextFunc) {
      chrome.networkingPrivate.unlockCellularSim(
          kCellularGuid, incorrectPin, '',
          callbackFail('incorrect-pin', nextFunc));
    };
    // Try with incorrect PIN three times, SIM should become PUK locked.
    unlockFailFunc(unlockFailFunc(unlockFailFunc(function() {
      // Ensure the SIM is PUK locked.
      chrome.networkingPrivate.getProperties(
          kCellularGuid, callbackPass(function(result) {
            assertEq('sim-puk', result.Cellular.SIMLockStatus.LockType);
            // Try to unlock with an incorrect PUK, expect failure.
            chrome.networkingPrivate.unlockCellularSim(
                kCellularGuid, newPin, incorrectPuk,
                callbackFail('incorrect-pin', function() {
                  // Try with the correct PUK, expect success.
                  chrome.networkingPrivate.unlockCellularSim(
                      kCellularGuid, newPin, kDefaultPuk,
                      callbackPass(function() {
                        // Set state with the new PIN, expect success.
                        var simState = {requirePin: true, currentPin: newPin};
                        chrome.networkingPrivate.setCellularSimState(
                            kCellularGuid, simState, networkCallbackPass());
                      }));
                }));
          }));
    })));
  }
];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));
