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
    shared_ptr<Stream> m_stream;

    class EncoderParams
    {
    public:
        EncoderParams(shared_ptr<Ffr::Encoder>& encoder, CropOptions cropOptions)
            : m_encoder(move(encoder))
            , m_cropList(move(cropOptions))
        {}

        shared_ptr<Ffr::Encoder> m_encoder = nullptr;
        CropOptions m_cropList;
        int64_t m_lastValidTime = INT64_MIN;
    };

    vector<EncoderParams> m_encoders;
    int64_t m_currentFrame = 0;
    int64_t m_lastFrame;

    /**
     * Multi crop
     * @param [in,out] stream    The input stream.
     * @param [in,out] encoders  The configured output encoders and associated data.
     * @param          lastFrame The last frame required by all output encoders.
     */
    FFFRAMEREADER_NO_EXPORT MultiCrop(
        shared_ptr<Stream> stream, vector<EncoderParams>& encoders, const int64_t lastFrame) noexcept
        : m_stream(move(stream))
        , m_encoders(move(encoders))
        , m_lastFrame(lastFrame)
    {}

    FFFRAMEREADER_NO_EXPORT static shared_ptr<MultiCrop> getMultiCrop(const string& sourceFile,
        const vector<CropOptions>& cropList, const EncoderOptions& options = EncoderOptions()) noexcept
    {
        // Try and open source video
        const auto stream = Ffr::Stream::getStream(sourceFile);
        if (stream == nullptr) {
            return nullptr;
        }

        return getMultiCrop(stream, cropList, options);
    }

    FFFRAMEREADER_NO_EXPORT static std::shared_ptr<MultiCrop> getMultiCrop(const std::shared_ptr<Stream>& stream,
        const std::vector<CropOptions>& cropList, const EncoderOptions& options = EncoderOptions()) noexcept
    {
        // Auto calculate ideal number of threads
        auto numThreads = options.m_numThreads;
        if (numThreads == 0) {
            numThreads = std::max(static_cast<uint32_t>(std::thread::hardware_concurrency() / cropList.size()), 2U);
        }

        int64_t longestFrames = 0;
        int64_t startFrame = 0;
        vector<EncoderParams> encoders;
        for (auto& i : cropList) {
            // Validate the input crop sequence
            if (i.m_resolution.m_height > stream->getHeight() || i.m_resolution.m_width > stream->getWidth()) {
                Ffr::log("Required output resolution is greater than input stream"s, Ffr::LogLevel::Error);
                return nullptr;
            }
            if (i.m_cropList.size() > static_cast<size_t>(stream->getTotalFrames())) {
                Ffr::log("Crop list contains more frames than are found in input stream"s, Ffr::LogLevel::Error);
                return nullptr;
            }
            // Validate the input skip sequence
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
                // Calculate total number of frames that will be skipped
                skipFrames += j.second - j.first;
                // Check if skipping start frames
                if (j.first <= 0) {
                    startFrame = std::min(startFrame, static_cast<int64_t>(j.second));
                }
            }
            // Check total number of cropped/skipped frames
            int64_t totalFrames = skipFrames + i.m_cropList.size();
            if (totalFrames > stream->getTotalFrames()) {
                Ffr::log(
                    "Crop list size combined with skip regions is greater than input stream. Crops greater than file length will be ignored."s,
                    Ffr::LogLevel::Warning);
                totalFrames = stream->getTotalFrames();
            }
            longestFrames = std::max(longestFrames, totalFrames);
            // Seek to start crop region
            if (startFrame > stream->peekNextFrame()->getFrameNumber()) {
                stream->seekFrame(startFrame);
            }
            // Create the new encoder
            auto encoder = make_shared<Ffr::Encoder>(i.m_fileName, i.m_resolution.m_width, i.m_resolution.m_height,
                Ffr::getRational(Ffr::StreamUtils::getSampleAspectRatio(stream.get())), stream->getPixelFormat(),
                Ffr::getRational(Ffr::StreamUtils::getFrameRate(stream.get())),
                stream->frameToTime(i.m_cropList.size()), options.m_type, options.m_quality, options.m_preset,
                numThreads, options.m_gopSize, Ffr::Encoder::ConstructorLock());
            if (!encoder->isEncoderValid()) {
                return nullptr;
            }
            encoders.emplace_back(encoder, i);
        }

        // Create object
        return make_shared<MultiCrop>(stream, encoders, longestFrames);
    }

    FFFRAMEREADER_NO_EXPORT bool encodeLoop() noexcept
    {
        // Loop through each frame and apply crop values
        int64_t lastTime = 0;
        uint32_t sentFrames = 0;
        while (true) {
            // Check if already received all required frames
            if (m_currentFrame >= m_lastFrame) {
                // Send flush frame
                for (auto& i : m_encoders) {
                    if (!i.m_encoder->encodeFrame(nullptr, nullptr)) {
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
                    if (!i.m_encoder->encodeFrame(nullptr, nullptr)) {
                        return false;
                    }
                }
                return true;
            }
            ++m_currentFrame;
            // Send decoded frame to the encoder(s)
            for (auto& i : m_encoders) {
                const auto crop = i.m_cropList.getCrop(static_cast<uint64_t>(frame->getFrameNumber()));
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
                    auto cropTop = std::min(crop.m_top, m_stream->getHeight() - i.m_cropList.m_resolution.m_height);
                    auto cropLeft = std::min(crop.m_left, m_stream->getWidth() - i.m_cropList.m_resolution.m_width);
                    auto cropBottom = i.m_cropList.m_resolution.m_height + cropTop;
                    if (cropBottom > m_stream->getHeight()) {
                        cropTop -= cropBottom - m_stream->getHeight();
                        cropBottom = 0;
                    } else {
                        cropBottom = m_stream->getHeight() - cropBottom;
                    }
                    auto cropRight = i.m_cropList.m_resolution.m_width + cropLeft;
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

                        newFrame->m_frame->width = i.m_cropList.m_resolution.m_width;
                        newFrame->m_frame->height = i.m_cropList.m_resolution.m_height;

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

                    // Correct timestamp in case of skip regions
                    const int64_t timeStamp = (i.m_lastValidTime != INT64_MIN) ?
                        i.m_lastValidTime + (frame->m_frame->best_effort_timestamp - lastTime) :
                        0;
                    newFrame->m_frame->best_effort_timestamp = timeStamp;
                    newFrame->m_frame->pts = timeStamp;

                    i.m_lastValidTime = timeStamp;

                    // Encode new frame
                    if (!i.m_encoder->encodeFrame(newFrame, m_stream)) {
                        return false;
                    }
                    ++sentFrames;
                }
            }
            // Backup timestamp of last frame per output
            lastTime = frame->m_frame->best_effort_timestamp;
        }
    }

    FFFRAMEREADER_NO_EXPORT float getProgress() const
    {
        return static_cast<float>(m_currentFrame) / static_cast<float>(m_lastFrame);
    }
};

