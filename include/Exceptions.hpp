#ifndef ERL_EXCEPTIONS_HPP
#define ERL_EXCEPTIONS_HPP

// Unit IV: custom exception framework. Every class derives from std::exception
// (through ReliefException) and overrides what() with a diagnostic message.

#include <exception>
#include <iomanip>
#include <sstream>
#include <string>

namespace ReliefOps {

namespace detail {
inline std::string num(double v) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(1) << v;
    return os.str();
}
}  // namespace detail

class ReliefException : public std::exception {
protected:
    std::string message;

public:
    explicit ReliefException(const std::string &m) : message(m) {}
    const char *what() const noexcept override { return message.c_str(); }
};

class CargoOverweightException : public ReliefException {
public:
    CargoOverweightException(const std::string &vehicle, double loadKg, double capKg)
        : ReliefException("CargoOverweight: " + vehicle + " would carry " + detail::num(loadKg) +
                          " kg, capacity is " + detail::num(capKg) + " kg") {}
};

class FuelDepletionException : public ReliefException {
public:
    FuelDepletionException(const std::string &vehicle, double neededL, double haveL)
        : ReliefException("FuelDepletion: " + vehicle + " needs " + detail::num(neededL) +
                          " L for this leg but has " + detail::num(haveL) + " L") {}
};

class DamagedCorridorException : public ReliefException {
public:
    DamagedCorridorException(const std::string &from, const std::string &to, const std::string &why)
        : ReliefException("DamagedCorridor: " + from + " -> " + to + " is not passable (" + why + ")") {}
};

class InsufficientStockException : public ReliefException {
public:
    InsufficientStockException(const std::string &item, int wanted, int available)
        : ReliefException("InsufficientStock: " + item + " requested " + std::to_string(wanted) +
                          ", available " + std::to_string(available)) {}
};

class PersistenceException : public ReliefException {
public:
    explicit PersistenceException(const std::string &why) : ReliefException("Persistence: " + why) {}
};

}  // namespace ReliefOps

#endif
