#pragma once

#include "ikvm_input.hpp"

#include <string>

namespace ikvm
{

/*
 * @class Video
 * @brief Sets up the V4L2 video device and performs read operations
 */
class Video
{
  public:
    /*
     * @brief Constructs Video object
     *
     * @param[in] p     - Path to the V4L2 video device
     * @param[in] input - Reference to the Input object
     * @param[in] fr    - desired frame rate of the video
     */
    Video(const std::string& p, Input& input, int fr = 30);
    ~Video();
    Video(const Video&) = default;
    Video& operator=(const Video&) = default;
    Video(Video&&) = default;
    Video& operator=(Video&&) = default;

    /*
     * @brief Gets the height of the video frame
     *
     * @return Value of the height of video frame in pixels
     */
    inline size_t getHeight() const
    {
        return height;
    }
    /*
     * @brief Gets the width of the video frame
     *
     * @return Value of the width of video frame in pixels
     */
    inline size_t getWidth() const
    {
        return width;
    }

  private:
    /* @brief Height in pixels of the video frame */
    size_t height;
    /* @brief Width in pixels of the video frame */
    size_t width;
    /* @brief Reference to the Input object */
    Input& input;
    /* @brief Path to the V4L2 video device */
    const std::string path;
};

} // namespace ikvm
