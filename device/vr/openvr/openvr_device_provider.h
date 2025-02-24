// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_DEVICE_PROVIDER_H
#define DEVICE_VR_OPENVR_DEVICE_PROVIDER_H

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_export.h"

namespace vr {
class IVRSystem;
}  // namespace vr

namespace device {

class OpenVRDevice;

class DEVICE_VR_EXPORT OpenVRDeviceProvider : public VRDeviceProvider {
 public:
  OpenVRDeviceProvider();
  ~OpenVRDeviceProvider() override;

  void GetDevices(std::vector<VRDevice*>* devices) override;
  void Initialize() override;

 private:
  bool initialized_;
  vr::IVRSystem* vr_system_;
  std::unique_ptr<OpenVRDevice> device_;

  DISALLOW_COPY_AND_ASSIGN(OpenVRDeviceProvider);
};

}  // namespace device

#endif  // DEVICE_VR_OPENVR_DEVICE_PROVIDER_H
