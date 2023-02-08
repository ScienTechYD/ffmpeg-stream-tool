#pragma once

#include <iostream>
#include <string>
#include <queue>
#include <thread>
#include <mutex>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class FFMPEGoutputStream {
public:
  FFMPEGoutputStream(std::string url, int fps, int width, int height, std::string codecID);
  ~FFMPEGoutputStream();

  void setFps(int fps)
  {
    // time base 1/fps
    timeBase.num = 1;
    timeBase.den = fps;

    frameRate.num = fps;
    frameRate.den = 1;
  }

  int writePicture(unsigned char* data, int size, AVPixelFormat picFormat);

  int getWidth() { return width; }
  int getHeight() { return height; }
  AVPixelFormat getPixFormat() { return pixFormat; }
  void setNeedWait(bool needWait) { this->needWait = needWait; }
  void setMaxQueueSize(int maxQueueSize) { this->maxQueueSize = maxQueueSize; }

  int start();
  int reset(std::string url, int fps, int width, int height, std::string codecID);
  int stop();

protected:
  struct picFrame {
    unsigned char *data;
    int size;
    AVPixelFormat picFormat;
  };

  virtual int open() = 0;
  virtual int close();

  int run();
  int writeFrame();

  std::queue<std::shared_ptr<picFrame>> picFrameQueue;
  std::mutex picFrameQueueMutex;
  std::unique_ptr<std::thread> pushThread;

  bool stopSignal = false;
  int maxQueueSize = 10;

  void setUrl(std::string url) { this->url = url; }
  void setWidth(int width) { this->width = width; }
  void setHeight(int height) { this->height = height; }
  void setCodecID(std::string codecID) { this->codecID = codecID; }

  const AVCodec *codec = nullptr;
  AVCodecContext *codecCtx = nullptr;
  AVFormatContext *outputAVFormatCtx = nullptr;
  AVStream *outputStream = nullptr;
  AVRational timeBase;
  AVRational frameRate;
  AVFrame *avframe = nullptr;
  SwsContext *swsContext = nullptr;

  std::string url;
  std::string format;
  int width;
  int height;
  std::string codecID;
  AVPixelFormat pixFormat;
  int gopSize;
  int maxBFrames;
  int64_t pts;
  clock_t startTime;

  bool needWait;
  bool isOpen;
};
