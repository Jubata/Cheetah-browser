// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_win.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_win.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_win.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"

namespace device {

BluetoothRemoteGattCharacteristicWin::BluetoothRemoteGattCharacteristicWin(
    BluetoothRemoteGattServiceWin* parent_service,
    BTH_LE_GATT_CHARACTERISTIC* characteristic_info,
    scoped_refptr<base::SequencedTaskRunner>& ui_task_runner)
    : parent_service_(parent_service),
      characteristic_info_(characteristic_info),
      ui_task_runner_(ui_task_runner),
      characteristic_added_notified_(false),
      characteristic_value_read_or_write_in_progress_(false),
      gatt_event_handle_(nullptr),
      discovery_pending_count_(0),
      weak_ptr_factory_(this) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(parent_service_);
  DCHECK(characteristic_info_);

  task_manager_ =
      parent_service_->GetWinAdapter()->GetWinBluetoothTaskManager();
  DCHECK(task_manager_);
  characteristic_uuid_ =
      BluetoothTaskManagerWin::BluetoothLowEnergyUuidToBluetoothUuid(
          characteristic_info_->CharacteristicUuid);
  characteristic_identifier_ =
      parent_service_->GetIdentifier() + "_" +
      std::to_string(characteristic_info_->AttributeHandle);
  Update();
}

BluetoothRemoteGattCharacteristicWin::~BluetoothRemoteGattCharacteristicWin() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  ClearIncludedDescriptors();

  if (gatt_event_handle_ != nullptr) {
    task_manager_->PostUnregisterGattCharacteristicValueChangedEvent(
        gatt_event_handle_);
    gatt_event_handle_ = nullptr;
  }
  parent_service_->GetWinAdapter()->NotifyGattCharacteristicRemoved(this);

  if (!read_characteristic_value_callbacks_.first.is_null()) {
    DCHECK(!read_characteristic_value_callbacks_.second.is_null());
    read_characteristic_value_callbacks_.second.Run(
        BluetoothRemoteGattService::GATT_ERROR_FAILED);
  }

  if (!write_characteristic_value_callbacks_.first.is_null()) {
    DCHECK(!write_characteristic_value_callbacks_.second.is_null());
    write_characteristic_value_callbacks_.second.Run(
        BluetoothRemoteGattService::GATT_ERROR_FAILED);
  }

  // Clear pending StartNotifySession callbacks.
  for (const auto& callback : start_notify_session_callbacks_)
    callback.second.Run(BluetoothRemoteGattService::GATT_ERROR_FAILED);
}

std::string BluetoothRemoteGattCharacteristicWin::GetIdentifier() const {
  return characteristic_identifier_;
}

BluetoothUUID BluetoothRemoteGattCharacteristicWin::GetUUID() const {
  return characteristic_uuid_;
}

std::vector<uint8_t>& BluetoothRemoteGattCharacteristicWin::GetValue() const {
  return const_cast<std::vector<uint8_t>&>(characteristic_value_);
}

BluetoothRemoteGattService* BluetoothRemoteGattCharacteristicWin::GetService()
    const {
  return parent_service_;
}

BluetoothRemoteGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicWin::GetProperties() const {
  BluetoothRemoteGattCharacteristic::Properties properties = PROPERTY_NONE;

  if (characteristic_info_->IsBroadcastable)
    properties = properties | PROPERTY_BROADCAST;
  if (characteristic_info_->IsReadable)
    properties = properties | PROPERTY_READ;
  if (characteristic_info_->IsWritableWithoutResponse)
    properties = properties | PROPERTY_WRITE_WITHOUT_RESPONSE;
  if (characteristic_info_->IsWritable)
    properties = properties | PROPERTY_WRITE;
  if (characteristic_info_->IsNotifiable)
    properties = properties | PROPERTY_NOTIFY;
  if (characteristic_info_->IsIndicatable)
    properties = properties | PROPERTY_INDICATE;
  if (characteristic_info_->IsSignedWritable)
    properties = properties | PROPERTY_AUTHENTICATED_SIGNED_WRITES;
  if (characteristic_info_->HasExtendedProperties)
    properties = properties | PROPERTY_EXTENDED_PROPERTIES;

  // TODO(crbug.com/589304): Information about PROPERTY_RELIABLE_WRITE and
  // PROPERTY_WRITABLE_AUXILIARIES is not available in characteristic_info_
  // (BTH_LE_GATT_CHARACTERISTIC).

  return properties;
}

BluetoothRemoteGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicWin::GetPermissions() const {
  BluetoothRemoteGattCharacteristic::Permissions permissions = PERMISSION_NONE;

  if (characteristic_info_->IsReadable)
    permissions = permissions | PERMISSION_READ;
  if (characteristic_info_->IsWritable)
    permissions = permissions | PERMISSION_WRITE;

  return permissions;
}

bool BluetoothRemoteGattCharacteristicWin::IsNotifying() const {
  return gatt_event_handle_ != nullptr;
}

