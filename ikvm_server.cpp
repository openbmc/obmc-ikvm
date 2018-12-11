#include "ikvm_server.hpp"

namespace ikvm
{

Server::Server(const Args& args, Input& i, Video& v) :
    input(i), video(v)
{
}

Server::~Server()
{
}

} // namespace ikvm
