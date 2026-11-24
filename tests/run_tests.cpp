// Self-contained test suite (no external framework). Exit code 0 means every check passed.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Exceptions.hpp"
#include "Inventory.hpp"
#include "Logistics.hpp"
#include "MedicalNeed.hpp"
#include "Persistence.hpp"
#include "PriorityDispatchQueue.hpp"

using namespace ReliefOps;
using namespace ReliefOps::Inventory;
using namespace ReliefOps::Logistics;

static int total = 0, failed = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++total;                                                                 \
        if (!(cond)) {                                                           \
            ++failed;                                                            \
            std::cout << "  FAIL line " << __LINE__ << ": " #cond << std::endl;  \
        }                                                                        \
    } while (0)

#define CHECK_THROWS(expr, ExType)                          \
    do {                                                    \
        ++total;                                            \
        bool caught = false;                                \
        try { expr; } catch (const ExType &) { caught = true; } \
        if (!caught) {                                      \
            ++failed;                                       \
            std::cout << "  FAIL line " << __LINE__ << ": expected " #ExType << std::endl; \
        }                                                   \
    } while (0)

static void testCargoAndDepot() {
    std::cout << "[cargo and depot]\n";
    DistributionDepot d("T");
    d += ReliefCargo("Kit", Category::Medicine, 10, 2.0);
    d += ReliefCargo("Kit", Category::Medicine, 5, 2.0);   // restock merges
    CHECK(d.item("Kit").quantity() == 15);
    d -= ReliefCargo("Kit", Category::Medicine, 6, 2.0);
    CHECK(d.item("Kit").quantity() == 9);
    CHECK_THROWS(d -= ReliefCargo("Kit", Category::Medicine, 100, 2.0), InsufficientStockException);
    CHECK_THROWS(d -= ReliefCargo("Missing", Category::Water, 1, 1.0), InsufficientStockException);

    d.item("Kit").reserve(4);
    CHECK(d.item("Kit").available() == 5);
    CHECK_THROWS(d -= ReliefCargo("Kit", Category::Medicine, 6, 2.0), InsufficientStockException);
    IncidentCommander boss;
    CHECK(boss.reservedOf(d.item("Kit")) == 4);
    CHECK(boss.overrideReserve(d, "Kit") == 4);
    CHECK(d.item("Kit").available() == 9);
}

static void testAllocation() {
    std::cout << "[overloaded allocation]\n";
    DistributionDepot d("T");
    d += ReliefCargo("Ration", Category::Ration, 100, 10.0);
    d += ReliefCargo("Trauma", Category::Medicine, 50, 1.0);
    Manifest m(1);
    CHECK(allocate(d, "Ration", 255.0, m) == 250.0);     // volume: floor(255/10) = 25 units
    CHECK(d.item("Ration").quantity() == 75);
    CHECK(allocate(d, "Trauma", 10, 5, m) == 20);        // priority 5 doubles
    CHECK(allocate(d, "Trauma", 10, 2, m) == 10);        // priority 2 single
    CHECK(allocate(d, "Trauma", 100, 1, m) == 20);       // clamped to what is left
    CHECK(allocate(d, "Trauma", 5, 1, m) == 0);          // nothing left
}

static void testManifest() {
    std::cout << "[manifest]\n";
    Manifest a(7, 1);
    a.add(ReliefCargo("A", Category::Ration, 2, 1.0));
    a.add(ReliefCargo("B", Category::Water, 3, 5.0));      // forces grow()
    CHECK(a.size() == 2);
    CHECK(a.add(ReliefCargo("A", Category::Ration, 4, 1.0)) == false);   // duplicate merges
    CHECK(a.line(0).quantity() == 6);
    CHECK(a.totalKg() == 6 * 1.0 + 3 * 5.0);

    Manifest b(a);
    CHECK(a == b);
    b.add(ReliefCargo("C", Category::Blanket, 1, 1.0));
    CHECK(!(a == b));
    CHECK(a.size() == 2);                                  // original untouched: deep copy
    Manifest c;
    c = b;
    CHECK(c == b);
    c = c;                                                 // self assignment is safe
    CHECK(c == b);
    CHECK_THROWS(a.line(5), std::out_of_range);

    std::stringstream ss;
    ss << a;
    Manifest r;
    ss >> r;
    CHECK(r == a);

    std::stringstream bad("MANIFEST 1 1\nWidget Unknown 1 1.0\n");
    Manifest dummy;
    bad >> dummy;
    CHECK(bad.fail());

    CHECK(Manifest::fromAudit(7, a.toAudit()) == a);
}

