// Copyright (C) 1997, 1999-2001, 2008 Nathan Lamont
// Copyright (C) 2008-2018 The Antares Authors
//
// This file is part of Antares, a tactical space combat game.
//
// Antares is free software: you can redistribute it and/or modify it
// under the terms of the Lesser GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Antares is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with Antares.  If not, see http://www.gnu.org/licenses/

#ifndef ANTARES_SPRITE_DATA_HPP_
#define ANTARES_SPRITE_DATA_HPP_

#include <pn/string>
#include <pn/value>
#include <sfz/optional.hpp>
#include <vector>

#include "math/geometry.hpp"

namespace antares {

struct SpriteData {
    struct Frame {
        int32_t left, top, right, bottom;
        int32_t cx, cy;
    };
    std::vector<Frame> frames;
};

SpriteData sprite_data(pn::value_cref x);

}  // namespace antares

#endif  // ANTARES_SPRITE_DATA_HPP_
