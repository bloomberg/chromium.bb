// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_SERVICE_CONSTANTS_H_
#define SYSTEM_API_DBUS_SERVICE_CONSTANTS_H_

#include <stdint.h>  // for uint32_t

// We use relative includes here to make this compatible with both the
// Chromium OS and Chromium environment.
#include "authpolicy/dbus-constants.h"
#include "biod/dbus-constants.h"
#include "cecservice/dbus-constants.h"
#include "cros-disks/dbus-constants.h"
#include "cros_healthd/dbus-constants.h"
#include "cryptohome/dbus-constants.h"
#include "debugd/dbus-constants.h"
#include "drivefs/dbus-constants.h"
#include "hammerd/dbus-constants.h"
#include "hermes/dbus-constants.h"
#include "login_manager/dbus-constants.h"
#include "lorgnette/dbus-constants.h"
#include "oobe_config/dbus-constants.h"
#include "permission_broker/dbus-constants.h"
#include "power_manager/dbus-constants.h"
#include "runtime_probe/dbus-constants.h"
#include "seneschal/dbus-constants.h"
#include "shill/dbus-constants.h"
#include "smbprovider/dbus-constants.h"
#include "update_engine/dbus-constants.h"
#include "vm_applications/dbus-constants.h"
#include "vm_cicerone/dbus-constants.h"
#include "vm_concierge/dbus-constants.h"
#include "vm_plugin_dispatcher/dbus-constants.h"
#include "wilco_dtc_supportd/dbus-constants.h"

namespace dbus {
const char kDBusInterface[] = "org.freedesktop.DBus";
const char kDBusServiceName[] = "org.freedesktop.DBus";
const char kDBusServicePath[] = "/org/freedesktop/DBus";

// Object Manager interface
const char kDBusObjectManagerInterface[] = "org.freedesktop.DBus.ObjectManager";
// Methods
const char kDBusObjectManagerGetManagedObjects[] = "GetManagedObjects";
// Signals
const char kDBusObjectManagerInterfacesAddedSignal[] = "InterfacesAdded";
const char kDBusObjectManagerInterfacesRemovedSignal[] = "InterfacesRemoved";

// Properties interface
const char kDBusPropertiesInterface[] = "org.freedesktop.DBus.Properties";
// Methods
const char kDBusPropertiesGet[] = "Get";
const char kDBusPropertiesSet[] = "Set";
const char kDBusPropertiesGetAll[] = "GetAll";
// Signals
const char kDBusPropertiesChangedSignal[] = "PropertiesChanged";
}  // namespace dbus

namespace imageburn {
const char kImageBurnServiceName[] = "org.chromium.ImageBurner";
const char kImageBurnServicePath[] = "/org/chromium/ImageBurner";
const char kImageBurnServiceInterface[] = "org.chromium.ImageBurnerInterface";
// Methods
const char kBurnImage[] = "BurnImage";
// Signals
const char kSignalBurnFinishedName[] = "burn_finished";
const char kSignalBurnUpdateName[] = "burn_progress_update";
}  // namespace imageburn

namespace imageloader {
const char kImageLoaderServiceInterface[] = "org.chromium.ImageLoaderInterface";
const char kImageLoaderServiceName[] = "org.chromium.ImageLoader";
const char kImageLoaderServicePath[] = "/org/chromium/ImageLoader";
// Methods
const char kRegisterComponent[] = "RegisterComponent";
const char kLoadComponent[] = "LoadComponent";
const char kLoadComponentAtPath[] = "LoadComponentAtPath";
const char kGetComponentVersion[] = "GetComponentVersion";
const char kRemoveComponent[] = "RemoveComponent";
const char kUnmountComponent[] = "UnmountComponent";
const char kLoadDlcImage[] = "LoadDlcImage";
// Constants
const char kBadResult[] = "";
const char kTerminaComponentName[] = "cros-termina";
const char kSlotNameA[] = "Dlc-A";
const char kSlotNameB[] = "Dlc-B";
}  // namespace imageloader

namespace speech_synthesis {
const char kSpeechSynthesizerInterface[] =
    "org.chromium.SpeechSynthesizerInterface";
const char kSpeechSynthesizerServicePath[] = "/org/chromium/SpeechSynthesizer";
const char kSpeechSynthesizerServiceName[] = "org.chromium.SpeechSynthesizer";
// Methods
const char kSpeak[] = "Speak";
const char kStop[] = "Stop";
const char kIsSpeaking[] = "IsSpeaking";
const char kShutdown[] = "Shutdown";
}  // namespace speech_synthesis

namespace chromium {
const char kChromiumInterface[] = "org.chromium.Chromium";
// Text-to-speech service signals.
const char kTTSReadySignal[] = "TTSReady";
const char kTTSFailedSignal[] = "TTSFailed";
}  // namespace chromium

