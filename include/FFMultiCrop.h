/**
 * Copyright 2019 Matthew Oliver
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include "FFFRTypes.h"
#include "FFMCExports.h"

#include <future>
#include <string>
#include <vector>

namespace Fmc {
using EncodeType = Ffr::EncodeType;
using EncoderOptions = Ffr::EncoderOptions;

struct Resolution
{
    uint32_t m_width;
    uint32_t m_height;
};

struct Crop
{
    uint32_t m_top;  /**< The offset in pixels from top of frame */
    uint32_t m_left; /**< The offset in pixels from left of frame */
};

class CropOptions
{
public:
    FFMULTICROP_EXPORT CropOptions() = default;

    FFMULTICROP_EXPORT ~CropOptions() = default;

    FFMULTICROP_EXPORT CropOptions(const CropOptions& other) = default;

    FFMULTICROP_EXPORT CropOptions(CropOptions&& other) = default;

    FFMULTICROP_EXPORT CropOptions& operator=(const CropOptions& other) = default;

    FFMULTICROP_EXPORT CropOptions& operator=(CropOptions&& other) = default;

    /**
     * Gets a crop value.
     * @param frame The frame index of the value to get.
     * @returns The crop, {UINT32_MAX, UINT32_MAX} if frame is invalid.
     */
    FFMULTICROP_EXPORT Crop getCrop(uint64_t frame) const noexcept;

    std::vector<Crop> m_cropList;     /**< List of crops for each frame in video */
    Resolution m_resolution = {0, 0}; /**< The resolution of the output video (affects crop size) */
    std::string m_fileName;           /**< Filename of the output file */
    std::vector<std::pair<uint64_t, uint64_t>> m_skipRegions; /**< A list of frame ranges to skip during encoding. The
                                                               list elements takes the form [startFrame, endFrame) */
};

/**
 * Crops and encodes an input video into 1 or more output videos synchronously.
 * @param sourceFile Source video.
 * @param cropList   List of crop options for each desired output video.
 * @param options    (Optional) Options to control the out encode.
 * @returns True if it succeeds, false if it fails.
 */
FFMULTICROP_EXPORT bool cropAndEncode(const std::string& sourceFile, const std::vector<CropOptions>& cropList,
    const EncoderOptions& options = EncoderOptions()) noexcept;

class MultiCrop;

class MultiCropServer
{
public:
    FFMULTICROP_EXPORT MultiCropServer() = delete;

    FFMULTICROP_EXPORT ~MultiCropServer();

    FFMULTICROP_EXPORT MultiCropServer(const MultiCropServer& other) = delete;

    FFMULTICROP_EXPORT MultiCropServer(MultiCropServer&& other) noexcept = delete;

    FFMULTICROP_EXPORT MultiCropServer& operator=(const MultiCropServer& other) = delete;

    FFMULTICROP_EXPORT MultiCropServer& operator=(MultiCropServer&& other) noexcept = delete;

    class ConstructorLock
    {
        friend class MultiCrop;
    };

    FFMULTICROP_NO_EXPORT MultiCropServer(
        std::shared_ptr<MultiCrop>& multiCrop, std::future<bool>& future, ConstructorLock) noexcept;

    enum class Status
    {
        Failed,
        Running,
        Completed
    };

    /**
     * Gets the encode status.
     * @returns The status.
     */
    FFMULTICROP_EXPORT Status getStatus() noexcept;

    /**
     * Gets the encode progress.
     * @returns The progress (normalised value between 0 and 1 inclusive).
     */
    FFMULTICROP_EXPORT float getProgress() noexcept;

private:
    std::shared_ptr<MultiCrop> m_multiCrop;
    std::future<bool> m_future;
    Status m_status = Status::Running;
};

/**
 * Crops and encodes an input video into 1 or more output videos.
 * @param sourceFile Source video.
 * @param cropList   List of crop options for each desired output video.
 * @param options    (Optional) Options to control the out encode.
 * @returns The server object if succeeded, nullptr otherwise.
 */
FFMULTICROP_EXPORT std::shared_ptr<MultiCropServer> cropAndEncodeAsync(const std::string& sourceFile,
    const std::vector<CropOptions>& cropList, const EncoderOptions& options = EncoderOptions()) noexcept;
} // namespace Fmc