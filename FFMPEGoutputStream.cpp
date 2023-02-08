
#include "FFMPEGoutputStream.h"

#include <time.h>
#include <Windows.h>
//#include <fstream>

FFMPEGoutputStream::FFMPEGoutputStream(std::string url, int fps, int width, int height, std::string codecID)
  :url(url), width(width), height(height), codecID(codecID)
{
  gopSize = 12;
  maxBFrames = 0;
  pts = 0;

  setFps(fps);

  isOpen = false;
  needWait = false;
}

FFMPEGoutputStream::~FFMPEGoutputStream()
{
  stop();
}

int FFMPEGoutputStream::start() {
  if (pushThread) {
    std::cout << "start error: push thread is running" << std::endl;
    return -1;
  }

  stopSignal = false;
  pushThread = std::make_unique<std::thread>(&FFMPEGoutputStream::run, this);
}

int FFMPEGoutputStream::stop() {
  if (!pushThread) {
    return 0;
  }

  stopSignal = true;
  pushThread->join();
  pushThread = nullptr;
}

int FFMPEGoutputStream::reset(std::string url, int fps, int width, int height, std::string codecID) {
  stop();
  setFps(fps);
  setWidth(width);
  setHeight(height);
  setUrl(url);
  setCodecID(codecID);
  return start();
}

int FFMPEGoutputStream::run() {
  while (!stopSignal) {
    int ret = open();
    while (ret != 0) {
      if (stopSignal) {
        close();
        return 0;
      }
      std::cout << "ffmpeg output stream open error: " << ret << std::endl;
      Sleep(100);
    }

    int frameSize = width * height;

    while (!stopSignal) {
      std::shared_ptr<picFrame> pic;
      {
        std::lock_guard<std::mutex> lock(picFrameQueueMutex);
        if (picFrameQueue.empty()) {
          continue;
        }
        pic = picFrameQueue.front();
        picFrameQueue.pop();
      }

      bool flag = false;
      switch (pic->picFormat) {
      case AV_PIX_FMT_YUV420P:
        // YUV需要保证 data 的长度 >= frameSize* 3/2
        if (pic->data == nullptr || pic->size < frameSize * 3 / 2) {
          break;
        }
        av_image_fill_arrays(avframe->data, avframe->linesize, pic->data, AV_PIX_FMT_YUV420P, width, height, 1);
        flag = true;
        break;
      case AV_PIX_FMT_BGR24:
        if (!swsContext) {
          break;
        }
        // RGB需要保证 data 的长度 >= frameSize * 3
        if (pic->data == nullptr || pic->size < frameSize * 3) {
          break;
        }

        uint8_t *src_data[4];
        int src_linesize[4];
        av_image_fill_arrays(src_data, src_linesize, pic->data, AV_PIX_FMT_BGR24, width, height, 1);
        sws_scale(swsContext, (const uint8_t * const*)src_data, src_linesize, 0, height, avframe->data, avframe->linesize);
        flag = true;
        break;
      default:
        // unknown pic format
        break;
      }

      if (flag) {
        ret = writeFrame();
        if (ret == -2) {
          std::cout << "server error, reconnect" << std::endl;
          break;
        }
      }

      if (pic->data) {
        delete[] pic->data;
      }
    }
    close();
  }
}

int FFMPEGoutputStream::close()
{
  if (outputAVFormatCtx) {
    if (isOpen) {
      av_write_trailer(outputAVFormatCtx);
    }
    avformat_free_context(outputAVFormatCtx);
    outputAVFormatCtx = nullptr;
  }

  if (codecCtx) {
    avcodec_free_context(&codecCtx);
    codecCtx = nullptr;
  }

  if (avframe) {
    av_frame_free(&avframe);
    avframe = nullptr;
  }

  isOpen = false;
  return 0;
}


int FFMPEGoutputStream::writePicture(unsigned char * data, int size, AVPixelFormat picFormat)
{
  std::shared_ptr<picFrame> pic = std::make_shared<picFrame>();
  pic->data = (unsigned char *)malloc(size + 1);
  memcpy(pic->data, data, size);
  pic->size = size;
  pic->picFormat = picFormat;
  std::lock_guard<std::mutex> lock(picFrameQueueMutex);
  if (picFrameQueue.size() > maxQueueSize) {
    std::shared_ptr<picFrame> pic = picFrameQueue.front();
    picFrameQueue.pop();
    if (pic->data) {
      delete[] pic->data;
    }
  }
  picFrameQueue.push(pic);

  return 0;
}

int FFMPEGoutputStream::writeFrame()
{
  avframe->pts = pts;

  int64_t perFrame = 1000L / frameRate.num;
  int ret = avcodec_send_frame(codecCtx, avframe);
  if (ret < 0) {
    std::cout << "Error sending a frame to the encoder: " << ret << std::endl;
    return -1;
  }

  while (ret >= 0) {
    AVPacket pkt = { 0 };

    ret = avcodec_receive_packet(codecCtx, &pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      break;
    else if (ret < 0) {
      std::cout << "Error receiving packet from encoder: " << ret << std::endl;
      return -1;
    }

    if (needWait)
      while (clock() - startTime < pts * perFrame) { Sleep(1); }

    /* rescale output packet timestamp values from codec to stream timebase */
    pkt.stream_index = outputStream->index;
    av_packet_rescale_ts(&pkt, codecCtx->time_base, outputStream->time_base);
    ret = av_interleaved_write_frame(outputAVFormatCtx, &pkt);

    av_packet_unref(&pkt);
    if (ret < 0) {
      std::cout << "Error while writing output packet: " << ret << std::endl;
      return -2;
    }
  }
  pts++;

  return 0;
}