// Services in the chromeos namespace are owned by Chrome. Different services
// may be instantiated in different Chrome processes.
namespace chromeos {

const char kNetworkProxyServiceName[] = "org.chromium.NetworkProxyService";
const char kNetworkProxyServicePath[] = "/org/chromium/NetworkProxyService";
const char kNetworkProxyServiceInterface[] =
    "org.chromium.NetworkProxyServiceInterface";
const char kNetworkProxyServiceResolveProxyMethod[] = "ResolveProxy";

const char kLivenessServiceName[] = "org.chromium.LivenessService";
const char kLivenessServicePath[] = "/org/chromium/LivenessService";
const char kLivenessServiceInterface[] =
    "org.chromium.LivenessServiceInterface";
const char kLivenessServiceCheckLivenessMethod[] = "CheckLiveness";

const char kMetricsEventServiceName[] = "org.chromium.MetricsEventService";
const char kMetricsEventServicePath[] = "/org/chromium/MetricsEventService";
const char kMetricsEventServiceInterface[] =
    "org.chromium.MetricsEventServiceInterface";
const char kMetricsEventServiceChromeEventSignal[] = "ChromeEvent";

const char kComponentUpdaterServiceName[] =
    "org.chromium.ComponentUpdaterService";
const char kComponentUpdaterServicePath[] =
    "/org/chromium/ComponentUpdaterService";
const char kComponentUpdaterServiceInterface[] =
    "org.chromium.ComponentUpdaterService";
const char kComponentUpdaterServiceLoadComponentMethod[] = "LoadComponent";
const char kComponentUpdaterServiceUnloadComponentMethod[] = "UnloadComponent";
const char kComponentUpdaterServiceComponentInstalledSignal[] =
    "ComponentInstalled";

const char kKioskAppServiceName[] = "org.chromium.KioskAppService";
const char kKioskAppServicePath[] = "/org/chromium/KioskAppService";
const char kKioskAppServiceInterface[] =
    "org.chromium.KioskAppServiceInterface";
const char kKioskAppServiceGetRequiredPlatformVersionMethod[] =
    "GetRequiredPlatformVersion";

const char kDisplayServiceName[] = "org.chromium.DisplayService";
const char kDisplayServicePath[] = "/org/chromium/DisplayService";
const char kDisplayServiceInterface[] = "org.chromium.DisplayServiceInterface";
const char kDisplayServiceSetPowerMethod[] = "SetPower";
const char kDisplayServiceSetSoftwareDimmingMethod[] = "SetSoftwareDimming";
const char kDisplayServiceTakeOwnershipMethod[] = "TakeOwnership";
const char kDisplayServiceReleaseOwnershipMethod[] = "ReleaseOwnership";
enum DisplayPowerState {
  DISPLAY_POWER_ALL_ON = 0,
  DISPLAY_POWER_ALL_OFF = 1,
  DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON = 2,
  DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF = 3,
};

const char kScreenLockServiceName[] = "org.chromium.ScreenLockService";
const char kScreenLockServicePath[] = "/org/chromium/ScreenLockService";
const char kScreenLockServiceInterface[] =
    "org.chromium.ScreenLockServiceInterface";
const char kScreenLockServiceShowLockScreenMethod[] = "ShowLockScreen";

constexpr char kVirtualFileRequestServiceName[] =
    "org.chromium.VirtualFileRequestService";
constexpr char kVirtualFileRequestServicePath[] =
    "/org/chromium/VirtualFileRequestService";
constexpr char kVirtualFileRequestServiceInterface[] =
    "org.chromium.VirtualFileRequestService";
constexpr char kVirtualFileRequestServiceHandleReadRequestMethod[] =
    "HandleReadRequest";
constexpr char kVirtualFileRequestServiceHandleIdReleasedMethod[] =
    "HandleIdReleased";

const char kChromeFeaturesServiceName[] = "org.chromium.ChromeFeaturesService";
const char kChromeFeaturesServicePath[] = "/org/chromium/ChromeFeaturesService";
const char kChromeFeaturesServiceInterface[] =
    "org.chromium.ChromeFeaturesServiceInterface";
const char kChromeFeaturesServiceIsFeatureEnabledMethod[] =
    "IsFeatureEnabled";
const char kChromeFeaturesServiceIsCrostiniEnabledMethod[] =
    "IsCrostiniEnabled";
const char kChromeFeaturesServiceIsPluginVmEnabledMethod[] =
    "IsPluginVmEnabled";
const char kChromeFeaturesServiceIsUsbguardEnabledMethod[] =
    "IsUsbguardEnabled";
const char kChromeFeaturesServiceIsShillSandboxingEnabledMethod[] =
    "IsShillSandboxingEnabled";
const char kChromeFeaturesServiceIsFsNosymfollowEnabledMethod[] =
    "IsFsNosymfollowEnabled";

const char kUrlHandlerServiceName[] = "org.chromium.UrlHandlerService";
const char kUrlHandlerServicePath[] = "/org/chromium/UrlHandlerService";
const char kUrlHandlerServiceInterface[] =
    "org.chromium.UrlHandlerServiceInterface";
const char kUrlHandlerServiceOpenUrlMethod[] = "OpenUrl";

const char kPluginVmServiceName[] = "org.chromium.PluginVmService";
const char kPluginVmServicePath[] = "/org/chromium/PluginVmService";
const char kPluginVmServiceInterface[] =
    "org.chromium.PluginVmServiceInterface";
const char kPluginVmServiceGetLicenseDataMethod[] = "GetLicenseData";
const char kPluginVmServiceShowSettingsPage[] = "ShowSettingsPage";

const char kGesturePropertiesServiceName[] =
    "org.chromium.GesturePropertiesService";
const char kGesturePropertiesServicePath[] =
    "/org/chromium/GesturePropertiesService";
const char kGesturePropertiesServiceInterface[] =
    "org.chromium.GesturePropertiesServiceInterface";
const char kGesturePropertiesServiceListDevicesMethod[] = "ListDevices";
const char kGesturePropertiesServiceListPropertiesMethod[] = "ListProperties";
const char kGesturePropertiesServiceGetPropertyMethod[] = "GetProperty";
const char kGesturePropertiesServiceSetPropertyMethod[] = "SetProperty";

}  // namespace chromeos

namespace media_perception {

const char kMediaPerceptionServiceName[] = "org.chromium.MediaPerception";
const char kMediaPerceptionServicePath[] = "/org/chromium/MediaPerception";
const char kMediaPerceptionInterface[] = "org.chromium.MediaPerception";

const char kStateFunction[] = "State";
const char kGetDiagnosticsFunction[] = "GetDiagnostics";
const char kDetectionSignal[] = "MediaPerceptionDetection";
const char kBootstrapMojoConnection[] = "BootstrapMojoConnection";

}  // namespace media_perception

