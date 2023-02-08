#pragma once

#include <iostream>
#include <string>
#include "FFMPEGoutputStream.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

class RTMPoutputStream : public FFMPEGoutputStream {
protected:
  virtual int open() override;
public:
  RTMPoutputStream(std::string url, int fps, int width, int height, std::string codecID) :FFMPEGoutputStream(url, fps, width, height, codecID) {}
  virtual ~RTMPoutputStream() {};
};
