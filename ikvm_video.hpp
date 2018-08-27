#pragma once

#include <mutex>
#include <string>
#include <vector>
#include "ikvm_input.hpp"

namespace ikvm
{

/*
 * @class Video
 * @brief Sets up the V4L2 video device and performs read operations
 */
class Video {
    public:
        /*
         * @brief Constructs Video object
         *
         * @param[in] p     - Path to the V4L2 video device
         * @param[in] input - Reference to the Input object
         * @param[in] fr    - desired frame rate of the video
         */
        Video(const std::string& p, Input& input, int fr = 30);

        /*
         * @brief Gets the video frame data
         *
         * @return Pointer to the video frame data
         */
        char* getData();
        /* @brief Performs read to grab latest video frame */
        void getFrame();
        /*
         * @brief Gets whether or not the video frame needs to be resized
         *
         * @return Boolean indicating if the frame needs to be resized
         */
        bool needsResize();
        /* @brief Resets the video device */
        void reset();
        /* @brief Performs the resize and re-allocates framebuffer */
        void resize();
        /* @brief Starts streaming from the video device */
        void start();

        /*
         * @brief Gets the desired video frame rate in frames per second
         *
         * @return Value of the desired frame rate
         */
        inline int getFrameRate() const
        {
            return frameRate;
        }
        /*
         * @brief Gets the size of the video frame data
         *
         * @return Value of the size of the video frame data in bytes
         */
        inline size_t getFrameSize() const
        {
            return buffers[lastFrameIndex].size;
        }
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

        /* @brief Number of bits per component of a pixel */
        static const int bitsPerSample;
        /* @brief Number of bytes of storage for a pixel */
        static const int bytesPerPixel;
        /* @brief Number of components in a pixel (i.e. 3 for RGB pixel) */
        static const int samplesPerPixel;

    private:
        /*
         * @struct Buffer
         * @brief Store the address and size of frame data from streaming
         *        operations
         */
        struct Buffer
        {
            Buffer() :
                data(nullptr),
                queued(false),
                size(0)
            {}

            void* data;
            bool queued;
            size_t size;
        };

        /* @brief Boolean value to indicate if the video device is streaming */
        bool isStreaming;
        /* @brief File descriptor for the V4L2 video device */
        int fd;
        /* @brief Desired frame rate of video stream in frames per second */
        int frameRate;
        /* @brief Buffer index for the last video frame */
        int lastFrameIndex;
        /* @brief Height in pixels of the video frame */
        size_t height;
        /* @brief Width in pixels of the video frame */
        size_t width;
        /* @brief Mutex lock to prevent simultaneous reset and read frame */
        std::mutex lock;
        /* @brief Path to the V4L2 video device */
        std::string path;
        /* @brief Streaming buffer storage */
        std::vector<Buffer> buffers;
};

} // namespace ikvm
