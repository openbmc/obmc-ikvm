#pragma once

#include <rfb/rfb.h>
#include <vector>
#include "ikvm_args.hpp"
#include "ikvm_input.hpp"
#include "ikvm_video.hpp"

namespace ikvm
{

/*
 * @class Server
 * @brief Manages the RFB server connection and updates
 */
class Server {
    public:
        /*
         * @struct ClientData
         * @brief Store necessary data for each connected RFB client
         */
        struct ClientData
        {
            /*
             * @brief Constructs ClientData object
             *
             * @param[in] s - Number of frames to skip when client connects
             * @param[in] i - Pointer to Input object
             */
            ClientData(int s, Input* i) :
                skipFrame(s),
                input(i)
            {}

            int skipFrame;
            Input* input;
        };

        /*
         * @brief Constructs Server object
         *
         * @param[in] args - Reference to Args object
         * @param[in] i    - Reference to Input object
         * @param[in] v    - Reference to Video object
         */
        Server(const Args& args, Input& i, Video& v);

        /* @brief Resizes the RFB framebuffer */
        void resize();
        /* @brief Executes any pending RFB updates and client input */
        void run();
        /* @brief Sends pending video frame to clients */
        void sendFrame();

        /*
         * @brief Indicates whether or not video data is desired
         *
         * @return Boolean to indicate whether any clients need a video frame
         */
        inline bool wantsFrame() const
        {
            return server->clientHead;
        }
        /*
         * @brief Get the Video object
         *
         * @return Reference to the Video object
         */
        inline const Video& getVideo() const
        {
            return video;
        }

    private:
        /*
         * @brief Handler for a client disconnecting
         *
         * @param[in] cl - Handle to the client object
         */
        static void clientGone(rfbClientPtr cl);
        /*
         * @brief Handler for client connecting
         *
         * @param[in] cl - Handle to the client object
         */
        static enum rfbNewClientAction newClient(rfbClientPtr cl);

        /* @brief Number of connected clients */
        int numClients;
        /* @brief Microseconds to process RFB events every frame */
        long int processTime;
        /* @brief Handle to the RFB server object */
        rfbScreenInfoPtr server;
        /* @brief Reference to the Input object */
        Input& input;
        /* @brief Reference to the Video object */
        Video& video;
        /* @brief Default framebuffer storage */
        std::vector<char> framebuffer;
};

} // namespace ikvm
