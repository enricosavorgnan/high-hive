#pragma once

#include <cstdint>
#include <cstddef>

#include <functional>
#include <array>
#include <string>
#include <string_view>

// PIECES STRUCTURE IMPLEMENTATION
// Defines Pieces as a tuple (color, bug, id)
// where:
// color:   is the color of the bug (white or black)
// bug:     is the type of bug
// id:  is the id of the bug (piece=wA2 -> id=2; piece=bQ -> id=0)

namespace Hive {

    // Color class
    // Accepts two possible colors, White and Black
    enum class Color : std::uint8_t {White = 0, Black = 1};

    // Bug class
    // Accepts all the possible bug types
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

    // Piece structure
    // Tuple (Color, Bug, id)
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


    // ---- Utilities -----

    // Returns a string view of the name of the color
    constexpr std::string_view colorName (Color col) {
        return (col == Color::White) ? "White" : "Black";
    }

    // Returns a string view of the name of the bug
    constexpr std::string_view bugName (const Bug bug) {
        switch (bug) {
            case Bug::Queen:        return "Queen";
            case Bug::Beetle:       return "Beetle";
            case Bug::Spider:       return "Spider";
            case Bug::Grasshopper:  return "Grasshopper";
            case Bug::Ant:          return "Ant";
            case Bug::Ladybug:      return "Ladybug";
            case Bug::Mosquito:     return "Mosquito";
            case Bug::Pillbug:      return "Pillbug";
        }
        return "Unknown";
    }

    // Defines the "rival" color, i.e. the color of the opposite player
    constexpr Color rival(Color col) {
        return (col == Color::White) ? Color::Black : Color::White;
    }

}