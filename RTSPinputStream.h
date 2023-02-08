#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <mutex>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

typedef std::function<void(AVPacket*)> callback;
typedef std::function<void(AVFrame*)> decodeCallback;
typedef std::function<void()> onStop;

class RTSPinputStream {
public:
  RTSPinputStream(std::string url);
  ~RTSPinputStream();

  int init();
  // 若 readPacket 或 readFrame 没有停止 则关闭失败
  int close();

  int readPacket(callback callback, onStop onStop = nullptr);
  int readFrame(decodeCallback callback, onStop onStop = nullptr);
  void stop();

  int getWidth() { return width; }
  int getHeight() { return height; }
  AVCodecID getCodecID() { return codecID; }
  AVPixelFormat getPixFormat() { return pixFormat; }
private:

  void setReadingFalse();

  const AVCodec *codec = nullptr;
  AVCodecContext *codecCtx = nullptr;
  AVFormatContext *inputAVFormatCtx = nullptr;
  AVStream *inputStream = nullptr;

  std::string url;
  int width;
  int height;
  AVCodecID codecID;
  AVPixelFormat pixFormat;
  int videoStreamIdx;

  bool beInited = false;
  bool stopToRead;
  std::mutex readingMutex;
  bool reading;
};
