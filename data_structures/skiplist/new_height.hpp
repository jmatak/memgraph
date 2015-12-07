#pragma once

#include "utils/random/xorshift.hpp"

template <class randomizer_t>
size_t new_height(int max_height)
{
    // get 64 random bits (coin tosses)
    uint64_t rand = xorshift::next();
    size_t height = 1;

    // for every head (1) increase the tower height by one until the tail (0)
    // comes. this gives the following probabilities for tower heights:
    //
    // 1/2 1/4 1/8 1/16 1/32 1/64 ...
    //  1   2   3   4    5    6   ...
    //
    while(max_height-- && ((rand >>= 1) & 1))
        height++;

    return height;
}
