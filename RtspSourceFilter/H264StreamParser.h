#pragma once

#include <vector>
#include <cstdint>

/**
 * Basic SPS parser - extracts video width, height and framerate if applicable
 */
class H264StreamParser
{
public:
    H264StreamParser(uint8_t* sps, unsigned spsSize);

    unsigned GetWidth() const { return _width; }
    unsigned GetHeight() const { return _height; }
    double GetFramerate() const { return _framerate; }

private:
    std::vector<uint8_t> _sps;

    // Parsed value
    unsigned _width;
    unsigned _height;
    double _framerate;
};