// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_VIDEO_GPU_ARC_VIDEO_SERVICE_HOST_H_
#define CHROME_BROWSER_CHROMEOS_ARC_VIDEO_GPU_ARC_VIDEO_SERVICE_HOST_H_

#include "base/macros.h"
#include "components/arc/common/video.mojom.h"
#include "components/arc/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class takes requests for accessing the VideoAcceleratorFactory, from
// which video decode (or encode) accelerators could be created.
//
// This class runs in the browser process, while the created instances of
// VideoDecodeAccelerator or VideoEncodeAccelerator run in the GPU process.
//
// Lives on the UI thread.
class GpuArcVideoServiceHost : public KeyedService,
                               public ConnectionObserver<mojom::VideoInstance>,
                               public mojom::VideoHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static GpuArcVideoServiceHost* GetForBrowserContext(
      content::BrowserContext* context);

  GpuArcVideoServiceHost(content::BrowserContext* context,
                         ArcBridgeService* bridge_service);
  ~GpuArcVideoServiceHost() override;

  // ConnectionObserver<mojom::VideoInstance>::Observer implementation.
  void OnConnectionReady() override;

  // arc::mojom::VideoHost implementation.
  void OnBootstrapVideoAcceleratorFactory(
      OnBootstrapVideoAcceleratorFactoryCallback callback) override;

 private:
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  mojo::Binding<mojom::VideoHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(GpuArcVideoServiceHost);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_VIDEO_GPU_ARC_VIDEO_SERVICE_HOST_H_