namespace modemmanager {
// ModemManager D-Bus service identifiers
const char kModemManagerSMSInterface[] =
    "org.freedesktop.ModemManager.Modem.Gsm.SMS";

// ModemManager function names.
const char kSMSGetFunction[] = "Get";
const char kSMSDeleteFunction[] = "Delete";
const char kSMSListFunction[] = "List";

// ModemManager monitored signals
const char kSMSReceivedSignal[] = "SmsReceived";

// ModemManager1 interfaces and signals
// The canonical source for these constants is:
//   /usr/include/ModemManager/ModemManager-names.h
const char kModemManager1ServiceName[] = "org.freedesktop.ModemManager1";
const char kModemManager1ServicePath[] = "/org/freedesktop/ModemManager1";
const char kModemManager1ModemInterface[] =
    "org.freedesktop.ModemManager1.Modem";
const char kModemManager1MessagingInterface[] =
    "org.freedesktop.ModemManager1.Modem.Messaging";
const char kModemManager1SmsInterface[] =
    "org.freedesktop.ModemManager1.Sms";
const char kSMSAddedSignal[] = "Added";
}  // namespace modemmanager

namespace bluetooth_plugin {
// Service identifiers for the plugin interface added to the /org/bluez object.
const char kBluetoothPluginServiceName[] = "org.bluez";
const char kBluetoothPluginInterface[] = "org.chromium.Bluetooth";

// Bluetooth plugin properties.
const char kSupportsLEServices[] = "SupportsLEServices";
const char kSupportsConnInfo[] = "SupportsConnInfo";
}  // namespace bluetooth_plugin

namespace bluetooth_plugin_device {
// Service identifiers for the plugin interface added to Bluetooth Device
// objects.
const char kBluetoothPluginServiceName[] = "org.bluez";
const char kBluetoothPluginInterface[] = "org.chromium.BluetoothDevice";

// Bluetooth Device plugin methods.
const char kGetConnInfo[] = "GetConnInfo";
const char kSetLEConnectionParameters[] = "SetLEConnectionParameters";
// Valid connection parameters that can be passed to the
// SetLEConnectionParameters API as dictionary keys.
const char kLEConnectionParameterMinimumConnectionInterval[] =
    "MinimumConnectionInterval";
const char kLEConnectionParameterMaximumConnectionInterval[] =
    "MaximumConnectionInterval";
}  // namespace bluetooth_plugin_device

namespace bluetooth_adapter {
// Bluetooth Adapter service identifiers.
const char kBluetoothAdapterServiceName[] = "org.bluez";
const char kBluetoothAdapterInterface[] = "org.bluez.Adapter1";

// Bluetooth Adapter methods.
const char kStartDiscovery[] = "StartDiscovery";
const char kSetDiscoveryFilter[] = "SetDiscoveryFilter";
const char kStopDiscovery[] = "StopDiscovery";
const char kPauseDiscovery[] = "PauseDiscovery";
const char kUnpauseDiscovery[] = "UnpauseDiscovery";
const char kRemoveDevice[] = "RemoveDevice";
const char kCreateServiceRecord[] = "CreateServiceRecord";
const char kRemoveServiceRecord[] = "RemoveServiceRecord";
const char kHandleSuspendImminent[] = "HandleSuspendImminent";
const char kHandleSuspendDone[] = "HandleSuspendDone";

// Bluetooth Adapter properties.
const char kAddressProperty[] = "Address";
const char kNameProperty[] = "Name";
const char kAliasProperty[] = "Alias";
const char kClassProperty[] = "Class";
const char kPoweredProperty[] = "Powered";
const char kDiscoverableProperty[] = "Discoverable";
const char kPairableProperty[] = "Pairable";
const char kPairableTimeoutProperty[] = "PairableTimeout";
const char kDiscoverableTimeoutProperty[] = "DiscoverableTimeout";
const char kDiscoveringProperty[] = "Discovering";
const char kUUIDsProperty[] = "UUIDs";
const char kModaliasProperty[] = "Modalias";
const char kStackSyncQuittingProperty[] = "StackSyncQuitting";

// Bluetooth Adapter errors.
const char kErrorNotReady[] = "org.bluez.Error.NotReady";
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorInProgress[] = "org.bluez.Error.InProgress";
const char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
const char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";

// Bluetooth Adapter parameters supplied to SetDiscoveryFilter request.
const char kDiscoveryFilterParameterUUIDs[] = "UUIDs";
const char kDiscoveryFilterParameterRSSI[] = "RSSI";
const char kDiscoveryFilterParameterPathloss[] = "Pathloss";
const char kDiscoveryFilterParameterTransport[] = "Transport";
}  // namespace bluetooth_adapter

namespace bluetooth_agent_manager {
// Bluetooth Agent Manager service indentifiers
const char kBluetoothAgentManagerServiceName[] = "org.bluez";
const char kBluetoothAgentManagerServicePath[] = "/org/bluez";
const char kBluetoothAgentManagerInterface[] = "org.bluez.AgentManager1";

// Bluetooth Agent Manager methods.
const char kRegisterAgent[] = "RegisterAgent";
const char kUnregisterAgent[] = "UnregisterAgent";
const char kRequestDefaultAgent[] = "RequestDefaultAgent";

// Bluetooth capabilities.
const char kNoInputNoOutputCapability[] = "NoInputNoOutput";
const char kDisplayOnlyCapability[] = "DisplayOnly";
const char kKeyboardOnlyCapability[] = "KeyboardOnly";
const char kDisplayYesNoCapability[] = "DisplayYesNo";
const char kKeyboardDisplayCapability[] = "KeyboardDisplay";

// Bluetooth Agent Manager errors.
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
const char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
}  // namespace bluetooth_agent_manager


namespace bluetooth_agent {
// Bluetooth Agent service indentifiers
const char kBluetoothAgentInterface[] = "org.bluez.Agent1";

// Bluetooth Agent methods.
const char kRelease[] = "Release";
const char kRequestPinCode[] = "RequestPinCode";
const char kDisplayPinCode[] = "DisplayPinCode";
const char kRequestPasskey[] = "RequestPasskey";
const char kDisplayPasskey[] = "DisplayPasskey";
const char kRequestConfirmation[] = "RequestConfirmation";
const char kRequestAuthorization[] = "RequestAuthorization";
const char kAuthorizeService[] = "AuthorizeService";
const char kCancel[] = "Cancel";

// Bluetooth Agent errors.
const char kErrorRejected[] = "org.bluez.Error.Rejected";
const char kErrorCanceled[] = "org.bluez.Error.Canceled";
}  // namespace bluetooth_agent

