// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GEOLOCATION_LOCATION_PROVIDER_H_
#define DEVICE_GEOLOCATION_LOCATION_PROVIDER_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "device/geolocation/geolocation_export.h"
#include "device/geolocation/public/interfaces/geoposition.mojom.h"

namespace device {

// The interface for providing location information.
class DEVICE_GEOLOCATION_EXPORT LocationProvider {
 public:
  virtual ~LocationProvider() {}

  typedef base::Callback<void(const LocationProvider*,
                              const mojom::Geoposition&)>
      LocationProviderUpdateCallback;

  // This callback will be used to notify when a new Geoposition becomes
  // available.
  virtual void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) = 0;

  // StartProvider maybe called multiple times, e.g. to alter the
  // |high_accuracy| setting.
  virtual void StartProvider(bool high_accuracy) = 0;

  // Stops the provider from sending more requests.
  // Important: a LocationProvider may be instantiated and StartProvider() may
  // be called before the user has granted permission via OnPermissionGranted().
  // This is to allow underlying providers to warm up, load their internal
  // libraries, etc. No |LocationProviderUpdateCallback| can be run and no
  // network requests can be done until OnPermissionGranted() has been called.
  virtual void StopProvider() = 0;

  // Gets the current best position estimate.
  virtual const mojom::Geoposition& GetPosition() = 0;

  // Called everytime permission is granted to a page for using geolocation.
  // This may either be through explicit user action (e.g. responding to the
  // infobar prompt) or inferred from a persisted site permission.
  // Note: See |StartProvider()| for more information.
  virtual void OnPermissionGranted() = 0;
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_LOCATION_PROVIDER_H_
