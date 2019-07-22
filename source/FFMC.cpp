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
        // Create the new encoder
        encoders.emplace_back(make_shared<Ffr::Encoder>(sourceFile, i.m_resolution.m_width, i.m_resolution.m_height,
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

            // Apply crop settings
            const auto crop = cropList[current].m_cropList[newFrame.getFrameNumber()];
            newFrame.m_frame->crop_top = crop.m_top;
            newFrame.m_frame->crop_bottom = cropList[current].m_resolution.m_height - crop.m_top;
            newFrame.m_frame->crop_left = crop.m_left;
            newFrame.m_frame->crop_right = cropList[current].m_resolution.m_width - crop.m_left;

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