namespace bluetooth_device {
// Bluetooth Device service identifiers.
const char kBluetoothDeviceServiceName[] = "org.bluez";
const char kBluetoothDeviceInterface[] = "org.bluez.Device1";

// Bluetooth Device methods.
const char kConnect[] = "Connect";
const char kDisconnect[] = "Disconnect";
const char kConnectProfile[] = "ConnectProfile";
const char kDisconnectProfile[] = "DisconnectProfile";
const char kPair[] = "Pair";
const char kCancelPairing[] = "CancelPairing";
const char kGetServiceRecords[] = "GetServiceRecords";
const char kExecuteWrite[] = "ExecuteWrite";

// Bluetooth Device properties.
const char kAddressProperty[] = "Address";
const char kNameProperty[] = "Name";
const char kIconProperty[] = "Icon";
const char kClassProperty[] = "Class";
const char kTypeProperty[] = "Type";
const char kAppearanceProperty[] = "Appearance";
const char kUUIDsProperty[] = "UUIDs";
const char kPairedProperty[] = "Paired";
const char kConnectedProperty[] = "Connected";
const char kTrustedProperty[] = "Trusted";
const char kBlockedProperty[] = "Blocked";
const char kAliasProperty[] = "Alias";
const char kAdapterProperty[] = "Adapter";
const char kLegacyPairingProperty[] = "LegacyPairing";
const char kModaliasProperty[] = "Modalias";
const char kRSSIProperty[] = "RSSI";
const char kTxPowerProperty[] = "TxPower";
const char kManufacturerDataProperty[] = "ManufacturerData";
const char kServiceDataProperty[] = "ServiceData";
const char kServicesResolvedProperty[] = "ServicesResolved";
const char kAdvertisingDataFlagsProperty[] = "AdvertisingFlags";
const char kMTUProperty[] = "MTU";
const char kEIRProperty[] = "EIR";

// Bluetooth Device errors.
const char kErrorNotReady[] = "org.bluez.Error.NotReady";
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorInProgress[] = "org.bluez.Error.InProgress";
const char kErrorAlreadyConnected[] = "org.bluez.Error.AlreadyConnected";
const char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
const char kErrorNotConnected[] = "org.bluez.Error.NotConnected";
const char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";

// Undocumented errors that we know BlueZ returns for Bluetooth Device methods.
const char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
const char kErrorAuthenticationCanceled[] =
    "org.bluez.Error.AuthenticationCanceled";
const char kErrorAuthenticationFailed[] =
    "org.bluez.Error.AuthenticationFailed";
const char kErrorAuthenticationRejected[] =
    "org.bluez.Error.AuthenticationRejected";
const char kErrorAuthenticationTimeout[] =
    "org.bluez.Error.AuthenticationTimeout";
const char kErrorConnectionAttemptFailed[] =
    "org.bluez.Error.ConnectionAttemptFailed";
}  // namespace bluetooth_device

namespace bluetooth_gatt_characteristic {
// Bluetooth GATT Characteristic service identifiers. The service name is used
// only for characteristic objects hosted by bluetoothd.
const char kBluetoothGattCharacteristicServiceName[] = "org.bluez";
const char kBluetoothGattCharacteristicInterface[] =
    "org.bluez.GattCharacteristic1";

// Bluetooth GATT Characteristic methods.
const char kReadValue[] = "ReadValue";
const char kWriteValue[] = "WriteValue";
const char kStartNotify[] = "StartNotify";
const char kStopNotify[] = "StopNotify";
const char kPrepareWriteValue[] = "PrepareWriteValue";

// Bluetooth GATT Characteristic signals.
const char kValueUpdatedSignal[] = "ValueUpdated";

// Possible keys for option dict used in ReadValue, WriteValue and
// PrepareWriteValue.
const char kOptionOffset[] = "offset";
const char kOptionDevice[] = "device";
const char kOptionHasSubsequentWrite[] = "has-subsequent-write";

// Bluetooth GATT Characteristic properties.
const char kUUIDProperty[] = "UUID";
const char kServiceProperty[] = "Service";
const char kValueProperty[] = "Value";
const char kFlagsProperty[] = "Flags";
const char kNotifyingProperty[] = "Notifying";
const char kDescriptorsProperty[] = "Descriptors";

// Possible values for Bluetooth GATT Characteristic "Flags" property.
const char kFlagBroadcast[] = "broadcast";
const char kFlagRead[] = "read";
const char kFlagWriteWithoutResponse[] = "write-without-response";
const char kFlagWrite[] = "write";
const char kFlagNotify[] = "notify";
const char kFlagIndicate[] = "indicate";
const char kFlagAuthenticatedSignedWrites[] = "authenticated-signed-writes";
const char kFlagExtendedProperties[] = "extended-properties";
const char kFlagReliableWrite[] = "reliable-write";
const char kFlagWritableAuxiliaries[] = "writable-auxiliaries";
const char kFlagEncryptRead[] = "encrypt-read";
const char kFlagEncryptWrite[] = "encrypt-write";
const char kFlagEncryptAuthenticatedRead[] = "encrypt-authenticated-read";
const char kFlagEncryptAuthenticatedWrite[] = "encrypt-authenticated-write";
const char kFlagPermissionRead[] = "permission-read";
const char kFlagPermissionWrite[] = "permission-write";
const char kFlagPermissionEncryptRead[] = "permission-encrypt-read";
const char kFlagPermissionEncryptWrite[] = "permission-encrypt-write";
const char kFlagPermissionAuthenticatedRead[] = "permission-authenticated-read";
const char kFlagPermissionAuthenticatedWrite[] =
    "permission-authenticated-write";
const char kFlagPermissionSecureRead[] = "permission-secure-read";
const char kFlagPermissionSecureWrite[] = "permission-secure-write";
}  // namespace bluetooth_gatt_characteristic

