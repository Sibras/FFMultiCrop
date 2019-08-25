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
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

using namespace std;

namespace Fmc {
class MultiCrop
{
public:
    std::shared_ptr<Ffr::Stream> m_stream;
    std::vector<std::shared_ptr<Ffr::Encoder>> m_encoders;
    std::vector<CropOptions> m_cropList;
    int64_t m_currentFrame = 0;
    int64_t m_lastFrame;

    /**
     * Multi crop
     * @param [in,out] stream    The input stream.
     * @param [in,out] encoders  The configured output encoders.
     * @param          cropList  List of crop options for each desired output video.
     * @param          lastFrame The last frame required by all output encoders.
     */
    FFFRAMEREADER_NO_EXPORT MultiCrop(std::shared_ptr<Ffr::Stream>& stream,
        std::vector<std::shared_ptr<Ffr::Encoder>>& encoders, std::vector<CropOptions> cropList,
        const int64_t lastFrame) noexcept
        : m_stream(move(stream))
        , m_encoders(move(encoders))
        , m_cropList(move(cropList))
        , m_lastFrame(lastFrame)
    {}

    FFFRAMEREADER_NO_EXPORT static std::shared_ptr<MultiCrop> getMultiCrop(const std::string& sourceFile,
        const std::vector<CropOptions>& cropList, const EncoderOptions& options = EncoderOptions()) noexcept
    {
        // Try and open source video
        auto stream = Ffr::Stream::getStream(sourceFile);
        if (stream == nullptr) {
            return nullptr;
        }

        // Create output encoders for each crop sequence
        int64_t longestFrames = 0;
        vector<shared_ptr<Ffr::Encoder>> encoders;
        for (const auto& i : cropList) {
            // Validate the input crop sequence
            if (i.m_resolution.m_height > stream->getHeight() || i.m_resolution.m_width > stream->getWidth()) {
                Ffr::log("Required output resolution is greater than input stream"s, Ffr::LogLevel::Error);
                return nullptr;
            }
            if (i.m_cropList.size() > static_cast<size_t>(stream->getTotalFrames())) {
                Ffr::log("Crop list contains more frames than are found in input stream"s, Ffr::LogLevel::Error);
                return nullptr;
            }
            size_t skipFrames = 0;
            for (const auto& j : i.m_skipRegions) {
                if (j.second < j.first) {
                    Ffr::log("Crop list contains invalid skip region ("s += to_string(j.first) += ", "s +=
                        to_string(j.second) += ")."s,
                        Ffr::LogLevel::Error);
                    return nullptr;
                }
                if (j.second > static_cast<uint64_t>(stream->getTotalFrames()) ||
                    j.first > static_cast<uint64_t>(stream->getTotalFrames())) {
                    Ffr::log(
                        "Crop list contains skip regions greater than total video size. Region will be ignored ("s +=
                        to_string(j.first) += ", "s += to_string(j.second) += ")."s,
                        Ffr::LogLevel::Warning);
                }
                skipFrames += j.second - j.first;
            }
            int64_t totalFrames = skipFrames + i.m_cropList.size();
            if (totalFrames > stream->getTotalFrames()) {
                Ffr::log(
                    "Crop list size combined with skip regions is greater than input stream. Crops greater than file length will be ignored."s,
                    Ffr::LogLevel::Warning);
                totalFrames = stream->getTotalFrames();
            }
            longestFrames = std::max(longestFrames, totalFrames);
            // Create the new encoder
            // TODO: option for number of threads. split total available by number of encoders.
            encoders.emplace_back(make_shared<Ffr::Encoder>(i.m_fileName, i.m_resolution.m_width,
                i.m_resolution.m_height, Ffr::getRational(Ffr::StreamUtils::getSampleAspectRatio(stream.get())),
                stream->getPixelFormat(), Ffr::getRational(Ffr::StreamUtils::getFrameRate(stream.get())),
                stream->frameToTime(i.m_cropList.size()), options.m_type, options.m_quality, options.m_preset,
                options.m_gopSize, Ffr::Encoder::ConstructorLock()));
            if (!encoders.back()->isEncoderValid()) {
                return nullptr;
            }
        }

        // Create object
        return make_shared<MultiCrop>(stream, encoders, cropList, longestFrames);
    }

