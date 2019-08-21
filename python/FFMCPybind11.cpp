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
#include "FFMultiCrop.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

PYBIND11_MODULE(pyMultiCrop, m)
{
    m.doc() = "Crops and encodes an input video into 1 or more output videos";

    pybind11::enum_<Fmc::EncodeType>(m, "EncodeType", "")
        .value("h264", Fmc::EncodeType::h264)
        .value("h265", Fmc::EncodeType::h265);

    {
        pybind11::class_<Fmc::EncoderOptions, std::shared_ptr<Fmc::EncoderOptions>> cl(m, "EncoderOptions", "");
        cl.def(pybind11::init([]() { return new Fmc::EncoderOptions(); }));
        cl.def(pybind11::init([](Fmc::EncoderOptions const& o) { return new Fmc::EncoderOptions(o); }));

        pybind11::enum_<Fmc::EncoderOptions::Preset>(cl, "Preset", "")
            .value("Ultrafast", Fmc::EncoderOptions::Preset::Ultrafast)
            .value("Superfast", Fmc::EncoderOptions::Preset::Superfast)
            .value("Veryfast", Fmc::EncoderOptions::Preset::Veryfast)
            .value("Faster", Fmc::EncoderOptions::Preset::Faster)
            .value("Fast", Fmc::EncoderOptions::Preset::Fast)
            .value("Medium", Fmc::EncoderOptions::Preset::Medium)
            .value("Slow", Fmc::EncoderOptions::Preset::Slow)
            .value("Slower", Fmc::EncoderOptions::Preset::Slower)
            .value("Veryslow", Fmc::EncoderOptions::Preset::Veryslow)
            .value("Placebo", Fmc::EncoderOptions::Preset::Placebo);

        cl.def_readwrite("type", &Fmc::EncoderOptions::m_type);
        cl.def_readwrite("quality", &Fmc::EncoderOptions::m_quality);
        cl.def_readwrite("preset", &Fmc::EncoderOptions::m_preset);
        cl.def_readwrite("gopSize", &Fmc::EncoderOptions::m_gopSize);
        cl.def("assign",
            static_cast<Fmc::EncoderOptions& (Fmc::EncoderOptions::*)(const Fmc::EncoderOptions&)>(
                &Fmc::EncoderOptions::operator=),
            "", pybind11::return_value_policy::automatic, pybind11::arg("other"));
        cl.def("__eq__",
            static_cast<bool (Fmc::EncoderOptions::*)(const Fmc::EncoderOptions&) const>(
                &Fmc::EncoderOptions::operator==),
            "", pybind11::arg("other"));
        cl.def("__ne__",
            static_cast<bool (Fmc::EncoderOptions::*)(const Fmc::EncoderOptions&) const>(
                &Fmc::EncoderOptions::operator!=),
            "", pybind11::arg("other"));
    }

    pybind11::class_<Fmc::Resolution, std::shared_ptr<Fmc::Resolution>>(m, "Resolution", "")
        .def(pybind11::init<uint32_t, uint32_t>())
        .def(pybind11::init([]() { return new Fmc::Resolution(); }))
        .def(pybind11::init([](Fmc::Resolution const& o) { return new Fmc::Resolution(o); }))
        .def_readwrite("width", &Fmc::Resolution::m_width)
        .def_readwrite("height", &Fmc::Resolution::m_height)
        .def("assign",
            static_cast<Fmc::Resolution& (Fmc::Resolution::*)(const Fmc::Resolution&)>(&Fmc::Resolution::operator=), "",
            pybind11::return_value_policy::automatic, pybind11::arg("other"));

    pybind11::class_<Fmc::Crop, std::shared_ptr<Fmc::Crop>>(m, "Crop", "")
        .def(pybind11::init<uint32_t, uint32_t>())
        .def(pybind11::init([]() { return new Fmc::Crop(); }))
        .def(pybind11::init([](Fmc::Crop const& o) { return new Fmc::Crop(o); }))
        .def_readwrite("top", &Fmc::Crop::m_top)
        .def_readwrite("left", &Fmc::Crop::m_left)
        .def("assign", static_cast<struct Fmc::Crop& (Fmc::Crop::*)(const struct Fmc::Crop&)>(&Fmc::Crop::operator=),
            "", pybind11::return_value_policy::automatic, pybind11::arg("other"));

    pybind11::class_<Fmc::CropOptions, std::shared_ptr<Fmc::CropOptions>>(m, "CropOptions", "")
        .def(pybind11::init([]() { return new Fmc::CropOptions(); }))
        .def(pybind11::init([](Fmc::CropOptions const& o) { return new Fmc::CropOptions(o); }))
        .def_readwrite("cropList", &Fmc::CropOptions::m_cropList)
        .def_readwrite("resolution", &Fmc::CropOptions::m_resolution)
        .def_readwrite("fileName", &Fmc::CropOptions::m_fileName)
        .def_readwrite("skipRegions", &Fmc::CropOptions::m_skipRegions)
        .def("assign",
            static_cast<class Fmc::CropOptions& (Fmc::CropOptions::*)(const class Fmc::CropOptions&)>(
                &Fmc::CropOptions::operator=),
            "", pybind11::return_value_policy::automatic, pybind11::arg("other"));

    {
        pybind11::class_<Fmc::MultiCropServer, std::shared_ptr<Fmc::MultiCropServer>> cl(m, "MultiCropServer", "");
        pybind11::enum_<Fmc::MultiCropServer::Status>(cl, "Status", "")
            .value("Failed", Fmc::MultiCropServer::Status::Failed)
            .value("Running", Fmc::MultiCropServer::Status::Running)
            .value("Completed", Fmc::MultiCropServer::Status::Completed);
        cl.def("getStatus",
            static_cast<enum Fmc::MultiCropServer::Status (Fmc::MultiCropServer::*)()>(
                &Fmc::MultiCropServer::getStatus),
            "Gets the encode status.");
        cl.def("getProgress", static_cast<float (Fmc::MultiCropServer::*)()>(&Fmc::MultiCropServer::getProgress),
            "Gets the encode progress (normalised value between 0 and 1 inclusive).");
    }

    m.def("cropAndEncode",
        static_cast<bool (*)(const std::string&, const std::vector<class Fmc::CropOptions>&,
            const Fmc::EncoderOptions&)>(&Fmc::cropAndEncode),
        "Crops and encodes an input video into 1 or more output videos synchronously.",
        pybind11::return_value_policy::automatic, pybind11::arg("sourceFile"), pybind11::arg("cropList"),
        pybind11::arg_v("options", Fmc::EncoderOptions(), "EncoderOptions()"));

    m.def("cropAndEncodeAsync",
        static_cast<std::shared_ptr<Fmc::MultiCropServer> (*)(const std::string&,
            const std::vector<class Fmc::CropOptions>&, const Fmc::EncoderOptions&)>(&Fmc::cropAndEncodeAsync),
        "Crops and encodes an input video into 1 or more output videos synchronously.",
        pybind11::return_value_policy::automatic, pybind11::arg("sourceFile"), pybind11::arg("cropList"),
        pybind11::arg_v("options", Fmc::EncoderOptions(), "EncoderOptions()"));
}