namespace bluetooth_gatt_descriptor {
// Bluetooth GATT Descriptor service identifiers. The service name is used
// only for descriptor objects hosted by bluetoothd.
const char kBluetoothGattDescriptorServiceName[] = "org.bluez";
const char kBluetoothGattDescriptorInterface[] = "org.bluez.GattDescriptor1";

// Bluetooth GATT Descriptor methods.
const char kReadValue[] = "ReadValue";
const char kWriteValue[] = "WriteValue";

// Possible keys for option dict used in ReadValue and WriteValue.
const char kOptionOffset[] = "offset";
const char kOptionDevice[] = "device";

// Bluetooth GATT Descriptor properties.
const char kUUIDProperty[] = "UUID";
const char kCharacteristicProperty[] = "Characteristic";
const char kValueProperty[] = "Value";
const char kFlagsProperty[] = "Flags";

// Possible values for Bluetooth GATT Descriptor "Flags" property.
const char kFlagRead[] = "read";
const char kFlagWrite[] = "write";
const char kFlagEncryptRead[] = "encrypt-read";
const char kFlagEncryptWrite[] = "encrypt-write";
const char kFlagEncryptAuthenticatedRead[] = "encrypt-authenticated-read";
const char kFlagEncryptAuthenticatedWrite[] = "encrypt-authenticated-write";
}  // namespace bluetooth_gatt_descriptor

namespace bluetooth_gatt_manager {
// Bluetooth GATT Manager service identifiers.
const char kBluetoothGattManagerServiceName[] = "org.bluez";
const char kBluetoothGattManagerInterface[] = "org.bluez.GattManager1";

// Bluetooth GATT Manager methods.
const char kRegisterApplication[] = "RegisterApplication";
const char kUnregisterApplication[] = "UnregisterApplication";
const char kRegisterService[] = "RegisterService";
const char kUnregisterService[] = "UnregisterService";

// Bluetooth GATT Manager errors.
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
const char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
}  // namespace bluetooth_gatt_manager

namespace bluetooth_gatt_service {
// Bluetooth GATT Service service identifiers. The service name is used
// only for service objects hosted by bluetoothd.
const char kBluetoothGattServiceServiceName[] = "org.bluez";
const char kBluetoothGattServiceInterface[] = "org.bluez.GattService1";

// Bluetooth GATT Service properties.
const char kUUIDProperty[] = "UUID";
const char kDeviceProperty[] = "Device";
const char kPrimaryProperty[] = "Primary";
const char kIncludesProperty[] = "Includes";
const char kCharacteristicsProperty[] = "Characteristics";

// Bluetooth GATT Service errors.
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorInProgress[] = "org.bluez.Error.InProgress";
const char kErrorInvalidValueLength[] = "org.bluez.Error.InvalidValueLength";
const char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
const char kErrorNotPaired[] = "org.bluez.Error.NotPaired";
const char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
const char kErrorNotPermitted[] = "org.bluez.Error.NotPermitted";
}  // namespace bluetooth_gatt_service

namespace bluetooth_input {
// Bluetooth Input service identifiers.
const char kBluetoothInputServiceName[] = "org.bluez";
const char kBluetoothInputInterface[] = "org.bluez.Input1";

// Bluetooth Input properties.
const char kReconnectModeProperty[] = "ReconnectMode";

// Bluetooth Input property values.
const char kNoneReconnectModeProperty[] = "none";
const char kHostReconnectModeProperty[] = "host";
const char kDeviceReconnectModeProperty[] = "device";
const char kAnyReconnectModeProperty[] = "any";
}  // namespace bluetooth_input

namespace bluetooth_media {
// Bluetooth Media service identifiers
const char kBluetoothMediaServiceName[] = "org.bluez";
const char kBluetoothMediaInterface[] = "org.bluez.Media1";

// Bluetooth Media methods
const char kRegisterEndpoint[] = "RegisterEndpoint";
const char kUnregisterEndpoint[] = "UnregisterEndpoint";
const char kRegisterPlayer[] = "RegisterPlayer";
const char kUnregisterPlayer[] = "UnregisterPlayer";

// Bluetooth Media errors
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
}  // namespace bluetooth_media

namespace bluetooth_media_endpoint {
// Bluetooth Media Endpoint service identifiers
const char kBluetoothMediaEndpointServiceName[] = "org.bluez";
const char kBluetoothMediaEndpointInterface[] = "org.bluez.MediaEndpoint1";

// Bluetooth Media Endpoint methods
const char kSetConfiguration[] = "SetConfiguration";
const char kSelectConfiguration[] = "SelectConfiguration";
const char kClearConfiguration[] = "ClearConfiguration";
const char kRelease[] = "Release";
}  // namespace bluetooth_media_endpoint

namespace bluetooth_media_transport {
// Bluetooth Media Transport service identifiers
const char kBluetoothMediaTransportServiceName[] = "org.bluez";
const char kBluetoothMediaTransportInterface[] = "org.bluez.MediaTransport1";

// Bluetooth Media Transport methods
const char kAcquire[] = "Acquire";
const char kTryAcquire[] = "TryAcquire";
const char kRelease[] = "Release";

// Bluetooth Media Transport property names.
const char kDeviceProperty[] = "Device";
const char kUUIDProperty[] = "UUID";
const char kCodecProperty[] = "Codec";
const char kConfigurationProperty[] = "Configuration";
const char kStateProperty[] = "State";
const char kDelayProperty[] = "Delay";
const char kVolumeProperty[] = "Volume";

// Possible states for the "State" property
const char kStateIdle[] = "idle";
const char kStatePending[] = "pending";
const char kStateActive[] = "active";

// Bluetooth Media Transport errors.
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
const char kErrorNotAvailable[] = "org.bluez.Error.NotAvailable";
}  // namespace bluetooth_media_transport

