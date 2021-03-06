﻿/**
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
#include "FFFRTestData.h"
#include "FFFrameReader.h"
#include "FFMultiCrop.h"

#include <gtest/gtest.h>
using namespace Fmc;

struct TestParamsEncode
{
    uint32_t m_testDataIndex;
    std::vector<CropOptions> m_cropList;
};

static CropOptions s_options1 = {{{0, 0}, {0, 1}}, {640, 480}, "test-mc-1.mkv"};
static CropOptions s_options2 = {{{0, 0}, {0, 1}}, {480, 640}, "test-mc-2.mkv"};
static CropOptions s_options3 = {
    {{0, 0}, {0, 1}}, {640, 480}, "test-mc-3.mkv", {std::make_pair(0ULL, 250ULL), std::make_pair(500ULL, 750ULL)}};

static std::vector<TestParamsEncode> g_testDataEncode = {
    {0, {s_options1, s_options2}},
    {0, {s_options3}},
};

class EncodeTest1 : public ::testing::TestWithParam<TestParamsEncode>
{
protected:
    EncodeTest1() = default;

    void SetUp() override
    {
        Ffr::setLogLevel(Ffr::LogLevel::Error);
        m_cropOps = GetParam().m_cropList;

        // Generate list of crops
        int32_t xMod = 10;
        int32_t yMod = 1;
        for (auto& j : m_cropOps) {
            int32_t directionX = 1;
            int32_t directionY = 1;
            int32_t x = 0, y = 0;
            j.m_cropList.resize(0);
            // Set width/height offset to create out of bounds values that will need to be clamped
            const int32_t width = j.m_resolution.m_width / 2;
            const int32_t height = j.m_resolution.m_height / 2;
            for (int32_t i = 0; i < 1000; ++i) {
                if (x >= (g_testData[GetParam().m_testDataIndex].m_width - width) || x < (0 - width)) {
                    directionX *= -1;
                    x += xMod * directionX;
                }
                if (y >= (g_testData[GetParam().m_testDataIndex].m_height - height) || y < (0 - height)) {
                    directionY *= -1;
                    y += yMod * directionY;
                }
                j.m_cropList.emplace_back(
                    CropPosition({static_cast<uint32_t>(std::max(y, 0)), static_cast<uint32_t>(std::max(x, 0))}));
                y += yMod * directionY;
                x += xMod * directionX;
            }
            std::swap(xMod, yMod);
        }
    }

    void TearDown() override {}

    std::vector<CropOptions> m_cropOps;
};

TEST_P(EncodeTest1, encodeStream)
{
    // Just run an encode and see if output is correct manually
    ASSERT_TRUE(cropAndEncode(g_testData[GetParam().m_testDataIndex].m_fileName, m_cropOps));

    // Check that we can open encoded file and its parameters are correct
    for (const auto& i : m_cropOps) {
        auto stream = Ffr::Stream::getStream(i.m_fileName);
        ASSERT_NE(stream, nullptr);

        ASSERT_EQ(stream->getWidth(), i.m_resolution.m_width);
        ASSERT_EQ(stream->getHeight(), i.m_resolution.m_height);
        ASSERT_EQ(stream->getTotalFrames(), i.m_cropList.size());
        ASSERT_DOUBLE_EQ(stream->getFrameRate(), g_testData[GetParam().m_testDataIndex].m_frameRate);
    }
}

TEST_P(EncodeTest1, encodeStreamAsync)
{
    // Use different output name for this test
    for (auto& i : m_cropOps) {
        i.m_fileName = "async-" + i.m_fileName;
    }

    // Just run an encode and see if output is correct manually
    auto server = cropAndEncodeAsync(g_testData[GetParam().m_testDataIndex].m_fileName, m_cropOps);
    ASSERT_NE(server, nullptr);

    // Wait for encode to finish
    while (server->getStatus() == MultiCropServer::Status::Running) {
        ASSERT_GE(server->getProgress(), 0.0f);
        ASSERT_LE(server->getProgress(), 1.0f);
    }
    ASSERT_EQ(server->getStatus(), MultiCropServer::Status::Completed);
    ASSERT_FLOAT_EQ(server->getProgress(), 1.0f);

    // Check that we can open encoded file and its parameters are correct
    for (const auto& i : m_cropOps) {
        auto stream = Ffr::Stream::getStream(i.m_fileName);
        ASSERT_NE(stream, nullptr);

        ASSERT_EQ(stream->getWidth(), i.m_resolution.m_width);
        ASSERT_EQ(stream->getHeight(), i.m_resolution.m_height);
        ASSERT_EQ(stream->getTotalFrames(), i.m_cropList.size());
        ASSERT_DOUBLE_EQ(stream->getFrameRate(), g_testData[GetParam().m_testDataIndex].m_frameRate);
    }
}

INSTANTIATE_TEST_SUITE_P(EncodeTestData, EncodeTest1, ::testing::ValuesIn(g_testDataEncode));