    FFFRAMEREADER_NO_EXPORT bool encodeLoop() noexcept
    {
        // Loop through each frame and apply crop values
        while (true) {
            // Check if already received all required frames
            if (m_currentFrame >= m_lastFrame) {
                // Send flush frame
                for (auto& i : m_encoders) {
                    if (!i->encodeFrame(nullptr)) {
                        return false;
                    }
                }
                return true;
            }
            // Get next frame
            auto frame = m_stream->getNextFrame();
            if (frame == nullptr) {
                if (!m_stream->isEndOfFile()) {
                    return false;
                }
                // Send flush frame
                for (auto& i : m_encoders) {
                    if (!i->encodeFrame(nullptr)) {
                        return false;
                    }
                }
                return true;
            }
            ++m_currentFrame;
            // Send decoded frame to the encoder(s)
            uint32_t current = 0;
            for (auto& i : m_encoders) {
                const auto crop = m_cropList[current].getCrop(static_cast<uint64_t>(frame->getFrameNumber()));
                if (crop.m_top != UINT32_MAX || crop.m_left != UINT32_MAX) {
                    // Duplicate frame
                    Ffr::FramePtr copyFrame(av_frame_clone(frame->m_frame.m_frame));
                    if (copyFrame.m_frame == nullptr) {
                        Ffr::log("Failed to copy frame", Ffr::LogLevel::Error);
                        return false;
                    }
                    auto newFrame = make_shared<Ffr::Frame>(copyFrame, frame->m_timeStamp, frame->m_frameNum,
                        frame->m_formatContext, frame->m_codecContext);

                    // Correct out of range crop values
                    auto cropTop =
                        std::min(crop.m_top, m_stream->getHeight() - m_cropList[current].m_resolution.m_height);
                    auto cropLeft =
                        std::min(crop.m_left, m_stream->getWidth() - m_cropList[current].m_resolution.m_width);
                    auto cropBottom = m_cropList[current].m_resolution.m_height + cropTop;
                    if (cropBottom > m_stream->getHeight()) {
                        cropTop -= cropBottom - m_stream->getHeight();
                        cropBottom = 0;
                    } else {
                        cropBottom = m_stream->getHeight() - cropBottom;
                    }
                    auto cropRight = m_cropList[current].m_resolution.m_width + cropLeft;
                    if (cropRight > m_stream->getWidth()) {
                        cropLeft -= cropRight - m_stream->getWidth();
                        cropRight = 0;
                    } else {
                        cropRight = m_stream->getWidth() - cropRight;
                    }
                    if (cropTop != crop.m_top || cropLeft != crop.m_left) {
                        Ffr::log("Out of range crop values detected, crop has been clamped for frame: "s +
                                to_string(newFrame->getFrameNumber()),
                            Ffr::LogLevel::Warning);
                    }

                    // Apply crop settings
                    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(newFrame->m_codecContext->pix_fmt);
                    if (desc->flags & AV_PIX_FMT_FLAG_HWACCEL) {
                        newFrame->m_frame->crop_top += cropTop;
                        newFrame->m_frame->crop_bottom = cropBottom - newFrame->m_frame->crop_bottom;
                        newFrame->m_frame->crop_left += cropLeft;
                        newFrame->m_frame->crop_right = cropRight - newFrame->m_frame->crop_right;
                    } else {
                        int32_t maxStep[4];
                        av_image_fill_max_pixsteps(maxStep, nullptr, desc);

                        newFrame->m_frame->width = m_cropList[current].m_resolution.m_width;
                        newFrame->m_frame->height = m_cropList[current].m_resolution.m_height;

                        newFrame->m_frame->data[0] += cropTop * newFrame->m_frame->linesize[0];
                        newFrame->m_frame->data[0] += cropLeft * maxStep[0];

                        if (!(desc->flags & AV_PIX_FMT_FLAG_PAL || desc->flags & AV_PIX_FMT_FLAG_PSEUDOPAL)) {
                            for (uint32_t j = 1; j < 3; j++) {
                                if (newFrame->m_frame->data[j]) {
                                    newFrame->m_frame->data[j] +=
                                        (cropTop >> desc->log2_chroma_h) * newFrame->m_frame->linesize[j];
                                    newFrame->m_frame->data[j] += (cropLeft * maxStep[j]) >> desc->log2_chroma_w;
                                }
                            }
                        }

                        // Alpha plane must be treated separately
                        if (newFrame->m_frame->data[3]) {
                            newFrame->m_frame->data[3] += cropTop * newFrame->m_frame->linesize[3];
                            newFrame->m_frame->data[3] += cropLeft * maxStep[3];
                        }
                    }
                    // Encode new frame
                    if (!i->encodeFrame(newFrame)) {
                        return false;
                    }
                }

                ++current;
            }
        }
    }