namespace bluez_object_manager {
// BlueZ daemon Object Manager service identifiers.
const char kBluezObjectManagerServiceName[] = "org.bluez";
const char kBluezObjectManagerServicePath[] = "/";
}  // namespace bluez_object_manager

namespace bluetooth_object_manager {
// Bluetooth daemon Object Manager service identifiers.
const char kBluetoothObjectManagerServiceName[] = "org.chromium.Bluetooth";
const char kBluetoothObjectManagerServicePath[] = "/";
}  // namespace bluetooth_object_manager

namespace newblue_object_manager {
// NewBlue daemon Object Manager service identifiers.
const char kNewblueObjectManagerServiceName[] = "org.chromium.Newblue";
const char kNewblueObjectManagerServicePath[] = "/";
}  // namespace newblue_object_manager

namespace bluetooth_profile_manager {
// Bluetooth Profile Manager service identifiers.
const char kBluetoothProfileManagerServiceName[] = "org.bluez";
const char kBluetoothProfileManagerServicePath[] = "/org/bluez";
const char kBluetoothProfileManagerInterface[] = "org.bluez.ProfileManager1";

// Bluetooth Profile Manager methods.
const char kRegisterProfile[] = "RegisterProfile";
const char kUnregisterProfile[] = "UnregisterProfile";

// Bluetooth Profile Manager option names.
const char kNameOption[] = "Name";
const char kServiceOption[] = "Service";
const char kRoleOption[] = "Role";
const char kChannelOption[] = "Channel";
const char kPSMOption[] = "PSM";
const char kRequireAuthenticationOption[] = "RequireAuthentication";
const char kRequireAuthorizationOption[] = "RequireAuthorization";
const char kAutoConnectOption[] = "AutoConnect";
const char kServiceRecordOption[] = "ServiceRecord";
const char kVersionOption[] = "Version";
const char kFeaturesOption[] = "Features";

// Bluetooth Profile Manager option values.
const char kClientRoleOption[] = "client";
const char kServerRoleOption[] = "server";

// Bluetooth Profile Manager errors.
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
const char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
}  // namespace bluetooth_profile_manager

namespace bluetooth_profile {
// Bluetooth Profile service identifiers.
const char kBluetoothProfileInterface[] = "org.bluez.Profile1";

// Bluetooth Profile methods.
const char kRelease[] = "Release";
const char kNewConnection[] = "NewConnection";
const char kRequestDisconnection[] = "RequestDisconnection";
const char kCancel[] = "Cancel";

// Bluetooth Profile property names.
const char kVersionProperty[] = "Version";
const char kFeaturesProperty[] = "Features";

// Bluetooth Profile errors.
const char kErrorRejected[] = "org.bluez.Error.Rejected";
const char kErrorCanceled[] = "org.bluez.Error.Canceled";
}  // namespace bluetooth_profile

namespace bluetooth_advertisement {
// Bluetooth LE Advertisement service identifiers.
const char kBluetoothAdvertisementServiceName[] = "org.bluez";
const char kBluetoothAdvertisementInterface[] =
    "org.bluez.LEAdvertisement1";

// Bluetooth Advertisement methods.
const char kRelease[] = "Release";

// Bluetooth Advertisement properties.
const char kManufacturerDataProperty[] = "ManufacturerData";
const char kServiceUUIDsProperty[] = "ServiceUUIDs";
const char kServiceDataProperty[] = "ServiceData";
const char kSolicitUUIDsProperty[] = "SolicitUUIDs";
const char kTypeProperty[] = "Type";
const char kIncludeTxPowerProperty[] = "IncludeTxPower";

// Possible values for the "Type" property.
const char kTypeBroadcast[] = "broadcast";
const char kTypePeripheral[] = "peripheral";

}  // namespace bluetooth_advertisement

namespace bluetooth_advertising_manager {
// Bluetooth LE Advertising Manager service identifiers.
const char kBluetoothAdvertisingManagerServiceName[] = "org.bluez";
const char kBluetoothAdvertisingManagerInterface[] =
    "org.bluez.LEAdvertisingManager1";

// Bluetooth LE Advertising Manager methods.
const char kRegisterAdvertisement[] = "RegisterAdvertisement";
const char kUnregisterAdvertisement[] = "UnregisterAdvertisement";
const char kSetAdvertisingIntervals[] = "SetAdvertisingIntervals";
const char kResetAdvertising[] = "ResetAdvertising";

// Bluetooth LE Advertising Manager properties.
const char kIsTXPowerSupportedProperty[] = "IsTXPowerSupported";

// Bluetooth LE Advertising Manager errors.
const char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
const char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorInvalidLength[] = "org.bluez.Error.InvalidLength";
}  // namespace bluetooth_advertising_manager

namespace mtpd {
const char kMtpdInterface[] = "org.chromium.Mtpd";
const char kMtpdServicePath[] = "/org/chromium/Mtpd";
const char kMtpdServiceName[] = "org.chromium.Mtpd";
const char kMtpdServiceError[] = "org.chromium.Mtpd.Error";

// Methods.
const char kEnumerateStorages[] = "EnumerateStorages";
const char kGetStorageInfo[] = "GetStorageInfo";
const char kGetStorageInfoFromDevice[] = "GetStorageInfoFromDevice";
const char kOpenStorage[] = "OpenStorage";
const char kCloseStorage[] = "CloseStorage";
const char kReadDirectoryEntryIds[] = "ReadDirectoryEntryIds";
const char kGetFileInfo[] = "GetFileInfo";
const char kReadFileChunk[] = "ReadFileChunk";
const char kCopyFileFromLocal[] = "CopyFileFromLocal";
const char kDeleteObject[] = "DeleteObject";
const char kRenameObject[] = "RenameObject";
const char kCreateDirectory[] = "CreateDirectory";

// Signals.
const char kMTPStorageAttached[] = "MTPStorageAttached";
const char kMTPStorageDetached[] = "MTPStorageDetached";

// For FileEntry struct:
const uint32_t kInvalidFileId = 0xffffffff;

// For OpenStorage method:
const char kReadOnlyMode[] = "ro";
const char kReadWriteMode[] = "rw";

// For GetFileInfo() method:
// The id of the root node in a storage, as defined by the PTP/MTP standards.
// Use this when referring to the root node in the context of GetFileInfo().
const uint32_t kRootFileId = 0;
}  // namespace mtpd