static void testVehicles() {
    std::cout << "[vehicles]\n";
    CHECK(ReliefVehicle::operationalVehicles() == 0);
    {
        AmphibiousMedicalUnit hybrid(1, "H", 1000, 500, Point());
        GroundConvoy truck(2, "T", 1000, 500, Point());
        CHECK(ReliefVehicle::operationalVehicles() == 2);   // hybrid counted once: single base subobject

        ReliefVehicle *p = &hybrid;                          // implicit upcast is unambiguous (virtual base)
        CHECK(std::string(p->kind()) == "AmphibiousMedical");

        Corridor road = makeCorridor("a", "b", Point(0, 0), Point(30, 40));   // 62.5 km
        CHECK(std::fabs(road.km - 62.5) < 1e-9);
        CHECK(std::fabs(truck.evaluateRoute(road) - 62.5 / 40.0) < 1e-9);

        road.markRoadDamaged();
        CHECK_THROWS(truck.evaluateRoute(road), DamagedCorridorException);
        CHECK_THROWS(hybrid.evaluateRoute(road), DamagedCorridorException);   // no waterway yet
        road.openWaterway();
        CHECK(std::fabs(hybrid.evaluateRoute(road) - 62.5 / 25.0) < 1e-9);    // falls back to water

        Manifest m(1);
        m.add(ReliefCargo("X", Category::Water, 20, 100.0));   // 2000 kg
        CHECK_THROWS(truck.load(m), CargoOverweightException);
        CHECK(truck.loadKg() == 0.0);                           // failed load changes nothing

        AerialTransport heli(3, "A", 1000, 10, Point());
        Corridor far = makeCorridor("a", "b", Point(0, 0), Point(100, 0));
        CHECK_THROWS(heli.evaluateRoute(far), FuelDepletionException);
        CHECK(heli.fuel() == 10.0);

        CHECK(hybrid.treatPatients(10) == 6);                   // limited by beds
        CHECK(hybrid.treatPatients(10) == 6);                   // 480 min: 6 x 30 = 180, still enough
        CHECK(hybrid.treatPatients(10) == 4);                   // only 120 min of oxygen left
    }
    CHECK(ReliefVehicle::operationalVehicles() == 0);
}

static void testDispatchTotals() {
    std::cout << "[dispatch statics]\n";
    const double before = ReliefVehicle::totalDispatchedTonnes();
    GroundConvoy truck(1, "T", 5000, 500, Point());
    Manifest m(1);
    m.add(ReliefCargo("X", Category::Ration, 100, 20.0));   // 2000 kg
    truck.load(m);
    Corridor c = makeCorridor("a", "b", Point(0, 0), Point(8, 6));
    TransitLogRecord r = truck.dispatch(c, 5);
    CHECK(r.sequence == 5 && r.vehicleId == 1);
    CHECK(std::fabs(ReliefVehicle::totalDispatchedTonnes() - before - 2.0) < 1e-9);
    CHECK(truck.loadKg() == 0.0);
    CHECK(truck.beacon().position().x == 8.0);
}

static void testQueue() {
    std::cout << "[priority queue]\n";
    PriorityDispatchQueue<MedicalNeed, LessUrgent> q;
    CHECK_THROWS(q.pop(), std::out_of_range);
    MedicalNeed a = {"A", "k", 5, 2}, b = {"B", "k", 9, 5}, c = {"C", "k", 20, 5};
    q.push(a); q.push(b); q.push(c);
    CHECK(q.size() == 3);
    CHECK(q.pop().site == "C");   // severity 5, more victims
    CHECK(q.pop().site == "B");
    CHECK(q.pop().site == "A");
    CHECK(q.empty());
}

static void testPersistence() {
    std::cout << "[persistence]\n";
    const std::string path = "test_records.bin";
    std::remove(path.c_str());
    CHECK_THROWS(Persistence::loadRecords<TransitLogRecord>(path), PersistenceException);   // missing file

    std::vector<TransitLogRecord> recs;
    for (int i = 0; i < 3; ++i) {
        TransitLogRecord r;
        std::memset(&r, 0, sizeof(r));
        r.sequence = i + 1;
        r.vehicleId = 10 + i;
        r.tonnes = 1.5 * i;
        std::strncpy(r.from, "Hub", sizeof(r.from) - 1);
        recs.push_back(r);
    }
    Persistence::saveAtomic(path, recs);
    CHECK(!std::filesystem::exists(path + ".tmp"));   // temp file is gone after the rename
    std::vector<TransitLogRecord> back = Persistence::loadRecords<TransitLogRecord>(path);
    CHECK(back.size() == 3 && back[2].vehicleId == 12 && back[1].tonnes == 1.5);

    Persistence::appendAtomic(path, recs[0]);
    CHECK(Persistence::loadRecords<TransitLogRecord>(path).size() == 4);

    CHECK_THROWS(Persistence::loadRecords<ManifestAuditRecord>(path), PersistenceException);   // wrong record type

    {   // flip a byte: checksum must catch it
        std::fstream f(path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(sizeof(Persistence::FileHeader) + 3);
        f.put('\x55');
    }
    CHECK_THROWS(Persistence::loadRecords<TransitLogRecord>(path), PersistenceException);

    Persistence::saveAtomic(path, recs);   // overwrite with a good file again
    CHECK(Persistence::loadRecords<TransitLogRecord>(path).size() == 3);

    std::vector<TransitLogRecord> empty;   // zero records is a valid file
    Persistence::saveAtomic(path, empty);
    CHECK(Persistence::loadRecords<TransitLogRecord>(path).empty());

    std::remove(path.c_str());
}

int main() {
    testCargoAndDepot();
    testAllocation();
    testManifest();
    testVehicles();
    testDispatchTotals();
    testQueue();
    testPersistence();
    std::cout << "\n" << (total - failed) << " of " << total << " checks passed\n";
    return failed == 0 ? 0 : 1;
}
