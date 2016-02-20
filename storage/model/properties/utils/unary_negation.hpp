#pragma once

#include "utils/crtp.hpp"

template <class Derived>
struct UnaryNegation : public Crtp<Derived>
{
    Derived operator-() const
    {
        return Derived(-this->derived().value);
    }
};