CropPosition CropOptions::getCrop(const uint64_t frame) const noexcept
{
    // Check if frame is in skip region
    bool skip = false;
    uint64_t skipSize = 0;
    for (const auto& i : m_skipRegions) {
        if (frame >= i.first && frame < i.second) {
            skip = true;
            break;
        }
        if (frame >= i.first) {
            skipSize += i.second - i.first;
        } else {
            break;
        }
    }
    if (!skip) {
        return m_cropList[frame - skipSize];
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

bool cropAndEncode(
    const shared_ptr<Stream>& stream, const vector<CropOptions>& cropList, const EncoderOptions& options) noexcept
{
    if (stream->peekNextFrame()->getFrameNumber() != 0) {
        // Ensure stream is at the start
        stream->seek(0);
    }
    const auto multiCrop(MultiCrop::getMultiCrop(stream, cropList, options));
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

MultiCropServer::MultiCropServer(shared_ptr<MultiCrop>& multiCrop, future<bool>& future, ConstructorLock) noexcept
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

shared_ptr<MultiCropServer> cropAndEncodeAsync(
    const string& sourceFile, const vector<CropOptions>& cropList, const EncoderOptions& options) noexcept
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

shared_ptr<MultiCropServer> cropAndEncodeAsync(
    const shared_ptr<Stream>& stream, const vector<CropOptions>& cropList, const EncoderOptions& options) noexcept
{
    if (stream->peekNextFrame()->getFrameNumber() != 0) {
        // Ensure stream is at the start
        stream->seek(0);
    }
    auto multiCrop(MultiCrop::getMultiCrop(stream, cropList, options));
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
