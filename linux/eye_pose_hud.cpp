// Copyright 2026, The DisplayXR Project and its contributors
// SPDX-License-Identifier: Apache-2.0
#include "eye_pose_hud.h"

#include <cstring>
#include <cstdio>

// ── Embedded 5x7 bitmap font ────────────────────────────────────────────────
// Each glyph is 7 rows; bit 7 (0x80) is the leftmost pixel. Written as binary
// literals so the shape is visible (and verifiable) right here in the source.
namespace {

struct Glyph { char c; uint8_t rows[7]; };

const Glyph kGlyphs[] = {
  {'0', {0b01110000,0b10001000,0b10011000,0b10101000,0b11001000,0b10001000,0b01110000}},
  {'1', {0b00100000,0b01100000,0b00100000,0b00100000,0b00100000,0b00100000,0b01110000}},
  {'2', {0b01110000,0b10001000,0b00001000,0b00110000,0b01000000,0b10000000,0b11111000}},
  {'3', {0b11110000,0b00001000,0b00010000,0b00110000,0b00001000,0b10001000,0b01110000}},
  {'4', {0b00010000,0b00110000,0b01010000,0b10010000,0b11111000,0b00010000,0b00010000}},
  {'5', {0b11111000,0b10000000,0b11110000,0b00001000,0b00001000,0b10001000,0b01110000}},
  {'6', {0b01110000,0b10000000,0b10000000,0b11110000,0b10001000,0b10001000,0b01110000}},
  {'7', {0b11111000,0b00001000,0b00010000,0b00100000,0b01000000,0b01000000,0b01000000}},
  {'8', {0b01110000,0b10001000,0b10001000,0b01110000,0b10001000,0b10001000,0b01110000}},
  {'9', {0b01110000,0b10001000,0b10001000,0b01111000,0b00001000,0b00001000,0b01110000}},
  {'A', {0b01110000,0b10001000,0b10001000,0b11111000,0b10001000,0b10001000,0b10001000}},
  {'C', {0b01110000,0b10001000,0b10000000,0b10000000,0b10000000,0b10001000,0b01110000}},
  {'D', {0b11100000,0b10010000,0b10001000,0b10001000,0b10001000,0b10010000,0b11100000}},
  {'E', {0b11111000,0b10000000,0b10000000,0b11110000,0b10000000,0b10000000,0b11111000}},
  {'G', {0b01110000,0b10001000,0b10000000,0b10111000,0b10001000,0b10001000,0b01110000}},
  {'I', {0b01110000,0b00100000,0b00100000,0b00100000,0b00100000,0b00100000,0b01110000}},
  {'K', {0b10001000,0b10010000,0b10100000,0b11000000,0b10100000,0b10010000,0b10001000}},
  {'L', {0b10000000,0b10000000,0b10000000,0b10000000,0b10000000,0b10000000,0b11111000}},
  {'W', {0b10001000,0b10001000,0b10001000,0b10101000,0b10101000,0b11011000,0b10001000}},
  {'M', {0b10001000,0b11011000,0b10101000,0b10101000,0b10001000,0b10001000,0b10001000}},
  {'N', {0b10001000,0b11001000,0b10101000,0b10011000,0b10001000,0b10001000,0b10001000}},
  {'O', {0b01110000,0b10001000,0b10001000,0b10001000,0b10001000,0b10001000,0b01110000}},
  {'P', {0b11110000,0b10001000,0b10001000,0b11110000,0b10000000,0b10000000,0b10000000}},
  {'R', {0b11110000,0b10001000,0b10001000,0b11110000,0b10100000,0b10010000,0b10001000}},
  {'S', {0b01110000,0b10001000,0b10000000,0b01110000,0b00001000,0b10001000,0b01110000}},
  {'T', {0b11111000,0b00100000,0b00100000,0b00100000,0b00100000,0b00100000,0b00100000}},
  {'V', {0b10001000,0b10001000,0b10001000,0b10001000,0b10001000,0b01010000,0b00100000}},
  {'X', {0b10001000,0b10001000,0b01010000,0b00100000,0b01010000,0b10001000,0b10001000}},
  {'Y', {0b10001000,0b10001000,0b01010000,0b00100000,0b00100000,0b00100000,0b00100000}},
  {'Z', {0b11111000,0b00001000,0b00010000,0b00100000,0b01000000,0b10000000,0b11111000}},
  {'.', {0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b01100000,0b01100000}},
  {'-', {0b00000000,0b00000000,0b00000000,0b11110000,0b00000000,0b00000000,0b00000000}},
  {'+', {0b00000000,0b00100000,0b00100000,0b11111000,0b00100000,0b00100000,0b00000000}},
  {':', {0b00000000,0b01100000,0b01100000,0b00000000,0b01100000,0b01100000,0b00000000}},
  {'(', {0b00100000,0b01000000,0b10000000,0b10000000,0b10000000,0b01000000,0b00100000}},
  {')', {0b00100000,0b00010000,0b00001000,0b00001000,0b00001000,0b00010000,0b00100000}},
};

const uint8_t* glyphFor(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (const auto& g : kGlyphs)
        if (g.c == c) return g.rows;
    return nullptr;  // space / unknown → blank cell
}

constexpr uint32_t kScale   = 3;         // pixel size of one font dot
constexpr uint32_t kGlyphW  = 5;         // glyph is 5 columns wide
constexpr uint32_t kCharAdv = (kGlyphW + 1) * kScale;   // advance incl. 1-dot gap = 18
constexpr uint32_t kLineAdv = (7 + 2) * kScale;         // 7 rows + 2-dot gap = 27
constexpr uint32_t kMargin  = 12;

uint32_t findMemoryType(VkPhysicalDevice pd, uint32_t bits, VkMemoryPropertyFlags want) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want)
            return i;
    return UINT32_MAX;
}

}  // namespace