std::vector<BluetoothRemoteGattDescriptor*>
BluetoothRemoteGattCharacteristicWin::GetDescriptors() const {
  std::vector<BluetoothRemoteGattDescriptor*> descriptors;
  for (const auto& descriptor : included_descriptors_)
    descriptors.push_back(descriptor.second.get());
  return descriptors;
}

BluetoothRemoteGattDescriptor*
BluetoothRemoteGattCharacteristicWin::GetDescriptor(
    const std::string& identifier) const {
  GattDescriptorMap::const_iterator it = included_descriptors_.find(identifier);
  if (it != included_descriptors_.end())
    return it->second.get();
  return nullptr;
}

void BluetoothRemoteGattCharacteristicWin::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  if (!characteristic_info_.get()->IsReadable) {
    error_callback.Run(BluetoothRemoteGattService::GATT_ERROR_NOT_PERMITTED);
    return;
  }

  if (characteristic_value_read_or_write_in_progress_) {
    error_callback.Run(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS);
    return;
  }

  characteristic_value_read_or_write_in_progress_ = true;
  read_characteristic_value_callbacks_ =
      std::make_pair(callback, error_callback);
  task_manager_->PostReadGattCharacteristicValue(
      parent_service_->GetServicePath(), characteristic_info_.get(),
      base::Bind(&BluetoothRemoteGattCharacteristicWin::
                     OnReadRemoteCharacteristicValueCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothRemoteGattCharacteristicWin::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  if (!characteristic_info_.get()->IsWritable) {
    error_callback.Run(BluetoothRemoteGattService::GATT_ERROR_NOT_PERMITTED);
    return;
  }

  if (characteristic_value_read_or_write_in_progress_) {
    error_callback.Run(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS);
    return;
  }

  characteristic_value_read_or_write_in_progress_ = true;
  write_characteristic_value_callbacks_ =
      std::make_pair(callback, error_callback);
  task_manager_->PostWriteGattCharacteristicValue(
      parent_service_->GetServicePath(), characteristic_info_.get(), value,
      base::Bind(&BluetoothRemoteGattCharacteristicWin::
                     OnWriteRemoteCharacteristicValueCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothRemoteGattCharacteristicWin::Update() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  ++discovery_pending_count_;
  task_manager_->PostGetGattIncludedDescriptors(
      parent_service_->GetServicePath(), characteristic_info_.get(),
      base::Bind(&BluetoothRemoteGattCharacteristicWin::
                     OnGetIncludedDescriptorsCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

uint16_t BluetoothRemoteGattCharacteristicWin::GetAttributeHandle() const {
  return characteristic_info_->AttributeHandle;
}

void BluetoothRemoteGattCharacteristicWin::SubscribeToNotifications(
    BluetoothRemoteGattDescriptor* ccc_descriptor,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  task_manager_->PostRegisterGattCharacteristicValueChangedEvent(
      parent_service_->GetServicePath(), characteristic_info_.get(),
      static_cast<BluetoothRemoteGattDescriptorWin*>(ccc_descriptor)
          ->GetWinDescriptorInfo(),
      base::Bind(
          &BluetoothRemoteGattCharacteristicWin::GattEventRegistrationCallback,
          weak_ptr_factory_.GetWeakPtr(), callback, error_callback),
      base::Bind(&BluetoothRemoteGattCharacteristicWin::
                     OnGattCharacteristicValueChanged,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothRemoteGattCharacteristicWin::UnsubscribeFromNotifications(
    BluetoothRemoteGattDescriptor* ccc_descriptor,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // TODO(crbug.com/735828): Implement this method.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void BluetoothRemoteGattCharacteristicWin::OnGetIncludedDescriptorsCallback(
    std::unique_ptr<BTH_LE_GATT_DESCRIPTOR> descriptors,
    uint16_t num,
    HRESULT hr) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  UpdateIncludedDescriptors(descriptors.get(), num);
  if (!characteristic_added_notified_) {
    characteristic_added_notified_ = true;
    parent_service_->GetWinAdapter()->NotifyGattCharacteristicAdded(this);
  }

  // Report discovery complete.
  if (--discovery_pending_count_ == 0)
    parent_service_->GattCharacteristicDiscoveryComplete(this);
}

void BluetoothRemoteGattCharacteristicWin::UpdateIncludedDescriptors(
    PBTH_LE_GATT_DESCRIPTOR descriptors,
    uint16_t num) {
  if (num == 0) {
    included_descriptors_.clear();
    return;
  }

  // First, remove descriptors that no longer exist.
  std::vector<std::string> to_be_removed;
  for (const auto& d : included_descriptors_) {
    if (!DoesDescriptorExist(descriptors, num, d.second.get()))
      to_be_removed.push_back(d.second->GetIdentifier());
  }
  for (auto id : to_be_removed) {
    included_descriptors_[id].reset();
    included_descriptors_.erase(id);
  }

  // Return if no new descriptors have been added.
  if (included_descriptors_.size() == num)
    return;

  // Add new descriptors.
  for (uint16_t i = 0; i < num; i++) {
    if (!IsDescriptorDiscovered(descriptors[i].DescriptorUuid,
                                descriptors[i].AttributeHandle)) {
      PBTH_LE_GATT_DESCRIPTOR win_descriptor_info =
          new BTH_LE_GATT_DESCRIPTOR();
      *win_descriptor_info = descriptors[i];
      BluetoothRemoteGattDescriptorWin* descriptor =
          new BluetoothRemoteGattDescriptorWin(this, win_descriptor_info,
                                               ui_task_runner_);
      included_descriptors_[descriptor->GetIdentifier()] =
          base::WrapUnique(descriptor);
    }
  }
}

bool BluetoothRemoteGattCharacteristicWin::IsDescriptorDiscovered(
    BTH_LE_UUID& uuid,
    uint16_t attribute_handle) {
  BluetoothUUID bt_uuid =
      BluetoothTaskManagerWin::BluetoothLowEnergyUuidToBluetoothUuid(uuid);
  for (const auto& d : included_descriptors_) {
    if (bt_uuid == d.second->GetUUID() &&
        attribute_handle == d.second->GetAttributeHandle()) {
      return true;
    }
  }
  return false;
}

bool BluetoothRemoteGattCharacteristicWin::DoesDescriptorExist(
    PBTH_LE_GATT_DESCRIPTOR descriptors,
    uint16_t num,
    BluetoothRemoteGattDescriptorWin* descriptor) {
  for (uint16_t i = 0; i < num; i++) {
    BluetoothUUID uuid =
        BluetoothTaskManagerWin::BluetoothLowEnergyUuidToBluetoothUuid(
            descriptors[i].DescriptorUuid);
    if (descriptor->GetUUID() == uuid &&
        descriptor->GetAttributeHandle() == descriptors[i].AttributeHandle) {
      return true;
    }
  }
  return false;
}

void BluetoothRemoteGattCharacteristicWin::
    OnReadRemoteCharacteristicValueCallback(
        std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC_VALUE> value,
        HRESULT hr) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  std::pair<ValueCallback, ErrorCallback> callbacks;
  callbacks.swap(read_characteristic_value_callbacks_);
  if (FAILED(hr)) {
    callbacks.second.Run(HRESULTToGattErrorCode(hr));
  } else {
    characteristic_value_.clear();
    for (ULONG i = 0; i < value->DataSize; i++)
      characteristic_value_.push_back(value->Data[i]);

    callbacks.first.Run(characteristic_value_);
  }
  characteristic_value_read_or_write_in_progress_ = false;
}

void BluetoothRemoteGattCharacteristicWin::
    OnWriteRemoteCharacteristicValueCallback(HRESULT hr) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  std::pair<base::Closure, ErrorCallback> callbacks;
  callbacks.swap(write_characteristic_value_callbacks_);
  if (FAILED(hr)) {
    callbacks.second.Run(HRESULTToGattErrorCode(hr));
  } else {
    callbacks.first.Run();
  }
  characteristic_value_read_or_write_in_progress_ = false;
}

BluetoothRemoteGattService::GattErrorCode
BluetoothRemoteGattCharacteristicWin::HRESULTToGattErrorCode(HRESULT hr) {
  if (HRESULT_FROM_WIN32(ERROR_INVALID_USER_BUFFER) == hr)
    return BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH;

  switch (hr) {
    case E_BLUETOOTH_ATT_READ_NOT_PERMITTED:
    case E_BLUETOOTH_ATT_WRITE_NOT_PERMITTED:
      return BluetoothRemoteGattService::GATT_ERROR_NOT_PERMITTED;
    case E_BLUETOOTH_ATT_UNKNOWN_ERROR:
      return BluetoothRemoteGattService::GATT_ERROR_UNKNOWN;
    case E_BLUETOOTH_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH:
      return BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH;
    case E_BLUETOOTH_ATT_REQUEST_NOT_SUPPORTED:
      return BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED;
    default:
      return BluetoothRemoteGattService::GATT_ERROR_FAILED;
  }
}

void BluetoothRemoteGattCharacteristicWin::OnGattCharacteristicValueChanged(
    std::unique_ptr<std::vector<uint8_t>> new_value) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  characteristic_value_.assign(new_value->begin(), new_value->end());
  parent_service_->GetWinAdapter()->NotifyGattCharacteristicValueChanged(
      this, characteristic_value_);
}

void BluetoothRemoteGattCharacteristicWin::GattEventRegistrationCallback(
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    BLUETOOTH_GATT_EVENT_HANDLE event_handle,
    HRESULT hr) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  if (SUCCEEDED(hr)) {
    gatt_event_handle_ = event_handle;
    callback.Run();
  } else {
    error_callback.Run(HRESULTToGattErrorCode(hr));
  }
}

void BluetoothRemoteGattCharacteristicWin::ClearIncludedDescriptors() {
  // Explicitly reset to null to ensure that calling GetDescriptor() on the
  // removed descriptor in GattDescriptorRemoved() returns null.
  for (auto& entry : included_descriptors_)
    entry.second.reset();
  included_descriptors_.clear();
}

}  // namespace device.
