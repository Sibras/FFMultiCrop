FFMultiCrop
=============
[![Github All Releases](https://img.shields.io/github/downloads/Sibras/FFMultiCrop/total.svg)](https://github.com/Sibras/FFMultiCrop/releases)
[![GitHub release](https://img.shields.io/github/release/Sibras/FFMultiCrop.svg)](https://github.com/Sibras/FFMultiCrop/releases/latest)
[![GitHub issues](https://img.shields.io/github/issues/Sibras/FFMultiCrop.svg)](https://github.com/Sibras/FFMultiCrop/issues)
[![license](https://img.shields.io/github/license/Sibras/FFMultiCrop.svg)](https://github.com/Sibras/FFMultiCrop/blob/master/LICENSE)
[![donate](https://img.shields.io/badge/donate-link-brightgreen.svg)](https://shiftmediaproject.github.io/8-donate/)

## About

This project provides a library the wraps Ffmpeg decoding/encoding functionality into a simple to use c++ library.
It is designed to allow for easily generating multiple cropped output videos from a single source video.
Each output video can specify different per frame crop options and output resolutions.

## Example:
To use this library you can simply start a crop and encoding job by passing in the input video file and a vector of crop options.
~~~~
CropOptions options1 = {{{0, 0}, {0, 1}}/*list of crop positions for 2 frames*/, {60, 40}/*resolution of output*/, "outFileName.mkv"};
std::vector<CropOptions> cropOps;
cropOps.push_back(options1);
if (!cropAndEncode(fileName, cropOps)) {
    // Operation has failed
}
~~~~
Each CropOptions entry will result in a seperate encoded video output being created with the specified parameters.
Crop options can be used to specify the output fileName, the output video resolution (must be constant for all frames), and then a list of crop offsets for each frame in the output video.
The output video will then be encoded using the specified crop lists where the output will contain as many frames as are specified in the input list.
Should the input list specify fewer crop values than the input videos total frames then the final output will be cut short to match the input crop list. An error will occur if the input video is however shorter than the input crop list.

Encoding can also be performed asynchronously.
~~~~
auto server = cropAndEncode(fileName, cropOps);
~~~~
This will return a server object that can be used to check the status of the encode.
~~~~
while (server->getStatus() == MultiCropServer::Status::Running) {
    auto progress = server->getProgress();
     ....
}
if(server->getStatus() == MultiCropServer::Status::Failed) {
    // Operation has failed
}
~~~~
Both crop and encode functions support an optional 3rd parameter that can be used to specify the encoder options to be used.
This can be used to control the output codec used (h264, h265 etc.), the encoder preset, and the encoder quality (encodes all use CRF based constant quality encoding).
~~~~
EncoderOptions options;
options.m_type = EncodeType::h264;
options.m_quality = 125;
options.m_preset = EncoderOptions::Preset::Ultrafast;
~~~~