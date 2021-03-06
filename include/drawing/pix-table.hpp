// Copyright (C) 1997, 1999-2001, 2008 Nathan Lamont
// Copyright (C) 2008-2017 The Antares Authors
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

#ifndef ANTARES_DRAWING_PIX_TABLE_HPP_
#define ANTARES_DRAWING_PIX_TABLE_HPP_

#include <vector>

#include "drawing/pix-map.hpp"
#include "video/driver.hpp"

namespace antares {

class NatePixTable {
  public:
    class Frame;

    NatePixTable(pn::string_view name, Hue hue);
    NatePixTable(const NatePixTable&) = delete;
    NatePixTable(NatePixTable&&)      = default;
    NatePixTable& operator=(const NatePixTable&) = delete;
    NatePixTable& operator=(NatePixTable&&) = default;
    ~NatePixTable();

    const Frame& at(size_t index) const;
    size_t       size() const;

  private:
    size_t             _size;
    std::vector<Frame> _frames;
};

class NatePixTable::Frame {
  public:
    Frame(Rect bounds, const PixMap& image, pn::string_view name, int frame);
    Frame(Rect bounds, const PixMap& image, pn::string_view name, int frame, const PixMap& overlay,
          Hue hue);
    Frame(Frame&&) = default;
    ~Frame();

    uint16_t       width() const;
    uint16_t       height() const;
    Point          center() const;
    const PixMap&  pix_map() const;
    const Texture& texture() const;

  private:
    void load_image(const PixMap& pix);
    void load_overlay(const PixMap& pix, Hue hue);
    void build(pn::string_view name, int frame);

    Rect        _bounds;
    ArrayPixMap _pix_map;
    Texture     _texture;
};

}  // namespace antares

#endif  // ANTARES_DRAWING_PIX_TABLE_HPP_
