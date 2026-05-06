#include "ikvm_args.hpp"

#include <unistd.h>

#include <cstring>
#include <vector>

#include <gtest/gtest.h>

namespace ikvm
{

class ArgsTest : public ::testing::Test
{
  protected:
    // Helper to create argv array
    static char** createArgv(const std::vector<std::string>& args)
    {
        char** argv = new char*[args.size()];
        for (size_t i = 0; i < args.size(); ++i)
        {
            argv[i] = new char[args[i].size() + 1];
            strcpy(argv[i], args[i].c_str());
        }
        return argv;
    }

    // Helper to clean up argv array
    static void deleteArgv(char** argv, size_t argc)
    {
        for (size_t i = 0; i < argc; ++i)
        {
            delete[] argv[i];
        }
        delete[] argv;
    }

    // Reset getopt state before each test
    void SetUp() override
    {
        optind = 1;
    }
};

TEST_F(ArgsTest, DefaultValuesWithNoArguments)
{
    std::vector<std::string> args = {"obmc-ikvm"};
    char** argv = createArgv(args);

    Args parser(args.size(), argv);

    EXPECT_EQ(parser.getFrameRate(), 30);
    EXPECT_EQ(parser.getSubsampling(), 0);
    EXPECT_TRUE(parser.getKeyboardPath().empty());
    EXPECT_TRUE(parser.getPointerPath().empty());
    EXPECT_TRUE(parser.getUdcName().empty());
    EXPECT_TRUE(parser.getVideoPath().empty());
    EXPECT_FALSE(parser.getCalcFrameCRC());

    deleteArgv(argv, args.size());
}

TEST_F(ArgsTest, ParseValidFrameRate)
{
    std::vector<std::string> args = {"obmc-ikvm", "-f", "60"};
    char** argv = createArgv(args);

    Args parser(args.size(), argv);

    EXPECT_EQ(parser.getFrameRate(), 60);

    deleteArgv(argv, args.size());
}

TEST_F(ArgsTest, ParseValidSubsampling)
{
    std::vector<std::string> args = {"obmc-ikvm", "-s", "1"};
    char** argv = createArgv(args);

    Args parser(args.size(), argv);

    EXPECT_EQ(parser.getSubsampling(), 1);

    deleteArgv(argv, args.size());
}

TEST_F(ArgsTest, ParseDevicePaths)
{
    std::vector<std::string> args = {"obmc-ikvm",  "-v", "/dev/video0", "-k",
                                     "/dev/hidg0", "-p", "/dev/hidg1"};
    char** argv = createArgv(args);

    Args parser(args.size(), argv);

    EXPECT_EQ(parser.getVideoPath(), "/dev/video0");
    EXPECT_EQ(parser.getKeyboardPath(), "/dev/hidg0");
    EXPECT_EQ(parser.getPointerPath(), "/dev/hidg1");

    deleteArgv(argv, args.size());
}

TEST_F(ArgsTest, ParseUdcName)
{
    std::vector<std::string> args = {"obmc-ikvm", "-u", "f0b00000.usb"};
    char** argv = createArgv(args);

    Args parser(args.size(), argv);

    EXPECT_EQ(parser.getUdcName(), "f0b00000.usb");

    deleteArgv(argv, args.size());
}

TEST_F(ArgsTest, ParseCalcCRCFlag)
{
    std::vector<std::string> args = {"obmc-ikvm", "-c"};
    char** argv = createArgv(args);

    Args parser(args.size(), argv);

    EXPECT_TRUE(parser.getCalcFrameCRC());

    deleteArgv(argv, args.size());
}

TEST_F(ArgsTest, FrameRateOutOfRangeHigh)
{
    std::vector<std::string> args = {"obmc-ikvm", "-f", "100"};
    char** argv = createArgv(args);

    Args parser(args.size(), argv);

    EXPECT_EQ(parser.getFrameRate(), 30);

    deleteArgv(argv, args.size());
}

TEST_F(ArgsTest, FrameRateOutOfRangeLow)
{
    std::vector<std::string> args = {"obmc-ikvm", "-f", "-5"};
    char** argv = createArgv(args);

    Args parser(args.size(), argv);

    EXPECT_EQ(parser.getFrameRate(), 30);

    deleteArgv(argv, args.size());
}

TEST_F(ArgsTest, SubsamplingOutOfRange)
{
    std::vector<std::string> args = {"obmc-ikvm", "-s", "5"};
    char** argv = createArgv(args);

    Args parser(args.size(), argv);

    EXPECT_EQ(parser.getSubsampling(), 0);

    deleteArgv(argv, args.size());
}

TEST_F(ArgsTest, AllArgumentsCombined)
{
    std::vector<std::string> args = {
        "obmc-ikvm",  "-f",          "45",           "-s",         "1",
        "-v",         "/dev/video0", "-k",           "/dev/hidg0", "-p",
        "/dev/hidg1", "-u",          "f0b00000.usb", "-c"};
    char** argv = createArgv(args);

    Args parser(args.size(), argv);

    EXPECT_EQ(parser.getFrameRate(), 45);
    EXPECT_EQ(parser.getSubsampling(), 1);
    EXPECT_EQ(parser.getVideoPath(), "/dev/video0");
    EXPECT_EQ(parser.getKeyboardPath(), "/dev/hidg0");
    EXPECT_EQ(parser.getPointerPath(), "/dev/hidg1");
    EXPECT_EQ(parser.getUdcName(), "f0b00000.usb");
    EXPECT_TRUE(parser.getCalcFrameCRC());

    deleteArgv(argv, args.size());
}

} // namespace ikvm
