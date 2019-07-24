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
#include "ffmulticrop_export.h"

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
    FFMULTICROP_EXPORT Crop getCrop(uint32_t frame) const noexcept;

    std::vector<Crop> m_cropList;     /**< List of crops for each frame in video */
    Resolution m_resolution = {0, 0}; /**< The resolution of the output video (affects crop size) */
    std::string m_fileName;           /**< Filename of the output file */
};

class MultiCrop
{
public:
    /**
     * Crops and encodes and input video into 1 or more output videos
     * @param sourceFile Source video.
     * @param cropList   List of crop options for each desired output video.
     * @param options    (Optional) Options to control the out encode.
     * @returns True if it succeeds, false if it fails.
     */
    FFMULTICROP_EXPORT static bool cropAndEncode(const std::string& sourceFile,
        const std::vector<CropOptions>& cropList, const EncoderOptions& options = EncoderOptions());
};
} // namespace Fmc