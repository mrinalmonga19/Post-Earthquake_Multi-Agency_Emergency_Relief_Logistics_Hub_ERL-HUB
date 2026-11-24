#include "Logistics.hpp"

#include <cstring>
#include <iostream>

namespace ReliefOps {
namespace Logistics {

// ------------------------------------------------------------------ helpers
Corridor makeCorridor(const std::string &from, const std::string &to, const Point &a, const Point &b) {
    Corridor c;
    c.from = from;
    c.to = to;
    c.km = distanceKm(a, b) * 1.25;
    c.destination = b;
    return c;
}

void rerouteAround(Corridor &c, double extraKm) {
    c.km += extraKm;
}

int EmergencyLifeSupportUnit::treat(int patients) {
    int treated = patients < beds_ ? patients : beds_;
    const int oxygenPerPatient = 30;
    while (treated > 0 && treated * oxygenPerPatient > oxygenMinutes_)
        --treated;
    oxygenMinutes_ -= treated * oxygenPerPatient;
    return treated;
}

static void copyText(char *dest, std::size_t size, const std::string &src) {
    std::memset(dest, 0, size);
    std::strncpy(dest, src.c_str(), size - 1);
}

// ------------------------------------------------------------ ReliefVehicle
int ReliefVehicle::operational_ = 0;
double ReliefVehicle::totalDispatchedTonnes_ = 0.0;
bool ReliefVehicle::trace_ = false;

void ReliefVehicle::log(const std::string &line) {
    if (trace_)
        std::cout << "  " << line << '\n';
}

ReliefVehicle::ReliefVehicle(int id, const std::string &name, double capacityKg, double fuelL,
                             double burnLPerKm, const Point &pos, const EmergencyLifeSupportUnit &ls)
    : id_(id), name_(name), capacityKg_(capacityKg), fuelL_(fuelL), burnLPerKm_(burnLPerKm), loadKg_(0.0),
      beacon_("GPS-" + std::to_string(id), pos), lifeSupport_(ls) {
    ++operational_;
    log("[ctor] ReliefVehicle #" + std::to_string(id_) + " (" + name_ + ")");
}

ReliefVehicle::~ReliefVehicle() {
    --operational_;
    log("[dtor] ReliefVehicle #" + std::to_string(id_));
}

void ReliefVehicle::ensureFuel(double km) const {
    const double needed = km * burnLPerKm_;
    if (needed > fuelL_)
        throw FuelDepletionException(name_, needed, fuelL_);
}

void ReliefVehicle::load(const Inventory::Manifest &m) {
    const double total = loadKg_ + m.totalKg();
    if (total > capacityKg_)
        throw CargoOverweightException(name_, total, capacityKg_);
    loadKg_ = total;
}

TransitLogRecord ReliefVehicle::dispatch(const Corridor &c, int sequence) {
    const double eta = evaluateRoute(c);   // dynamic dispatch: may throw before anything changes
    fuelL_ -= c.km * burnLPerKm_;
    const double tonnes = loadKg_ / 1000.0;
    totalDispatchedTonnes_ += tonnes;
    beacon_.moveTo(c.destination);
    loadKg_ = 0.0;

    TransitLogRecord r;
    std::memset(&r, 0, sizeof(r));
    r.sequence = sequence;
    r.vehicleId = id_;
    r.tonnes = tonnes;
    r.etaHours = eta;
    copyText(r.kind, sizeof(r.kind), kind());
    copyText(r.from, sizeof(r.from), c.from);
    copyText(r.to, sizeof(r.to), c.to);
    return r;
}

// -------------------------------------------------------------- AerialTransport
AerialTransport::AerialTransport(int id, const std::string &name, double capacityKg, double fuelL, const Point &pos)
    : ReliefVehicle(id, name, capacityKg, fuelL, 6.0, pos) {
    log("[ctor]   AerialTransport");
}
AerialTransport::~AerialTransport() { log("[dtor]   AerialTransport"); }

double AerialTransport::evaluateRoute(const Corridor &c) const {
    if (!c.airspaceClear)
        throw DamagedCorridorException(c.from, c.to, "airspace closed");
    ensureFuel(c.km);
    return etaHours(c.km, 200.0);
}

// ---------------------------------------------------------------- GroundConvoy
GroundConvoy::GroundConvoy(int id, const std::string &name, double capacityKg, double fuelL, const Point &pos)
    : ReliefVehicle(id, name, capacityKg, fuelL, 0.6, pos) {
    log("[ctor]   GroundConvoy");
}
GroundConvoy::~GroundConvoy() { log("[dtor]   GroundConvoy"); }

double GroundConvoy::evaluateRoute(const Corridor &c) const {
    if (c.roadDamaged)
        throw DamagedCorridorException(c.from, c.to, "road damaged");
    ensureFuel(c.km);
    return etaHours(c.km, 40.0);
}

// ------------------------------------------------------------- AmphibiousCraft
AmphibiousCraft::AmphibiousCraft(int id, const std::string &name, double capacityKg, double fuelL, const Point &pos)
    : ReliefVehicle(id, name, capacityKg, fuelL, 0.9, pos) {
    log("[ctor]   AmphibiousCraft");
}
AmphibiousCraft::~AmphibiousCraft() { log("[dtor]   AmphibiousCraft"); }

double AmphibiousCraft::evaluateRoute(const Corridor &c) const {
    if (c.roadDamaged && !c.waterway)
        throw DamagedCorridorException(c.from, c.to, "road damaged and no waterway");
    ensureFuel(c.km);
    return etaHours(c.km, c.waterway ? 25.0 : 35.0);
}

// ------------------------------------------------------- AmphibiousMedicalUnit
// The most derived class names the virtual base itself; the ReliefVehicle(...) calls in
// GroundConvoy and AmphibiousCraft are skipped when building this class.
AmphibiousMedicalUnit::AmphibiousMedicalUnit(int id, const std::string &name, double capacityKg, double fuelL,
                                             const Point &pos)
    : ReliefVehicle(id, name, capacityKg, fuelL, 0.8, pos, EmergencyLifeSupportUnit(480, 6)),
      GroundConvoy(id, name, capacityKg, fuelL, pos),
      AmphibiousCraft(id, name, capacityKg, fuelL, pos) {
    log("[ctor] AmphibiousMedicalUnit");
}
AmphibiousMedicalUnit::~AmphibiousMedicalUnit() { log("[dtor] AmphibiousMedicalUnit"); }

double AmphibiousMedicalUnit::evaluateRoute(const Corridor &c) const {
    try {
        return GroundConvoy::evaluateRoute(c);        // prefer the road
    } catch (const DamagedCorridorException &) {
        return AmphibiousCraft::evaluateRoute(c);     // fall back to water
    }
}

}  // namespace Logistics
}  // namespace ReliefOps
