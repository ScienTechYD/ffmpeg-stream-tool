#include "RTSPinputStream.h"

RTSPinputStream::RTSPinputStream(std::string url) :url(url)
{}

RTSPinputStream::~RTSPinputStream()
{
  close();
}

int RTSPinputStream::init()
{
  if (beInited) {
    return 0;
  }

  AVDictionary *options = NULL;
  av_dict_set(&options, "rtsp_transport", "tcp", 0);
  av_dict_set(&options, "stimeout", "8000000", 0);

  int ret = avformat_open_input(&inputAVFormatCtx, url.c_str(), NULL, &options);
  if (ret < 0) {
    std::cout << "Could not open source url " << url << std::endl;
    close();
    return -1;
  }

  ret = avformat_find_stream_info(inputAVFormatCtx, NULL);
  videoStreamIdx = av_find_best_stream(inputAVFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (videoStreamIdx < 0) {
    std::cout << "Could not find " << av_get_media_type_string(AVMEDIA_TYPE_VIDEO) << " stream\n";
    close();
    return -1;
  }

  inputStream = inputAVFormatCtx->streams[videoStreamIdx];

  if (inputStream->codecpar->codec_id == AV_CODEC_ID_H264) {
    codec = avcodec_find_decoder_by_name("h264_cuvid");
  }
  else if (inputStream->codecpar->codec_id == AV_CODEC_ID_HEVC) {
    codec = avcodec_find_decoder_by_name("hevc_cuvid");
  }
  else {
    codec = avcodec_find_decoder(inputStream->codecpar->codec_id);
  }
  if (!codec) {
    std::cout << "Failed to find codec " << av_get_media_type_string(AVMEDIA_TYPE_VIDEO) << std::endl;
    close();
    return -1;
  }

  codecCtx = avcodec_alloc_context3(codec);
  if (!codecCtx) {
    std::cout << "Failed to allocate the codec context\n";
    close();
    return -1;
  }

  ret = avcodec_parameters_to_context(codecCtx, inputStream->codecpar);
  if (!codecCtx) {
    std::cout << "Failed to copy codec parameters to decoder context\n";
    close();
    return -1;
  }

  ret = avcodec_open2(codecCtx, codec, NULL);
  if (!codecCtx) {
    std::cout << "Failed to open codec\n";
    close();
    return -1;
  }

  width = codecCtx->width;
  height = codecCtx->height;
  codecID = codecCtx->codec_id;
  pixFormat = codecCtx->pix_fmt;

  beInited = true;
  stopToRead = false;
  reading = false;
  return 0;
}

int RTSPinputStream::close()
{
  if (reading) {
    return -1;
  }

  beInited = false;

  if (codecCtx) {
    avcodec_free_context(&codecCtx);
    codecCtx = nullptr;
  }

  if (inputAVFormatCtx) {
    avformat_close_input(&inputAVFormatCtx);
    inputAVFormatCtx = nullptr;
  }

  return 0;
}

int RTSPinputStream::readPacket(callback callback, onStop onStop)
{
  {
    std::lock_guard<std::mutex> lock(readingMutex);
    if (reading) {
      return -1;
    }
    reading = true;
  }

  AVPacket *packet = av_packet_alloc();
  if (!packet) {
    setReadingFalse();
    return -1;
  }

  while (!stopToRead && av_read_frame(inputAVFormatCtx, packet) >= 0) {
    if (packet->stream_index == videoStreamIdx) {
      if (callback) {
        callback(packet);
      }
    }
  }

  if (onStop) {
    onStop();
  }

  av_packet_free(&packet);
  setReadingFalse();
  return 0;
}

int RTSPinputStream::readFrame(decodeCallback callback, onStop onStop)
{
  {
    std::lock_guard<std::mutex> lock(readingMutex);
    if (reading) {
      return -1;
    }
    reading = true;
  }

  AVFrame *avframe = av_frame_alloc();
  if (!avframe) {
    setReadingFalse();
    return -1;
  }

  AVPacket *packet = av_packet_alloc();
  if (!packet) {
    setReadingFalse();
    return -1;
  }

  while (!stopToRead && av_read_frame(inputAVFormatCtx, packet) >= 0) {
    if (packet->stream_index == videoStreamIdx) {
      int ret = avcodec_send_packet(codecCtx, packet);
      if (ret < 0) {
        std::cout << "Error submitting a packet for decoding\n";
        break;
      }

      while (ret >= 0) {
        ret = avcodec_receive_frame(codecCtx, avframe);
        if (ret < 0) {
          if (ret == AVERROR_EOF) {
            setReadingFalse();
            return 0;
          }
          if (ret == AVERROR(EAGAIN)) {
            break;
          }

          std::cout << "Error during decoding\n";
          setReadingFalse();
          return ret;
        }

        if (codecCtx->codec->type == AVMEDIA_TYPE_VIDEO) {
          if (callback) {
            callback(avframe);
          }
        }

        av_frame_unref(avframe);
      }
    }

  }

  if (onStop) {
    onStop();
  }

  av_frame_free(&avframe);
  setReadingFalse();
  return 0;
}

void RTSPinputStream::stop()
{
  stopToRead = true;
}

void RTSPinputStream::setReadingFalse()
{
  std::lock_guard<std::mutex> lock(readingMutex);
  reading = false;
}
