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

using namespace Fmc;

void bindMultiCrop(pybind11::module& m)
{
    m.doc() = "Crops and encodes an input video into 1 or more output videos";

    pybind11::class_<CropPosition, std::shared_ptr<CropPosition>>(m, "CropPosition", "")
        .def(pybind11::init<uint32_t, uint32_t>())
        .def(pybind11::init([]() { return new CropPosition(); }))
        .def(pybind11::init([](CropPosition const& o) { return new CropPosition(o); }))
        .def_readwrite("top", &CropPosition::m_top)
        .def_readwrite("left", &CropPosition::m_left)
        .def("assign", static_cast<CropPosition& (CropPosition::*)(const CropPosition&)>(&CropPosition::operator=), "",
            pybind11::return_value_policy::automatic, pybind11::arg("other"));

    pybind11::class_<CropOptions, std::shared_ptr<CropOptions>>(m, "CropOptions", "")
        .def(pybind11::init([]() { return new CropOptions(); }))
        .def(pybind11::init([](CropOptions const& o) { return new CropOptions(o); }))
        .def_readwrite("cropList", &CropOptions::m_cropList)
        .def_readwrite("resolution", &CropOptions::m_resolution)
        .def_readwrite("fileName", &CropOptions::m_fileName)
        .def_readwrite("skipRegions", &CropOptions::m_skipRegions)
        .def("assign", static_cast<CropOptions& (CropOptions::*)(const CropOptions&)>(&CropOptions::operator=), "",
            pybind11::return_value_policy::automatic, pybind11::arg("other"));

    {
        pybind11::class_<MultiCropServer, std::shared_ptr<MultiCropServer>> cl(m, "MultiCropServer", "");
        pybind11::enum_<MultiCropServer::Status>(cl, "Status", "")
            .value("Failed", MultiCropServer::Status::Failed)
            .value("Running", MultiCropServer::Status::Running)
            .value("Completed", MultiCropServer::Status::Completed);
        cl.def("getStatus", static_cast<MultiCropServer::Status (MultiCropServer::*)()>(&MultiCropServer::getStatus),
            "Gets the encode status.");
        cl.def("getProgress", static_cast<float (MultiCropServer::*)()>(&MultiCropServer::getProgress),
            "Gets the encode progress (normalised value between 0 and 1 inclusive).");
    }

    m.def("cropAndEncode",
        static_cast<bool (*)(const std::string&, const std::vector<CropOptions>&, const EncoderOptions&)>(
            &cropAndEncode),
        "Crops and encodes an input video into 1 or more output videos synchronously.",
        pybind11::return_value_policy::automatic, pybind11::arg("sourceFile"), pybind11::arg("cropList"),
        pybind11::arg_v("options", EncoderOptions(), "EncoderOptions()"));

    m.def("cropAndEncode",
        static_cast<bool (*)(const std::shared_ptr<Stream>&, const std::vector<CropOptions>&, const EncoderOptions&)>(
            &cropAndEncode),
        "Crops and encodes an input stream into 1 or more output videos synchronously.",
        pybind11::return_value_policy::automatic, pybind11::arg("stream"), pybind11::arg("cropList"),
        pybind11::arg_v("options", EncoderOptions(), "EncoderOptions()"));

    m.def("cropAndEncodeAsync",
        static_cast<std::shared_ptr<MultiCropServer> (*)(
            const std::string&, const std::vector<CropOptions>&, const EncoderOptions&)>(&cropAndEncodeAsync),
        "Crops and encodes an input video into 1 or more output videos asynchronously.",
        pybind11::return_value_policy::automatic, pybind11::arg("sourceFile"), pybind11::arg("cropList"),
        pybind11::arg_v("options", EncoderOptions(), "EncoderOptions()"));

    m.def("cropAndEncodeAsync",
        static_cast<std::shared_ptr<MultiCropServer> (*)(const std::shared_ptr<Stream>&,
            const std::vector<CropOptions>&, const EncoderOptions&)>(&cropAndEncodeAsync),
        "Crops and encodes an input stream into 1 or more output videos asynchronously.",
        pybind11::return_value_policy::automatic, pybind11::arg("stream"), pybind11::arg("cropList"),
        pybind11::arg_v("options", EncoderOptions(), "EncoderOptions()"));
}

extern void bindFrameReader(pybind11::module& m);

PYBIND11_MODULE(pyMultiCrop, m)
{
    bindFrameReader(m);
    bindMultiCrop(m);
}
