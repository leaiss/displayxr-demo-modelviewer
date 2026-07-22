// Copyright 2026, The DisplayXR Project and its contributors
// SPDX-License-Identifier: Apache-2.0
//
// EyePoseHud — a tiny debug overlay for the Linux model viewer that shows the
// per-eye pose the runtime returns from xrLocateViews, drawn on-panel so you can
// watch whether it tracks your head (#778: "the bug is it's not changing").
//
// Fully self-contained: an embedded 5x7 bitmap font is CPU-rasterized into an
// RGBA8 panel, uploaded to a device-local image, and blitted into each eye tile
// AFTER the model has rendered (reusing the same TRANSFER_DST↔COLOR_ATTACHMENT
// barrier + vkCmdBlitImage pattern as ModelRenderer::renderEye). No text stack,
// no font file, no extra composition layer — so it works on the native-VK path.
// Non-fatal: if init fails, render() is a no-op and the app runs without the HUD.
#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <vector>

struct EyePoseHud {
    // Panel size in source pixels (blitted 1:1 into each eye tile's top-left).
    static constexpr uint32_t kPanelW = 640;
    static constexpr uint32_t kPanelH = 208;

    bool init(VkPhysicalDevice physicalDevice,
              VkDevice device,
              VkQueue queue,
              uint32_t queueFamilyIndex);

    // Rasterize `lines` and blit the panel into each eye tile. `tileOffsets` are
    // the per-eye (x,y) pixel offsets into the swapchain image (same values the
    // app passes to renderEye); drawn at the same tile-relative spot in every eye
    // → zero disparity → the readout sits at screen depth. Swapchain image must be
    // in COLOR_ATTACHMENT_OPTIMAL on entry (renderEye's final layout) and is left
    // that way on exit. No-op if not ready.
    void render(VkImage swapchainImage,
                const std::vector<std::pair<uint32_t, uint32_t>>& tileOffsets,
                const std::vector<std::string>& lines);

    void destroy();

private:
    bool ready_ = false;
    VkPhysicalDevice phys_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    VkCommandPool cmdPool_ = VK_NULL_HANDLE;

    VkImage image_ = VK_NULL_HANDLE;          // device-local RGBA8 panel (blit src)
    VkDeviceMemory imageMem_ = VK_NULL_HANDLE;
    VkBuffer staging_ = VK_NULL_HANDLE;        // host-visible upload buffer
    VkDeviceMemory stagingMem_ = VK_NULL_HANDLE;
    void* stagingMapped_ = nullptr;

    std::vector<uint8_t> pixels_;              // CPU RGBA8 scratch (kPanelW*kPanelH*4)

    void rasterize(const std::vector<std::string>& lines);
};
