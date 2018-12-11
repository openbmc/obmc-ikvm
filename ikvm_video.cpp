#include "ikvm_video.hpp"

namespace ikvm
{

Video::Video(const std::string& p, Input& input, int fr) :
    height(600), width(800), input(input), path(p)
{
}

Video::~Video()
{
}

} // namespace ikvm