namespace system_clock {
const char kSystemClockInterface[] = "org.torproject.tlsdate";
const char kSystemClockServicePath[] = "/org/torproject/tlsdate";
const char kSystemClockServiceName[] = "org.torproject.tlsdate";

// Methods.
const char kSystemClockCanSet[] = "CanSetTime";
const char kSystemClockSet[] = "SetTime";
const char kSystemLastSyncInfo[] = "LastSyncInfo";

// Signals.
const char kSystemClockUpdated[] = "TimeUpdated";
}  // namespace system_clock

namespace cras {
const char kCrasServicePath[] = "/org/chromium/cras";
const char kCrasServiceName[] = "org.chromium.cras";
const char kCrasControlInterface[] = "org.chromium.cras.Control";

// Methods.
const char kSetOutputVolume[] = "SetOutputVolume";
const char kSetOutputNodeVolume[] = "SetOutputNodeVolume";
const char kSwapLeftRight[] = "SwapLeftRight";
const char kSetOutputMute[] = "SetOutputMute";
const char kSetOutputUserMute[] = "SetOutputUserMute";
const char kSetSuspendAudio[] = "SetSuspendAudio";
const char kSetInputGain[] = "SetInputGain";
const char kSetInputNodeGain[] = "SetInputNodeGain";
const char kSetInputMute[] = "SetInputMute";
const char kGetVolumeState[] = "GetVolumeState";
const char kGetDefaultOutputBufferSize[] = "GetDefaultOutputBufferSize";
const char kGetNodes[] = "GetNodes";
const char kSetActiveOutputNode[] = "SetActiveOutputNode";
const char kSetActiveInputNode[] = "SetActiveInputNode";
const char kSetHotwordModel[] = "SetHotwordModel";
const char kAddActiveOutputNode[] = "AddActiveOutputNode";
const char kAddActiveInputNode[] = "AddActiveInputNode";
const char kRemoveActiveOutputNode[] = "RemoveActiveOutputNode";
const char kRemoveActiveInputNode[] = "RemoveActiveInputNode";
const char kGetNumberOfActiveStreams[] = "GetNumberOfActiveStreams";
const char kGetNumberOfActiveInputStreams[] = "GetNumberOfActiveInputStreams";
const char kGetNumberOfActiveOutputStreams[] = "GetNumberOfActiveOutputStreams";
const char kIsAudioOutputActive[] = "IsAudioOutputActive";
const char kSetGlobalOutputChannelRemix[] = "SetGlobalOutputChannelRemix";
const char kGetSystemAecSupported[] = "GetSystemAecSupported";
const char kGetSystemAecGroupId[] = "GetSystemAecGroupId";

// Names of properties returned by GetNodes()
const char kIsInputProperty[] = "IsInput";
const char kIdProperty[] = "Id";
const char kTypeProperty[] = "Type";
const char kNameProperty[] = "Name";
const char kDeviceNameProperty[] = "DeviceName";
const char kActiveProperty[] = "Active";
const char kPluggedTimeProperty[] = "PluggedTime";
const char kMicPositionsProperty[] = "MicPositions";
const char kStableDeviceIdProperty[] = "StableDeviceId";
const char kStableDeviceIdNewProperty[] = "StableDeviceIdNew";

// Signals.
const char kOutputVolumeChanged[] = "OutputVolumeChanged";
const char kOutputMuteChanged[] = "OutputMuteChanged";
const char kOutputNodeVolumeChanged[] = "OutputNodeVolumeChanged";
const char kNodeLeftRightSwappedChanged[] = "NodeLeftRightSwappedChanged";
const char kInputGainChanged[] = "InputGainChanged";
const char kInputMuteChanged[] = "InputMuteChanged";
const char kNodesChanged[] = "NodesChanged";
const char kActiveOutputNodeChanged[] = "ActiveOutputNodeChanged";
const char kActiveInputNodeChanged[] = "ActiveInputNodeChanged";
const char kNumberOfActiveStreamsChanged[] = "NumberOfActiveStreamsChanged";
const char kAudioOutputActiveStateChanged[] = "AudioOutputActiveStateChanged";
const char kHotwordTriggered[] = "HotwordTriggered";
}  // namespace cras

namespace feedback {
const char kFeedbackServicePath[] = "/org/chromium/feedback";
const char kFeedbackServiceName[] = "org.chromium.feedback";

// Methods.
const char kSendFeedback[] = "SendFeedback";
}  // namespace feedback

namespace easy_unlock {
const char kEasyUnlockServicePath[] = "/org/chromium/EasyUnlock";
const char kEasyUnlockServiceName[] = "org.chromium.EasyUnlock";
const char kEasyUnlockServiceInterface[] = "org.chromium.EasyUnlock";

// Values supplied as enrcryption type to CreateSecureMessage and
// UnwrapSecureMessage methods.
const char kEncryptionTypeNone[] = "NONE";
const char kEncryptionTypeAES256CBC[] = "AES_256_CBC";

// Values supplied as signature type to CreateSecureMessage and
// UnwrapSecureMessage methods.
const char kSignatureTypeECDSAP256SHA256[] = "ECDSA_P256_SHA256";
const char kSignatureTypeHMACSHA256[] = "HMAC_SHA256";

// Values supplied as key algorithm to WrapPublicKey method.
const char kKeyAlgorithmRSA[] = "RSA";
const char kKeyAlgorithmECDSA[] = "ECDSA";

// Methods
const char kPerformECDHKeyAgreementMethod[] = "PerformECDHKeyAgreement";
const char kWrapPublicKeyMethod[] = "WrapPublicKey";
const char kGenerateEcP256KeyPairMethod[] = "GenerateEcP256KeyPair";
const char kCreateSecureMessageMethod[] = "CreateSecureMessage";
const char kUnwrapSecureMessageMethod[] = "UnwrapSecureMessage";
}  // namespace easy_unlock