// ── CPU rasterization ───────────────────────────────────────────────────────
void EyePoseHud::rasterize(const std::vector<std::string>& lines) {
    const uint32_t W = kPanelW, H = kPanelH;
    uint8_t* px = pixels_.data();

    // Opaque black panel with a thin white border (visible against any scene).
    for (uint32_t y = 0; y < H; y++) {
        for (uint32_t x = 0; x < W; x++) {
            uint8_t v = 0;
            if (x < 2 || x >= W - 2 || y < 2 || y >= H - 2) v = 255;  // border
            uint8_t* p = px + (size_t)(y * W + x) * 4;
            p[0] = v; p[1] = v; p[2] = v; p[3] = 255;
        }
    }

    auto plot = [&](uint32_t x, uint32_t y) {
        if (x >= W || y >= H) return;
        uint8_t* p = px + (size_t)(y * W + x) * 4;
        p[0] = 255; p[1] = 255; p[2] = 255; p[3] = 255;
    };

    for (size_t li = 0; li < lines.size(); li++) {
        uint32_t oy = kMargin + (uint32_t)li * kLineAdv;
        if (oy + 7 * kScale + kMargin > H) break;
        uint32_t ox = kMargin;
        for (char ch : lines[li]) {
            const uint8_t* g = glyphFor(ch);
            if (g) {
                for (uint32_t row = 0; row < 7; row++)
                    for (uint32_t col = 0; col < kGlyphW; col++)
                        if (g[row] & (0x80u >> col))
                            for (uint32_t sy = 0; sy < kScale; sy++)
                                for (uint32_t sx = 0; sx < kScale; sx++)
                                    plot(ox + col * kScale + sx, oy + row * kScale + sy);
            }
            ox += kCharAdv;
            if (ox + kCharAdv > W - kMargin) break;  // clip overflow
        }
    }
}

