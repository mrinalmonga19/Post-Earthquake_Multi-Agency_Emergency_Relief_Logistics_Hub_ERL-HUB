// ERL-HUB: Post-Earthquake Multi-Agency Emergency Relief Logistics Hub
// Demo driver. Every section is labelled with the syllabus unit it demonstrates.

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "Exceptions.hpp"
#include "Inventory.hpp"
#include "Logistics.hpp"
#include "Manipulators.hpp"
#include "MedicalNeed.hpp"
#include "Persistence.hpp"
#include "PriorityDispatchQueue.hpp"

using namespace ReliefOps;
using namespace ReliefOps::Inventory;
using namespace ReliefOps::Logistics;

static void banner(const char *title) {
    std::cout << "\n======== " << title << " ========\n";
}

// Functor handed to std::for_each
struct PrintNeed {
    void operator()(const MedicalNeed &n) const {
        std::cout << "  severity " << n.severity << "  victims " << n.victims << "  " << n.site << "  (" << n.item
                  << ")\n";
    }
};

static void printLog(const std::vector<TransitLogRecord> &records) {
    using namespace ReliefOps::IO;
    std::cout << rule << cell(5) << "SEQ" << cell(5) << "VEH" << cell(20) << "KIND" << cell(14) << "FROM"
              << cell(14) << "TO" << cell(8) << "TONNES" << '\n' << rule;
    for (std::size_t i = 0; i < records.size(); ++i) {
        const TransitLogRecord &r = records[i];
        std::cout << cell(5) << r.sequence << cell(5) << r.vehicleId << cell(20) << r.kind << cell(14) << r.from
                  << cell(14) << r.to << oneDecimal << r.tonnes << '\n';
    }
    std::cout << rule;
}

static void unitOne() {
    banner("UNIT I: namespaces, overloading, references, inline, friend class");

    DistributionDepot depot("Srinagar Relief Hub");
    depot += ReliefCargo("Ration_Kit", Category::Ration, 500, 12.0);
    depot += ReliefCargo("Trauma_Kit", Category::Medicine, 80, 2.5);
    depot += ReliefCargo("Water_Tank", Category::Water, 40, 100.0);
    depot += ReliefCargo("Thermal_Blanket", Category::Blanket, 300, 0.8);
    depot.item("Trauma_Kit").reserve(30);   // return by reference: lock 30 kits for triage
    depot.print(std::cout);

    Manifest m(101, 2);
    double kg = allocate(depot, "Ration_Kit", 600.0, m);      // volume based overload
    int units = allocate(depot, "Trauma_Kit", 20, 5, m);      // priority triage overload
    std::cout << "allocate(volume 600 kg)   -> " << kg << " kg of Ration_Kit\n";
    std::cout << "allocate(20 victims, P5)  -> " << units << " Trauma_Kit\n";

    try {
        depot -= ReliefCargo("Trauma_Kit", Category::Medicine, 20, 2.5);   // only 10 free
    } catch (const InsufficientStockException &e) {
        std::cout << "Blocked by reserve: " << e.what() << '\n';
    }

    IncidentCommander commander;   // friend class
    int released = commander.overrideReserve(depot, "Trauma_Kit");
    std::cout << "IncidentCommander released " << released << " reserved kits; free now "
              << depot.item("Trauma_Kit").available() << '\n';

    Point hub(0, 0), village(30, 40);
    Corridor c = makeCorridor("Srinagar", "Kupwara", hub, village);
    std::cout << "Corridor " << c.from << " -> " << c.to << ": " << c.km << " km (inline distanceKm x 1.25)\n";
    rerouteAround(c, 17.5);                      // call by reference
    c.markRoadDamaged().openWaterway();          // chained through returned references
    std::cout << "After rerouteAround(+17.5) and chained flags: " << c.km << " km, roadDamaged=" << c.roadDamaged
              << ", waterway=" << c.waterway << '\n';
}

static void unitTwo() {
    banner("UNIT II: dynamic memory, copy semantics, statics, operators");

    Manifest original(201, 2);   // dynamic constructor
    original.add(ReliefCargo("Ration_Kit", Category::Ration, 40, 12.0));
    original.add(ReliefCargo("Water_Tank", Category::Water, 2, 100.0));
    original.add(ReliefCargo("Thermal_Blanket", Category::Blanket, 100, 0.8));   // forces grow()

    Manifest copy(original);     // deep copy constructor
    std::cout << "copy == original            : " << (copy == original ? "true" : "false") << '\n';
    copy.add(ReliefCargo("Ration_Kit", Category::Ration, 10, 12.0));   // duplicate item merges
    std::cout << "after adding to the copy    : " << (copy == original ? "still equal" : "differ (deep copy works)")
              << '\n';

    Manifest assigned;
    assigned = original;         // copy assignment
    std::cout << "assigned == original        : " << (assigned == original ? "true" : "false") << '\n';

    std::stringstream stream;    // << and >> as text serialisation
    stream << original;
    Manifest restored;
    stream >> restored;
    std::cout << "stream round trip equal    : " << (restored == original ? "true" : "false") << '\n';

    original.printTable(std::cout);

    std::cout << "Operational vehicles before : " << ReliefVehicle::operationalVehicles() << '\n';
    {
        GroundConvoy truck(900, "Temp Truck", 5000, 400, Point(0, 0));
        std::cout << "Operational vehicles inside : " << ReliefVehicle::operationalVehicles() << '\n';
    }
    std::cout << "Operational vehicles after  : " << ReliefVehicle::operationalVehicles() << '\n';
}

