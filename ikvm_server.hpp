#pragma once

#include "ikvm_args.hpp"
#include "ikvm_input.hpp"
#include "ikvm_video.hpp"

namespace ikvm
{

/*
 * @class Server
 * @brief Manages the RFB server connection and updates
 */
class Server
{
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
        ClientData(int s, Input* i) : skipFrame(s), input(i)
        {
        }
        ~ClientData() = default;
        ClientData(const ClientData&) = default;
        ClientData& operator=(const ClientData&) = default;
        ClientData(ClientData&&) = default;
        ClientData& operator=(ClientData&&) = default;

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
    ~Server();
    Server(const Server&) = default;
    Server& operator=(const Server&) = default;
    Server(Server&&) = default;
    Server& operator=(Server&&) = default;

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
    /* @brief Reference to the Input object */
    Input& input;
    /* @brief Reference to the Video object */
    Video& video;
};

} // namespace ikvm
