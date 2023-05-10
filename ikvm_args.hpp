#pragma once

#include <string>

namespace ikvm
{
/*
 * @class Args
 * @brief Command line argument parser and storage
 */
class Args
{
  public:
    /*
     * @struct CommandLine
     * @brief Stores the original command line arguments for later use
     */
    struct CommandLine
    {
        /*
         * @brief Constructs CommandLine object
         *
         * @param[in] c - Number of arguments
         * @param[in] v - Array of arguments
         */
        CommandLine(int c, char** v) : argc(c), argv(v) {}
        ~CommandLine() = default;
        CommandLine(const CommandLine&) = default;
        CommandLine& operator=(const CommandLine&) = default;
        CommandLine(CommandLine&&) = default;
        CommandLine& operator=(CommandLine&&) = default;

        int argc;
        char** argv;
    };

    /*
     * @brief Constructs Args object
     *
     * @param[in] argc - The number of arguments in the command line call
     * @param[in] argv - The array of arguments from the command line
     */
    Args(int argc, char* argv[]);
    ~Args() = default;
    Args(const Args&) = default;
    Args& operator=(const Args&) = default;
    Args(Args&&) = default;
    Args& operator=(Args&&) = default;

    /*
     * @brief Get the original command line arguments
     *
     * @return Reference to the CommandLine structure storing the original
     *         command line arguments
     */
    inline const CommandLine& getCommandLine() const
    {
        return commandLine;
    }

    /*
     * @brief Get the desired video frame rate
     *
     * @return Value of the desired frame rate in frames per second
     */
    inline int getFrameRate() const
    {
        return frameRate;
    }

    /*
     * @brief Get the video subsampling
     *
     * @return Value of the video subsampling
     */
    inline int getSubsampling() const
    {
        return subsampling;
    }

    /*
     * @brief Get the path to the USB keyboard device
     *
     * @return Reference to the string storing the path to the keyboard device
     */
    inline const std::string& getKeyboardPath() const
    {
        return keyboardPath;
    }

    /*
     * @brief Get the path to the USB mouse device
     *
     * @return Reference to the string storing the path to the mouse device
     */
    inline const std::string& getPointerPath() const
    {
        return pointerPath;
    }

    /*
     * @brief Get the name of UDC
     *
     * @return Reference to the string storing the name of UDC
     */
    inline const std::string& getUdcName() const
    {
        return udcName;
    }

    /*
     * @brief Get the path to the V4L2 video device
     *
     * @return Reference to the string storing the path to the video device
     */
    inline const std::string& getVideoPath() const
    {
        return videoPath;
    }

    /*
     * @brief Get the identical frames detection setting
     *
     * @return True if identical frames detection is enabled
     */
    inline bool getCalcFrameCRC() const
    {
        return calcFrameCRC;
    }

  private:
    /* @brief Prints the application usage to stderr */
    void printUsage();

    /*
     * @brief Desired frame rate (in frames per second) of the video
     *        stream
     */
    int frameRate;
    /* @brief Desired subsampling (0: 444, 1: 420) */
    int subsampling;
    /* @brief Path to the USB keyboard device */
    std::string keyboardPath;
    /* @brief Path to the USB mouse device */
    std::string pointerPath;
    /* @brief Name of UDC */
    std::string udcName;
    /* @brief Path to the V4L2 video device */
    std::string videoPath;
    /* @brief Identical frames detection */
    bool calcFrameCRC;
    /* @brief Original command line arguments passed to the application */
    CommandLine commandLine;
};

} // namespace ikvm