static std::vector<TransitLogRecord> unitThree() {
    banner("UNIT III: inheritance, diamond, composition, polymorphism");

    std::cout << "Building the amphibious medical unit (watch the base constructed once):\n";
    ReliefVehicle::setTrace(true);
    ReliefVehicle *hybrid = new AmphibiousMedicalUnit(4, "Medical Amphibian", 3000, 500, Point(0, 0));
    ReliefVehicle::setTrace(false);

    std::vector<ReliefVehicle *> fleet;
    fleet.push_back(new AerialTransport(1, "Heli Alpha", 1200, 800, Point(0, 0)));
    fleet.push_back(new GroundConvoy(2, "Truck Bravo", 9000, 600, Point(0, 0)));
    fleet.push_back(new AmphibiousCraft(3, "Boat Charlie", 2500, 400, Point(0, 0)));
    fleet.push_back(hybrid);
    std::cout << "Fleet size " << fleet.size() << ", operational vehicles " << ReliefVehicle::operationalVehicles()
              << '\n';

    Manifest cargo(301, 2);
    cargo.add(ReliefCargo("Ration_Kit", Category::Ration, 50, 12.0));   // 600 kg
    cargo.add(ReliefCargo("Trauma_Kit", Category::Medicine, 40, 2.5));  // 100 kg

    Corridor flooded = makeCorridor("Srinagar", "Baramulla", Point(0, 0), Point(24, 32));
    flooded.markRoadDamaged().openWaterway();
    Corridor blocked = makeCorridor("Srinagar", "Uri", Point(0, 0), Point(48, 64));
    blocked.markRoadDamaged().closeAirspace();

    std::vector<TransitLogRecord> log;
    int sequence = 1;
    std::cout << "\nDispatch on the flooded corridor (" << flooded.km << " km), through base pointers:\n";
    for (std::size_t i = 0; i < fleet.size(); ++i) {
        ReliefVehicle *v = fleet[i];
        try {
            v->load(cargo);
            TransitLogRecord r = v->dispatch(flooded, sequence);   // virtual evaluateRoute
            log.push_back(r);
            ++sequence;
            std::cout << "  " << v->kind() << " #" << v->id() << ": ETA " << r.etaHours << " h, "
                      << "GPS now (" << v->beacon().position().x << ", " << v->beacon().position().y << ")\n";
        } catch (const ReliefException &e) {
            std::cout << "  " << v->kind() << " #" << v->id() << ": " << e.what() << '\n';
        }
    }

    std::cout << "\nDispatch on the fully blocked corridor:\n";
    for (std::size_t i = 0; i < fleet.size(); ++i) {
        try {
            fleet[i]->dispatch(blocked, sequence);   // cargo from the earlier attempt is still on board
        } catch (const ReliefException &e) {
            std::cout << "  " << fleet[i]->kind() << " #" << fleet[i]->id() << ": " << e.what() << '\n';
        }
    }

    AmphibiousMedicalUnit *medical = dynamic_cast<AmphibiousMedicalUnit *>(fleet[3]);
    if (medical)
        std::cout << "\nMedical unit treated " << medical->treatPatients(10) << " of 10 patients (6 beds)\n";
    std::cout << "Total dispatched tonnage: " << ReliefVehicle::totalDispatchedTonnes() << " t\n";

    std::cout << "\nDeleting the fleet through ReliefVehicle* (virtual destructor chain, hybrid last):\n";
    ReliefVehicle::setTrace(true);
    for (std::size_t i = 0; i < fleet.size(); ++i)
        delete fleet[i];
    ReliefVehicle::setTrace(false);
    std::cout << "Operational vehicles after cleanup: " << ReliefVehicle::operationalVehicles() << '\n';
    return log;
}

