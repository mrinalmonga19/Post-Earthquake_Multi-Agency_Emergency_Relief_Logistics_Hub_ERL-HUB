#ifndef ERL_MANIPULATORS_HPP
#define ERL_MANIPULATORS_HPP

// Unit IV: user defined I/O manipulators used for the tabular manifest printing.

#include <iomanip>
#include <ostream>

namespace ReliefOps {
namespace IO {

// Plain manipulator: a function taking and returning ostream&
inline std::ostream &rule(std::ostream &os) {
    return os << std::setfill('-') << std::setw(58) << "" << std::setfill(' ') << '\n';
}

// Plain manipulator: fixed notation with one decimal
inline std::ostream &oneDecimal(std::ostream &os) {
    return os << std::fixed << std::setprecision(1);
}

// Parameterised manipulator: an object carrying a width, applied by operator<<
struct cell {
    int width;
    explicit cell(int w) : width(w) {}
};

inline std::ostream &operator<<(std::ostream &os, const cell &c) {
    return os << std::left << std::setw(c.width);
}

}  // namespace IO
}  // namespace ReliefOps

#endif
