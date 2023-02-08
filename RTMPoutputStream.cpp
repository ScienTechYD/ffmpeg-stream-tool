
#include "RTMPoutputStream.h"

#include <time.h>
#include <Windows.h>
//#include <fstream>

int RTMPoutputStream::open()
{
  if (isOpen) {
    return 0;
  }

  codec = avcodec_find_encoder_by_name(codecID.c_str());
  if (!codec) {
    std::cout << "Codec " << codecID << " not found" << std::endl;
    close();
    return -1;
  }
  if (codec->pix_fmts == NULL) {
    std::cout << "Codec " << codecID << " pix_fmts not found" << std::endl;
    close();
    return -1;
  }

  codecCtx = avcodec_alloc_context3(codec);
  if (!codecCtx) {
    std::cout << "Could not allocate video codec context" << std::endl;
    close();
    return -1;
  }

  codecCtx->width = width;
  codecCtx->height = height;
  codecCtx->time_base = timeBase;
  codecCtx->framerate = frameRate;

  codecCtx->gop_size = gopSize;
  codecCtx->max_b_frames = maxBFrames;

  if (codec->id == AV_CODEC_ID_H264)
    av_opt_set(codecCtx->priv_data, "preset", "slow", 0);

  codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  codecCtx->pix_fmt = codec->pix_fmts[0];

  swsContext = sws_getContext(width, height, AV_PIX_FMT_BGR24,
    width, height, codec->pix_fmts[0], SWS_BILINEAR, NULL, NULL, NULL);
  if (!swsContext) {
    printf("Impossible to create scale context for the conversion "
      "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
      av_get_pix_fmt_name(AV_PIX_FMT_BGR24), width, height,
      av_get_pix_fmt_name(codec->pix_fmts[0]), width, height);
  }

  int ret = avcodec_open2(codecCtx, codec, NULL);
  if (ret < 0) {
    std::cout << "Could not open codec: " << ret << std::endl;
    close();
    return -1;
  }

  ret = avformat_alloc_output_context2(&outputAVFormatCtx, NULL, "flv", url.c_str());
  if (ret < 0) {
    std::cout << "Could not allocate output context:  " << ret << std::endl;
    close();
    return -1;
  }

  //outputAVFormatCtx->oformat->video_codec = codec->id;

  outputStream = avformat_new_stream(outputAVFormatCtx, NULL);
  if (!outputStream) {
    std::cout << "Could not allocate output stream" << std::endl;
    close();
    return -1;
  }

  avcodec_parameters_from_context(outputStream->codecpar, codecCtx);
  outputStream->time_base = timeBase;

  if (!(outputAVFormatCtx->oformat->flags & AVFMT_NOFILE)) {
    ret = avio_open(&outputAVFormatCtx->pb, url.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
      std::cout << "Could not open avio: " << ret << std::endl;
      close();
      return -1;
    }
  }

  AVDictionary *options = NULL;
  if (format == "rtsp") {
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "stimeout", "8000000", 0);
  }

  ret = avformat_write_header(outputAVFormatCtx, &options);
  if (ret < 0) {
    std::cout << "Could not write output stream header url: " << url << ", ret:  " << ret << std::endl;
    close();
    return -1;
  }

  avframe = av_frame_alloc();

  avframe->format = codecCtx->pix_fmt;
  avframe->width = codecCtx->width;
  avframe->height = codecCtx->height;

  ret = av_frame_get_buffer(avframe, 0);
  if (ret < 0) {
    std::cout << "Could not allocate frame:  " << ret << std::endl;
    close();
    return -1;
  }

  startTime = clock();
  isOpen = true;

  return 0;
}
