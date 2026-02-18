#pragma once

#include <cstdint>
#include <cstddef>

#include <functional>
#include <array>
#include <string>
#include <string_view>

namespace Hive {
    
    enum class Color : std::uint8_t {White = 0, Black = 1};
    constexpr Color rival(Color col) {
        return (col == Color::White) ? Color::Black : Color::White;
    }
    
    enum class Bug : std::uint8_t {
        Queen,
        Beetle,
        Spider,
        Grasshopper,
        Ant,
        Ladybug,
        Mosquito,
        Pillbug
    };

    struct Piece {
        Color color;
        Bug bug;
        uint8_t id = 0;

        friend bool operator == (const Piece& a, const Piece& b) {
            return (a.color == b.color) && (a.bug == b.bug) && (a.id == b.id);
        }
        friend bool operator != (const Piece& a, const Piece& b) {
            return !(a == b);
        }
    };


    // utilities for bug name and color name
    constexpr std::string_view colorName (Color col) {
        return (col == Color::White) ? "White" : "Black";
    }
    constexpr std::string_view bugName (Bug bug) {
        switch (bug) {
            case Bug::Queen:        return "Queen";
            case Bug::Beetle:       return "Beetle";
            case Bug::Spider:      return "Spider";
            case Bug::Grasshopper: return "Grasshopper";
            case Bug::Ant:         return "Ant";
            case Bug::Ladybug:     return "Ladybug";
            case Bug::Mosquito:    return "Mosquito";
            case Bug::Pillbug:     return "Pillbug";
        }
        return "Unknown";
    }

}