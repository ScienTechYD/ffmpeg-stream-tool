

#include "FFMPEGoutputStream.h"
#include "RTSPoutputStream.h"
#include "RTMPoutputStream.h"
#include "RTSPinputStream.h"

extern "C"
{
#include <libswscale/swscale.h>
}

#include <thread>
#include <Windows.h>
#include <opencv2/opencv.hpp>

struct SwsContext *sws_ctx = NULL;
uint8_t *dst_data[4];
int dst_linesize[4];

void readPacketCallback(AVPacket *packet) {
  static int index = 0;
  index++;
  std::cout << "get packet " << index << " pts " << packet->pts << std::endl;
}

void readFrameCallback(AVFrame *frame) {
  static int index = 0;
  index++;
  std::cout << "get frame " << index << " pts " << frame->pts << std::endl;
  if (sws_ctx) {
    sws_scale(sws_ctx, (const uint8_t * const*)frame->data, frame->linesize, 0, frame->height,
      dst_data, dst_linesize);

    cv::Mat image = cv::Mat(frame->height, frame->width, CV_8UC3, dst_data[0], 0);
    cv::imshow("test", image);
    cv::waitKey(1);
  }
}

int main()
{

  //cv::Mat image = cv::imread("D:\\YD\\cjh\\input.jpg");
  //cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
  //int width = image.rows;
  //int height = image.cols;

  //int th = 50;

  //cv::Mat out = cv::Mat::zeros(width, height, CV_8UC1);

  //for (int i = 0; i < width; i++) {
  //  for (int j = 0; j < height; j++) {
  //    if (image.at<uchar>(i, j) < th) {
  //      out.at<uchar>(i, j) = 0;
  //    }
  //    else {
  //      out.at<uchar>(i, j) = 255;
  //    }
  //  }
  //}

  //cv::imwrite("D:\\YD\\cjh\\output.jpg", out);
  //cv::waitKey(0);
  //cv::destroyAllWindows();

  /* output test */
  cv::VideoCapture capture;
  capture.open("D:/YD/video/output2.264");
  cv::UMat frame;
  int fps = capture.get(cv::CAP_PROP_FPS);
  int width = capture.get(cv::CAP_PROP_FRAME_WIDTH);
  int height = capture.get(cv::CAP_PROP_FRAME_HEIGHT);

  //std::shared_ptr<FFMPEGoutputStream> outputStream(new RTSPoutputStream("rtsp://localhost:8554/lives/livestreams", fps, width, height, "libx264"));
  std::shared_ptr<FFMPEGoutputStream> outputStream(new RTMPoutputStream("rtmp://localhost/live/livestream", fps, width, height, "h264_nvenc"));
  outputStream->start();
  //outputStream->setNeedWait(true);

  int count = 3;

  while (count) {

    if (capture.read(frame)) {

      cv::UMat tmpFrame;
      // cv::cvtColor(frame, tmpFrame, cv::COLOR_BGR2YUV_I420);
      //cv::cvtColor(frame, tmpFrame, cv::COLOR_BGR2RGB);
      //cv::resize(frame, tmpFrame, cv::Size(800, 800));
      //int frame_size = frame.cols * frame.rows;
      unsigned char* data = frame.getMat(cv::ACCESS_READ).data;

      //outputStream->writeYUV420Frame(data);
      //outputStream->writeBGR24Frame(data);
      int imageSize = width * height * frame.elemSize();
      outputStream->writePicture(data, imageSize, AV_PIX_FMT_BGR24);
      Sleep(30);

    }
    else {
      capture.release();
      capture.open("D:/YD/video/output2.264");
      std::cout << "restart" << std::endl;
      //count--;
    }

  }

  outputStream->stop();

  //cv::Mat image = cv::imread("D:\\YD\\testpic\\test_1.png");
  ////cv::Mat frame1;
  //int width = image.cols;
  //int height = image.rows;
  ////std::shared_ptr<FFMPEGoutputStream> outputStream(new RTSPoutputStream("rtsp://localhost:8554/lives/livestreams", 120, width, height, "libx264"));
  //std::shared_ptr<FFMPEGoutputStream> outputStream(new RTMPoutputStream("rtmp://localhost/live/livestream", 100, width, height, "libx264"));
  //outputStream->setNeedWait(true);

  //while (true) {
  //  int imageSize = width * height * image.elemSize();
  //  outputStream->writePicture(image.data, imageSize, AV_PIX_FMT_BGR24);
  //  Sleep(10);
  //}

  //outputStream->close();

  /* input test */
  //std::string url = "rtsp://172.24.122.170:8555/video/out2.264";
  //RTSPinputStream inputStream(url);
  //int ret = inputStream.init();
  //if (ret < 0) {
  //  std::cout << "rtsp input init error" << std::endl;
  //  exit(1);
  //}

  //sws_ctx = sws_getContext(inputStream.getWidth(), inputStream.getHeight(), inputStream.getPixFormat(),
  //  inputStream.getWidth(), inputStream.getHeight(), AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
  //av_image_alloc(dst_data, dst_linesize,
  //  inputStream.getWidth(), inputStream.getHeight(), AV_PIX_FMT_RGB24, 1);


  //std::thread readThread(&RTSPinputStream::readPacket, &inputStream, readPacketCallback);
  //Sleep(100 * 1000);
  //inputStream.stop();
  //readThread.join();
  //inputStream.close();

  //sws_freeContext(sws_ctx);
  //av_freep(&dst_data[0]);
  return 0;
}