static void unitFour(const std::vector<TransitLogRecord> &log) {
    banner("UNIT IV: exceptions, STL, templates, binary persistence");

    // 1. Exception framework
    std::cout << "-- Custom exceptions --\n";
    AerialTransport heli(11, "Heli Delta", 1000, 30, Point(0, 0));
    Corridor longLeg = makeCorridor("Hub", "Far Camp", Point(0, 0), Point(80, 0));
    Manifest big(401, 1);
    big.add(ReliefCargo("Water_Tank", Category::Water, 15, 100.0));   // 1500 kg
    try { heli.load(big); } catch (const std::exception &e) { std::cout << "  " << e.what() << '\n'; }
    try { heli.evaluateRoute(longLeg); } catch (const std::exception &e) { std::cout << "  " << e.what() << '\n'; }
    GroundConvoy truck(12, "Truck Echo", 5000, 500, Point(0, 0));
    Corridor rubble = makeCorridor("Hub", "Old Town", Point(0, 0), Point(5, 5));
    rubble.markRoadDamaged();
    try { truck.evaluateRoute(rubble); } catch (const std::exception &e) { std::cout << "  " << e.what() << '\n'; }

    // 2. Priority queue template, functors and STL algorithms
    std::cout << "\n-- PriorityDispatchQueue<MedicalNeed, LessUrgent> --\n";
    std::vector<MedicalNeed> needs;
    MedicalNeed n1 = {"Kupwara Clinic", "Trauma_Kit", 12, 3};
    MedicalNeed n2 = {"Uri Camp", "Trauma_Kit", 30, 5};
    MedicalNeed n3 = {"Baramulla School", "Trauma_Kit", 45, 5};
    MedicalNeed n4 = {"Gulmarg Post", "Trauma_Kit", 4, 2};
    needs.push_back(n1); needs.push_back(n2); needs.push_back(n3); needs.push_back(n4);

    PriorityDispatchQueue<MedicalNeed, LessUrgent> queue;
    for (std::size_t i = 0; i < needs.size(); ++i)
        queue.push(needs[i]);
    std::cout << "Dispatch order from the queue:\n";
    while (!queue.empty())
        PrintNeed()(queue.pop());

    std::sort(needs.begin(), needs.end(), MoreUrgent());
    std::cout << "Same order from std::sort with the MoreUrgent functor, printed with std::for_each:\n";
    std::for_each(needs.begin(), needs.end(), PrintNeed());

    std::unordered_map<int, int> victimsBySeverity;
    for (std::size_t i = 0; i < needs.size(); ++i)
        victimsBySeverity[needs[i].severity] += needs[i].victims;
    std::cout << "Victims at severity 5 (unordered_map): " << victimsBySeverity[5] << '\n';

    // 3. Binary persistence with atomic updates
    std::cout << "\n-- Binary persistence --\n";
    const std::string logFile = "erl_transit.log";
    const std::string auditFile = "erl_manifest_audit.dat";
    Persistence::saveAtomic(logFile, log);
    std::vector<TransitLogRecord> loaded = Persistence::loadRecords<TransitLogRecord>(logFile);
    std::cout << "Saved and reloaded " << loaded.size() << " transit records:\n";
    printLog(loaded);

    TransitLogRecord extra = log.empty() ? TransitLogRecord() : log.front();
    extra.sequence = 99;
    Persistence::appendAtomic(logFile, extra);
    std::cout << "After appendAtomic: " << Persistence::loadRecords<TransitLogRecord>(logFile).size() << " records\n";

    Manifest audited(501, 2);
    audited.add(ReliefCargo("Ration_Kit", Category::Ration, 25, 12.0));
    audited.add(ReliefCargo("Trauma_Kit", Category::Medicine, 15, 2.5));
    Persistence::saveAtomic(auditFile, audited.toAudit());
    Manifest back = Manifest::fromAudit(501, Persistence::loadRecords<ManifestAuditRecord>(auditFile));
    std::cout << "Manifest audit round trip equal: " << (back == audited ? "true" : "false") << '\n';

    {   // damage one byte on disk with seekp, then try to load
        std::fstream f(logFile.c_str(), std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(sizeof(Persistence::FileHeader) + 6);
        f.put('\x7f');
    }
    try {
        Persistence::loadRecords<TransitLogRecord>(logFile);
    } catch (const PersistenceException &e) {
        std::cout << "Corrupted file detected: " << e.what() << '\n';
    }
    {   // cut the file short, then try to load
        std::vector<char> bytes;
        std::ifstream in(auditFile.c_str(), std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        in.close();
        std::ofstream out(auditFile.c_str(), std::ios::binary | std::ios::trunc);
        out.write(&bytes[0], static_cast<std::streamsize>(bytes.size() - 5));
    }
    try {
        Persistence::loadRecords<ManifestAuditRecord>(auditFile);
    } catch (const PersistenceException &e) {
        std::cout << "Truncated file detected: " << e.what() << '\n';
    }
}

int main() {
    std::cout << "ERL-HUB: Post-Earthquake Multi-Agency Emergency Relief Logistics Hub\n";
    unitOne();
    unitTwo();
    std::vector<TransitLogRecord> log = unitThree();
    unitFour(log);
    std::cout << "\nAll demonstrations finished.\n";
    return 0;
}
