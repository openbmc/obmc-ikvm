#include "ikvm_args.hpp"

#include <getopt.h>
#include <rfb/rfb.h>
#include <stdio.h>
#include <stdlib.h>

namespace ikvm
{
Args::Args(int argc, char* argv[]) :
    frameRate(30), subsampling(0), pixelformat(0), calcFrameCRC{false},
    commandLine(argc, argv)
{
    int option;
    const char* opts = "f:s:m:hk:p:u:v:c";
    struct option lopts[] = {
        {"frameRate", 1, 0, 'f'},   {"subsampling", 1, 0, 's'},
        {"pixelformat", 1, 0, 'm'}, {"help", 0, 0, 'h'},
        {"keyboard", 1, 0, 'k'},    {"mouse", 1, 0, 'p'},
        {"udcName", 1, 0, 'u'},     {"videoDevice", 1, 0, 'v'},
        {"calcCRC", 0, 0, 'c'},     {0, 0, 0, 0}};

    while ((option = getopt_long(argc, argv, opts, lopts, NULL)) != -1)
    {
        switch (option)
        {
            case 'f':
                frameRate = (int)strtol(optarg, NULL, 0);
                if (frameRate < 0 || frameRate > 60)
                    frameRate = 30;
                break;
            case 's':
                subsampling = (int)strtol(optarg, NULL, 0);
                if (subsampling < 0 || subsampling > 1)
                    subsampling = 0;
                break;
            case 'm':
                pixelformat = (int)strtol(optarg, NULL, 0);
                if (pixelformat < 0 || pixelformat > 1)
                    pixelformat = 0;
                break;
            case 'h':
                printUsage();
                exit(0);
            case 'k':
                keyboardPath = std::string(optarg);
                break;
            case 'p':
                pointerPath = std::string(optarg);
                break;
            case 'u':
                udcName = std::string(optarg);
                break;
            case 'v':
                videoPath = std::string(optarg);
                break;
            case 'c':
                calcFrameCRC = true;
                break;
        }
    }
}

void Args::printUsage()
{
    // use fprintf(stderr to match rfbUsage()
    fprintf(stderr, "OpenBMC IKVM daemon\n");
    fprintf(stderr, "Usage: obmc-ikvm [options]\n");
    fprintf(stderr, "-f frame rate          try this frame rate\n");
    fprintf(stderr, "-s subsampling         try this subsampling\n");
    fprintf(stderr, "-m pixelformat         try this V4L2 pixel format\n");
    fprintf(stderr, "-h, --help             show this message and exit\n");
    fprintf(stderr, "-k device              HID keyboard gadget device\n");
    fprintf(stderr, "-p device              HID mouse gadget device\n");
    fprintf(stderr,
            "-u udc name            UDC that HID gadget will connect to\n");
    fprintf(stderr, "-v device              V4L2 device\n");
    fprintf(
        stderr,
        "-c, --calcCRC          Calculate CRC for each frame to save bandwidth\n");
    rfbUsage();
}

} // namespace ikvm
