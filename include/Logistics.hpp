#ifndef ERL_LOGISTICS_HPP
#define ERL_LOGISTICS_HPP

#include <cmath>
#include <string>

#include "Exceptions.hpp"
#include "Inventory.hpp"

namespace ReliefOps {
namespace Logistics {

// ----------------------------------------------------------- inline utilities
struct Point {
    double x, y;
    Point(double px = 0.0, double py = 0.0) : x(px), y(py) {}
};

inline double distanceKm(const Point &a, const Point &b) {
    const double dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}
inline double payloadFraction(double loadKg, double capacityKg) {
    return capacityKg > 0.0 ? loadKg / capacityKg : 0.0;
}
inline double etaHours(double km, double kmPerHour) {
    return kmPerHour > 0.0 ? km / kmPerHour : 0.0;
}

// ------------------------------------------------------------------- Corridor
struct Corridor {
    std::string from, to;
    double km;
    bool roadDamaged;
    bool airspaceClear;
    bool waterway;
    Point destination;

    Corridor() : from(), to(), km(0.0), roadDamaged(false), airspaceClear(true), waterway(false), destination() {}

    // return by reference: calls can be chained, c.markRoadDamaged().openWaterway()
    Corridor &markRoadDamaged() { roadDamaged = true; return *this; }
    Corridor &openWaterway() { waterway = true; return *this; }
    Corridor &closeAirspace() { airspaceClear = false; return *this; }
};

// Builds a corridor between two points (road distance = straight line x 1.25 detour factor)
Corridor makeCorridor(const std::string &from, const std::string &to, const Point &a, const Point &b);

// call by reference: modifies the caller's corridor in place
void rerouteAround(Corridor &c, double extraKm);

// ------------------------------------------------------- binary log record (POD)
struct TransitLogRecord {
    int sequence;
    int vehicleId;
    double tonnes;
    double etaHours;
    char kind[20];
    char from[20];
    char to[20];
};

// ---------------------------------------- independent classes used by composition
class GPSBeacon {
private:
    std::string id_;
    Point position_;

public:
    GPSBeacon(const std::string &id = "GPS", const Point &p = Point()) : id_(id), position_(p) {}
    const std::string &id() const { return id_; }
    const Point &position() const { return position_; }
    void moveTo(const Point &p) { position_ = p; }
};

class EmergencyLifeSupportUnit {
private:
    int oxygenMinutes_;
    int beds_;

public:
    EmergencyLifeSupportUnit(int oxygenMinutes = 0, int beds = 0) : oxygenMinutes_(oxygenMinutes), beds_(beds) {}
    int oxygenMinutes() const { return oxygenMinutes_; }
    int beds() const { return beds_; }
    int treat(int patients);   // limited by beds and oxygen, returns patients treated
};

// -------------------------------------------------------------- ReliefVehicle
// Abstract base. Also plays the role of "TransportVehicle" in the Unit II list.
// GPSBeacon and EmergencyLifeSupportUnit are contained objects (composition),
// not base classes: a vehicle HAS a beacon, it is not a beacon.
class ReliefVehicle {
private:
    int id_;
    std::string name_;
    double capacityKg_;
    double fuelL_;
    double burnLPerKm_;
    double loadKg_;
    GPSBeacon beacon_;
    EmergencyLifeSupportUnit lifeSupport_;

    static int operational_;               // Unit II: network wide statics
    static double totalDispatchedTonnes_;
    static bool trace_;

    ReliefVehicle(const ReliefVehicle &);              // vehicles are unique, never copied
    ReliefVehicle &operator=(const ReliefVehicle &);

protected:
    void ensureFuel(double km) const;   // throws FuelDepletionException
    static void log(const std::string &line);

public:
    ReliefVehicle(int id, const std::string &name, double capacityKg, double fuelL, double burnLPerKm,
                  const Point &pos, const EmergencyLifeSupportUnit &ls = EmergencyLifeSupportUnit());
    virtual ~ReliefVehicle();   // virtual: deleting through ReliefVehicle* runs the full chain

    virtual double evaluateRoute(const Corridor &c) const = 0;   // hours, or throws
    virtual const char *kind() const = 0;

    void load(const Inventory::Manifest &m);   // throws CargoOverweightException
    TransitLogRecord dispatch(const Corridor &c, int sequence);

    int id() const { return id_; }
    const std::string &name() const { return name_; }
    double fuel() const { return fuelL_; }
    double loadKg() const { return loadKg_; }
    double capacity() const { return capacityKg_; }
    const GPSBeacon &beacon() const { return beacon_; }
    EmergencyLifeSupportUnit &lifeSupport() { return lifeSupport_; }   // return by reference

    static int operationalVehicles() { return operational_; }
    static double totalDispatchedTonnes() { return totalDispatchedTonnes_; }
    static void setTrace(bool on) { trace_ = on; }
};

typedef ReliefVehicle TransportVehicle;

// Every intermediate class inherits ReliefVehicle VIRTUALLY, so a class that derives from
// two of them (AmphibiousMedicalUnit) still holds only one ReliefVehicle subobject.
class AerialTransport : public virtual ReliefVehicle {
public:
    AerialTransport(int id, const std::string &name, double capacityKg, double fuelL, const Point &pos);
    ~AerialTransport();
    double evaluateRoute(const Corridor &c) const override;
    const char *kind() const override { return "Aerial"; }
};

class GroundConvoy : public virtual ReliefVehicle {
public:
    GroundConvoy(int id, const std::string &name, double capacityKg, double fuelL, const Point &pos);
    ~GroundConvoy();
    double evaluateRoute(const Corridor &c) const override;
    const char *kind() const override { return "Ground"; }
};

class AmphibiousCraft : public virtual ReliefVehicle {
public:
    AmphibiousCraft(int id, const std::string &name, double capacityKg, double fuelL, const Point &pos);
    ~AmphibiousCraft();
    double evaluateRoute(const Corridor &c) const override;
    const char *kind() const override { return "Amphibious"; }
};

// The diamond: ReliefVehicle <- GroundConvoy / AmphibiousCraft <- AmphibiousMedicalUnit
class AmphibiousMedicalUnit : public GroundConvoy, public AmphibiousCraft {
public:
    AmphibiousMedicalUnit(int id, const std::string &name, double capacityKg, double fuelL, const Point &pos);
    ~AmphibiousMedicalUnit();
    // Both parents override these, so the hybrid must supply the final overrider.
    double evaluateRoute(const Corridor &c) const override;
    const char *kind() const override { return "AmphibiousMedical"; }
    int treatPatients(int patients) { return lifeSupport().treat(patients); }
};

}  // namespace Logistics
}  // namespace ReliefOps

#endif