namespace arc_oemcrypto {
const char kArcOemCryptoServiceInterface[] = "org.chromium.ArcOemCrypto";
const char kArcOemCryptoServiceName[] = "org.chromium.ArcOemCrypto";
const char kArcOemCryptoServicePath[] = "/org/chromium/ArcOemCrypto";
// Methods
const char kBootstrapMojoConnection[] = "BootstrapMojoConnection";
}  // namespace arc_oemcrypto

namespace midis {
constexpr char kMidisServiceName[] = "org.chromium.Midis";
constexpr char kMidisServicePath[] = "/org/chromium/Midis";
constexpr char kMidisInterfaceName[] = "org.chromium.Midis";
// Methods
constexpr char kBootstrapMojoConnectionMethod[] = "BootstrapMojoConnection";
}  // namespace midis

namespace ml {
constexpr char kMachineLearningServiceName[] = "org.chromium.MachineLearning";
constexpr char kMachineLearningServicePath[] = "/org/chromium/MachineLearning";
constexpr char kMachineLearningInterfaceName[] = "org.chromium.MachineLearning";
// Methods
constexpr char kBootstrapMojoConnectionMethod[] = "BootstrapMojoConnection";
// Token identifying the primordial Mojo pipe passed to BootstrapMojoConnection.
constexpr char kBootstrapMojoConnectionChannelToken[] = "ml-service-bootstrap";
}  // namespace ml

namespace virtual_file_provider {
constexpr char kVirtualFileProviderServiceName[] =
    "org.chromium.VirtualFileProvider";
constexpr char kVirtualFileProviderServicePath[] =
    "/org/chromium/VirtualFileProvider";
constexpr char kVirtualFileProviderInterface[] =
    "org.chromium.VirtualFileProvider";
// Methods
constexpr char kOpenFileMethod[] = "OpenFile";
}  // namespace virtual_file_provider

namespace crosdns {
constexpr char kCrosDnsServiceName[] = "org.chromium.CrosDns";
constexpr char kCrosDnsServicePath[] = "/org/chromium/CrosDns";
constexpr char kCrosDnsInterfaceName[] = "org.chromium.CrosDns";
// Methods
constexpr char kSetHostnameIpMappingMethod[] = "SetHostnameIpMapping";
constexpr char kRemoveHostnameIpMappingMethod[] = "RemoveHostnameIpMapping";
}

namespace arc {

namespace keymaster {
constexpr char kArcKeymasterServiceName[] = "org.chromium.ArcKeymaster";
constexpr char kArcKeymasterServicePath[] = "/org/chromium/ArcKeymaster";
constexpr char kArcKeymasterInterfaceName[] = "org.chromium.ArcKeymaster";
// Methods
constexpr char kBootstrapMojoConnectionMethod[] = "BootstrapMojoConnection";
}  // namespace keymaster

namespace obb_mounter {
// D-Bus service constants.
constexpr char kArcObbMounterInterface[] =
    "org.chromium.ArcObbMounterInterface";
constexpr char kArcObbMounterServicePath[] = "/org/chromium/ArcObbMounter";
constexpr char kArcObbMounterServiceName[] = "org.chromium.ArcObbMounter";

// Method names.
constexpr char kMountObbMethod[] = "MountObb";
constexpr char kUnmountObbMethod[] = "UnmountObb";
}  // namespace obb_mounter

namespace appfuse {
// D-Bus service constants.
constexpr char kArcAppfuseProviderInterface[] =
    "org.chromium.ArcAppfuseProvider";
constexpr char kArcAppfuseProviderServicePath[] =
    "/org/chromium/ArcAppfuseProvider";
constexpr char kArcAppfuseProviderServiceName[] =
    "org.chromium.ArcAppfuseProvider";

// Method names.
constexpr char kMountMethod[] = "Mount";
constexpr char kUnmountMethod[] = "Unmount";
constexpr char kOpenFileMethod[] = "OpenFile";
}  // namespace appfuse

}  // namespace arc

namespace anomaly_detector {
const char kAnomalyEventServiceName[] = "org.chromium.AnomalyEventService";
const char kAnomalyEventServicePath[] = "/org/chromium/AnomalyEventService";
const char kAnomalyEventServiceInterface[] =
    "org.chromium.AnomalyEventServiceInterface";
const char kAnomalyEventSignalName[] = "AnomalyEvent";
}  // namespace anomaly_detector

namespace libvda {
const char kLibvdaServiceInterface[] = "org.chromium.LibvdaService";
const char kLibvdaServiceName[] = "org.chromium.LibvdaService";
const char kLibvdaServicePath[] = "/org/chromium/LibvdaService";

// Method names.
const char kProvideMojoConnectionMethod[] = "ProvideMojoConnection";
}  // namespace libvda

namespace printing {
constexpr char kCupsProxyDaemonName[] = "org.chromium.CupsProxyDaemon";
constexpr char kCupsProxyDaemonPath[] = "/org/chromium/CupsProxyDaemon";
constexpr char kCupsProxyDaemonInterface[] = "org.chromium.CupsProxyDaemon";

// Method names.
constexpr char kBootstrapMojoConnectionMethod[] = "BootstrapMojoConnection";

// Token identifying the primordial Mojo pipe passed to BootstrapMojoConnection.
constexpr char kBootstrapMojoConnectionChannelToken[] =
    "cups-proxy-service-bootstrap";
}  // namespace printing

namespace arc_camera {
constexpr char kArcCameraServiceName[] = "org.chromium.ArcCamera";
constexpr char kArcCameraServicePath[] = "/org/chromium/ArcCamera";
constexpr char kArcCameraServiceInterface[] = "org.chromium.ArcCamera";

// Method names.
constexpr char kStartServiceMethod[] = "StartService";
}  // namespace arc_camera

#endif  // SYSTEM_API_DBUS_SERVICE_CONSTANTS_H_
