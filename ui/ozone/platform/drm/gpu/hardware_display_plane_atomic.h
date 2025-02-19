// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_ATOMIC_H_
#define UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_ATOMIC_H_

#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"

#include <xf86drmMode.h>

namespace ui {

class CrtcController;
class DrmDevice;

class HardwareDisplayPlaneAtomic : public HardwareDisplayPlane {
 public:
  HardwareDisplayPlaneAtomic(uint32_t plane_id, uint32_t possible_crtcs);
  ~HardwareDisplayPlaneAtomic() override;

  bool SetPlaneData(drmModePropertySet* property_set,
                    uint32_t crtc_id,
                    uint32_t framebuffer,
                    const gfx::Rect& crtc_rect,
                    const gfx::Rect& src_rect);

  // HardwareDisplayPlane:
  bool Initialize(DrmDevice* drm,
                  const std::vector<uint32_t>& formats) override;
  bool IsSupportedFormat(uint32_t format) const override;

  void set_crtc(CrtcController* crtc) { crtc_ = crtc; }
  CrtcController* crtc() const { return crtc_; }

 private:
  struct Property {
    Property();
    bool Initialize(DrmDevice* drm,
                    const char* name,
                    const ScopedDrmObjectPropertyPtr& plane_properties);
    uint32_t id = 0;
  };

  Property crtc_prop_;
  Property fb_prop_;
  Property crtc_x_prop_;
  Property crtc_y_prop_;
  Property crtc_w_prop_;
  Property crtc_h_prop_;
  Property src_x_prop_;
  Property src_y_prop_;
  Property src_w_prop_;
  Property src_h_prop_;
  CrtcController* crtc_ = nullptr;
  std::vector<uint32_t> supported_formats_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_ATOMIC_H_
