#include <filesystem>
#include <format>
#include <iostream>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <string>

#include "progressbar.hpp"

using std::filesystem::path;

using Pixel = cv::Point3_<uint8_t>;

auto sqnorm(const Pixel& px) { return px.x * px.x + px.y * px.y + px.z * px.z; }

#define FOURCC(cc) cv::VideoWriter::fourcc((cc)[0], (cc)[1], (cc)[2], (cc)[3])

int main(int argc, const char** argv) {
    cv::VideoCapture cap(argv[1]);
    if (!cap.isOpened()) {
        std::cerr << "ERROR! Unable to open camera\n";
        return -1;
    }
    auto framecount = cap.get(cv::CAP_PROP_FRAME_COUNT);
    auto width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    auto height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    auto framerate = cap.get(cv::CAP_PROP_FPS);
    cv::Mat meanbuf = cv::Mat::zeros(height, width, CV_64FC3);
    cv::Mat meanframe = cv::Mat::zeros(height, width, CV_8UC3);
    cv::Mat maxbuf = cv::Mat::zeros(height, width, CV_8UC3);
    cv::Mat redbuf = cv::Mat::zeros(height, width, CV_8UC3);
    cv::Mat frame, tmp;
    path srcpth(argv[1]);
    path dstdir(argv[2]);
    // auto mean1writer = cv::VideoWriter(dstdir / (srcpth.stem().string() + "_mean1.mp4"), FOURCC("avc1"), framerate,
    //                                    cv::Size(width, height));
    // auto mean2writer = cv::VideoWriter(dstdir / (srcpth.stem().string() + "_mean2.mp4"), FOURCC("avc1"), framerate,
    //                                    cv::Size(width, height));
    auto maxwriter = cv::VideoWriter(dstdir / (srcpth.stem().string() + "_max.mp4"), FOURCC("avc1"), framerate,
                                     cv::Size(width, height));
    auto redwriter = cv::VideoWriter(dstdir / (srcpth.stem().string() + "_red.mp4"), FOURCC("avc1"), framerate,
                                     cv::Size(width, height));

    size_t count = 0;
    progressbar bar(framecount);

    for (; count < framecount; count++) {
        bar.update();
        cap.read(frame);
        if (frame.empty()) {
            std::cerr << "ERROR! blank frame grabbed\n";
            break;
        }
        meanbuf += frame;
        // tmp = meanbuf / framecount;
        // tmp.convertTo(meanframe, CV_8UC3);
        // mean1writer << meanframe;
        //
        // tmp = meanbuf / count;
        // mean2writer << meanframe;

        maxbuf.forEach<Pixel>([frame](Pixel& pix, const int pos[]) {
            auto y = pos[0], x = pos[1];
            const Pixel* framepix = frame.ptr<Pixel>(y, x);
            if (sqnorm(pix) < sqnorm(*framepix)) {
                pix = *framepix;
            }
        });
        maxwriter << maxbuf;

        redbuf.forEach<Pixel>([frame](Pixel& pix, const int pos[]) {
            auto y = pos[0], x = pos[1];
            const Pixel* framepix = frame.ptr<Pixel>(y, x);
            if (pix.x < framepix->x) {
                pix = *framepix;
            }
        });
        redwriter << redbuf;
    }
    cv::imwrite(dstdir / (srcpth.stem().string() + "_mean.png"), meanbuf / count);
    cv::imwrite(dstdir / (srcpth.stem().string() + "_max.png"), maxbuf);
    cv::imwrite(dstdir / (srcpth.stem().string() + "_red.png"), redbuf);
}