// ── Vulkan setup ────────────────────────────────────────────────────────────
bool EyePoseHud::init(VkPhysicalDevice pd, VkDevice dev, VkQueue q, uint32_t qfi) {
    phys_ = pd; device_ = dev; queue_ = q;
    pixels_.assign((size_t)kPanelW * kPanelH * 4, 0);

    VkCommandPoolCreateInfo pci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = qfi;
    if (vkCreateCommandPool(dev, &pci, nullptr, &cmdPool_) != VK_SUCCESS) return false;

    // Device-local RGBA8 panel image (blit source). RGBA→BGRA + linear→sRGB is
    // handled by the blit into the sRGB swapchain, matching renderEye.
    VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent = {kPanelW, kPanelH, 1};
    ici.mipLevels = 1; ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(dev, &ici, nullptr, &image_) != VK_SUCCESS) return false;

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(dev, image_, &mr);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = findMemoryType(pd, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mai.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(dev, &mai, nullptr, &imageMem_) != VK_SUCCESS) return false;
    vkBindImageMemory(dev, image_, imageMem_, 0);

    // Host-visible staging buffer (persistently mapped).
    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = (VkDeviceSize)kPanelW * kPanelH * 4;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(dev, &bci, nullptr, &staging_) != VK_SUCCESS) return false;
    VkMemoryRequirements bmr;
    vkGetBufferMemoryRequirements(dev, staging_, &bmr);
    VkMemoryAllocateInfo bmai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    bmai.allocationSize = bmr.size;
    bmai.memoryTypeIndex = findMemoryType(pd, bmr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (bmai.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(dev, &bmai, nullptr, &stagingMem_) != VK_SUCCESS) return false;
    vkBindBufferMemory(dev, staging_, stagingMem_, 0);
    vkMapMemory(dev, stagingMem_, 0, bci.size, 0, &stagingMapped_);

    ready_ = true;
    return true;
}

// ── Per-frame draw ──────────────────────────────────────────────────────────
void EyePoseHud::render(VkImage swapchainImage,
                        const std::vector<std::pair<uint32_t, uint32_t>>& tileOffsets,
                        const std::vector<std::string>& lines) {
    if (!ready_ || tileOffsets.empty()) return;

    rasterize(lines);
    std::memcpy(stagingMapped_, pixels_.data(), pixels_.size());

    VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = cmdPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    if (vkAllocateCommandBuffers(device_, &ai, &cmd) != VK_SUCCESS) return;
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    auto barrier = [&](VkImage img, VkImageLayout oldL, VkImageLayout newL,
                       VkAccessFlags srcA, VkAccessFlags dstA,
                       VkPipelineStageFlags srcS, VkPipelineStageFlags dstS) {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.srcAccessMask = srcA; b.dstAccessMask = dstA;
        b.oldLayout = oldL; b.newLayout = newL;
        b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = img;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(cmd, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &b);
    };

    // Upload panel → HUD image, then make it a blit source.
    barrier(image_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy copy = {};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {kPanelW, kPanelH, 1};
    vkCmdCopyBufferToImage(cmd, staging_, image_,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barrier(image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Swapchain (COLOR_ATTACHMENT after renderEye) → TRANSFER_DST, preserving
    // the rendered eyes (oldLayout carries prior contents).
    barrier(swapchainImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Blit the panel 1:1 into every eye tile at the same tile-relative spot.
    for (const auto& off : tileOffsets) {
        VkImageBlit blit = {};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {(int32_t)kPanelW, (int32_t)kPanelH, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.dstOffsets[0] = {(int32_t)off.first, (int32_t)off.second, 0};
        blit.dstOffsets[1] = {(int32_t)(off.first + kPanelW), (int32_t)(off.second + kPanelH), 1};
        vkCmdBlitImage(cmd, image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, VK_FILTER_NEAREST);
    }

    // Swapchain → COLOR_ATTACHMENT_OPTIMAL for the compositor.
    barrier(swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    vkEndCommandBuffer(cmd);
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    vkQueueSubmit(queue_, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue_);
    vkFreeCommandBuffers(device_, cmdPool_, 1, &cmd);
}

void EyePoseHud::destroy() {
    if (stagingMapped_) { vkUnmapMemory(device_, stagingMem_); stagingMapped_ = nullptr; }
    if (staging_)   vkDestroyBuffer(device_, staging_, nullptr);
    if (stagingMem_) vkFreeMemory(device_, stagingMem_, nullptr);
    if (image_)     vkDestroyImage(device_, image_, nullptr);
    if (imageMem_)  vkFreeMemory(device_, imageMem_, nullptr);
    if (cmdPool_)   vkDestroyCommandPool(device_, cmdPool_, nullptr);
    staging_ = VK_NULL_HANDLE; stagingMem_ = VK_NULL_HANDLE;
    image_ = VK_NULL_HANDLE; imageMem_ = VK_NULL_HANDLE; cmdPool_ = VK_NULL_HANDLE;
    ready_ = false;
}
