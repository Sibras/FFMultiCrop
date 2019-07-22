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

#include "FFFREncoder.h"
#include "FFFRStreamUtils.h"
#include "FFFRUtility.h"
#include "FFFrameReader.h"
#include "FFMultiCrop.h"

#include <algorithm>

extern "C" {
#include <libavutil/frame.h>
}

using namespace std;

namespace Fmc {
bool MultiCrop::cropAndEncode(
    const std::string& sourceFile, const std::vector<CropOptions>& cropList, const EncoderOptions& options)
{
    // Try and open source video
    const auto stream = Ffr::Stream::getStream(sourceFile);
    if (stream == nullptr) {
        return false;
    }

    // Create output encoders for each crop sequence
    vector<shared_ptr<Ffr::Encoder>> encoders;
    for (const auto& i : cropList) {
        // Validate the input crop sequence
        if (i.m_resolution.m_height > stream->getHeight() || i.m_resolution.m_width > stream->getWidth()) {
            Ffr::log("Required output resolution is greater than input stream"s, Ffr::LogLevel::Error);
            return false;
        }
        if (i.m_cropList.size() > static_cast<size_t>(stream->getTotalFrames())) {
            Ffr::log("Crop list contains more frames than are found in input stream"s, Ffr::LogLevel::Error);
            return false;
        }
        // Create the new encoder
        encoders.emplace_back(make_shared<Ffr::Encoder>(i.m_fileName, i.m_resolution.m_width, i.m_resolution.m_height,
            Ffr::getRational(Ffr::StreamUtils::getSampleAspectRatio(stream.get())), stream->getPixelFormat(),
            Ffr::getRational(Ffr::StreamUtils::getFrameRate(stream.get())), stream->getDuration(), options.m_type,
            options.m_quality, options.m_preset, options.m_gopSize, Ffr::Encoder::ConstructorLock()));
        if (!encoders.back()->isEncoderValid()) {
            return false;
        }
    }

    // Loop through each frame and apply crop values
    while (true) {
        // Get next frame
        auto frame = stream->getNextFrame();
        if (frame == nullptr) {
            if (!stream->isEndOfFile()) {
                return false;
            }
        }
        // Send decoded frame to the encoder(s)
        uint32_t current = 0;
        for (auto& i : encoders) {
            // Duplicate frame
            Ffr::FramePtr copyFrame(av_frame_clone(frame->m_frame.m_frame));
            if (copyFrame.m_frame == nullptr) {
                Ffr::log("Failed to copy frame", Ffr::LogLevel::Error);
                return false;
            }
            auto newFrame = Ffr::Frame(
                copyFrame, frame->m_timeStamp, frame->m_frameNum, frame->m_formatContext, frame->m_codecContext);

            const auto crop = cropList[current].m_cropList[newFrame.getFrameNumber()];
            // Correct out of range crop values
            auto cropTop = std::min(crop.m_top, stream->getHeight() - cropList[current].m_resolution.m_height);
            auto cropLeft = std::min(crop.m_left, stream->getWidth() - cropList[current].m_resolution.m_width);
            auto cropBottom = cropList[current].m_resolution.m_height + cropTop;
            if (cropBottom > stream->getHeight()) {
                cropTop -= cropBottom - stream->getHeight();
                cropBottom = 0;
            } else {
                cropBottom = stream->getHeight() - cropBottom;
            }
            auto cropRight = cropList[current].m_resolution.m_width + cropLeft;
            if (cropRight > stream->getWidth()) {
                cropLeft -= cropRight - stream->getWidth();
                cropRight = 0;
            } else {
                cropRight = stream->getWidth() - cropRight;
            }
            if (cropTop != crop.m_left || cropLeft != crop.m_left) {
                Ffr::log("Out of range crop values detected, crop has been clamped for frame: "s + to_string(current),
                    Ffr::LogLevel::Warning);
            }

            // Apply crop settings
            newFrame.m_frame->crop_top = cropTop;
            newFrame.m_frame->crop_bottom = cropBottom;
            newFrame.m_frame->crop_left = cropLeft;
            newFrame.m_frame->crop_right = cropRight;

            // Encode new frame
            if (!i->encodeFrame(frame)) {
                return false;
            }

            ++current;
        }
        // Exit if all frames have been processed
        if (frame == nullptr) {
            return true;
        }
    }
}
} // namespace Fmc