    FFFRAMEREADER_NO_EXPORT float getProgress() const
    {
        return static_cast<float>(m_currentFrame) / static_cast<float>(m_lastFrame);
    }
};

Crop CropOptions::getCrop(const uint64_t frame) const noexcept
{
    if (frame < static_cast<uint64_t>(m_cropList.size())) {
        // Check if frame is in skip region
        bool skip = false;
        for (const auto& i : m_skipRegions) {
            if (frame >= i.first && frame < i.second) {
                skip = true;
                break;
            }
        }
        if (!skip) {
            return m_cropList[frame];
        }
    }
    return {UINT32_MAX, UINT32_MAX};
}

bool cropAndEncode(
    const string& sourceFile, const vector<CropOptions>& cropList, const EncoderOptions& options) noexcept
{
    const auto multiCrop(MultiCrop::getMultiCrop(sourceFile, cropList, options));
    if (multiCrop == nullptr) {
        return false;
    }
    return multiCrop->encodeLoop();
}

MultiCropServer::~MultiCropServer()
{
    if (m_future.valid()) {
        m_future.wait();
    }
}

MultiCropServer::MultiCropServer(
    std::shared_ptr<MultiCrop>& multiCrop, std::future<bool>& future, ConstructorLock) noexcept
    : m_multiCrop(move(multiCrop))
    , m_future(move(future))
{}

MultiCropServer::Status MultiCropServer::getStatus() noexcept
{
    if (m_status == Status::Running) {
        // Check for updates status
        if (m_future.wait_for(chrono::seconds(0)) == future_status::ready) {
            m_status = m_future.get() ? Status::Completed : Status::Failed;
        }
    }
    return m_status;
}

float MultiCropServer::getProgress() noexcept
{
    if (getStatus() == Status::Completed) {
        return 1.0f;
    }
    if (getStatus() == Status::Failed) {
        return 0.0f;
    }
    return m_multiCrop->getProgress();
}

std::shared_ptr<MultiCropServer> cropAndEncodeAsync(
    const std::string& sourceFile, const std::vector<CropOptions>& cropList, const EncoderOptions& options) noexcept
{
    auto multiCrop(MultiCrop::getMultiCrop(sourceFile, cropList, options));
    if (multiCrop == nullptr) {
        return nullptr;
    }
    auto future(async(launch::async, &MultiCrop::encodeLoop, multiCrop));
    if (!future.valid()) {
        return nullptr;
    }
    return make_shared<MultiCropServer>(multiCrop, future, MultiCropServer::ConstructorLock());
}
} // namespace Fmc
