# ERL-HUB: Post-Earthquake Multi-Agency Emergency Relief Logistics Hub

Project for **CIC-211 Object-Oriented Programming Using C++** (Option 2).
Standard C++17, no third party libraries.

The program models a relief hub: cargo and depots, a manifest that owns dynamic memory,
a fleet of heterogeneous vehicles (including an amphibious medical unit built through a
virtual-base diamond), urgent medical requests ordered by a priority queue, and binary
transit logs that are updated atomically.

## Build and run

Requires `g++` 9 or newer (or any C++17 compiler).

```
make            # builds ./erlhub and ./run_tests
make run        # runs the demonstration of all four units
make test       # runs the 62 self checks
make clean
```

Without `make` (for example plain MinGW on Windows):

```
g++ -std=c++17 -Wall -Wextra -Iinclude src/Inventory.cpp src/Logistics.cpp src/main.cpp -o erlhub
g++ -std=c++17 -Wall -Wextra -Iinclude src/Inventory.cpp src/Logistics.cpp tests/run_tests.cpp -o run_tests
```

The demo writes two small files (`erl_transit.log`, `erl_manifest_audit.dat`) in the
current folder. They are ignored by git.

## Layout

```
include/Exceptions.hpp             custom exception hierarchy (std::exception)
include/Inventory.hpp, src/Inventory.cpp
                                   ReliefCargo, Manifest, DistributionDepot,
                                   allocate() overloads, IncidentCommander
include/Logistics.hpp, src/Logistics.cpp
                                   ReliefVehicle hierarchy, GPSBeacon,
                                   EmergencyLifeSupportUnit, Corridor, inline utilities
include/PriorityDispatchQueue.hpp  generic template class
include/MedicalNeed.hpp            request record and ordering functors
include/Persistence.hpp            atomic binary save/load (template)
include/Manipulators.hpp           user defined I/O manipulators
src/main.cpp                       demo, one section per unit
tests/run_tests.cpp                62 checks, no framework
docs/class_diagram.png             class diagram
```

## Where each requirement lives

| Unit | Requirement | Where |
|---|---|---|
| I | Namespaces `ReliefOps::Logistics`, `ReliefOps::Inventory` | all headers |
| I | Function overloading for allocation | `allocate()` x2 in `Inventory.hpp` |
| I | Call by reference / return by reference | `rerouteAround()`, `Corridor::markRoadDamaged()`, `DistributionDepot::item()` |
| I | Inline utilities | `distanceKm`, `payloadFraction`, `etaHours` in `Logistics.hpp` |
| I | Friend class `IncidentCommander` | `Inventory.hpp`, `overrideReserve()` |
| II | ADT classes, information hiding, const correctness | `ReliefCargo`, `Manifest`, `DistributionDepot`, `ReliefVehicle` |
| II | Static data and functions | `ReliefVehicle::operationalVehicles()`, `totalDispatchedTonnes()` |
| II | Dynamic constructor, copy constructor, destructor | `Manifest` |
| II | `+=`, `-=`, `==`, `<<`, `>>` | `DistributionDepot`, `Manifest`, `ReliefCargo` |
| III | Base `ReliefVehicle` and three derived vehicles | `Logistics.hpp` |
| III | Virtual base classes, diamond | `AmphibiousMedicalUnit` |
| III | Containership | `GPSBeacon`, `EmergencyLifeSupportUnit` inside `ReliefVehicle` |
| III | Runtime polymorphism, virtual destructor | `main.cpp`, `unitThree()` |
| IV | Exception framework | `Exceptions.hpp` |
| IV | Binary streams, atomic updates | `Persistence.hpp` |
| IV | `PriorityDispatchQueue<T>`, `vector`, `unordered_map`, functors, manipulators | `PriorityDispatchQueue.hpp`, `MedicalNeed.hpp`, `Manipulators.hpp`, `main.cpp` |

## Verification

* Compiles with `-Wall -Wextra -Wpedantic` and no warnings.
* All 62 checks in `tests/run_tests.cpp` pass.
* Demo and tests run clean under AddressSanitizer, LeakSanitizer and UBSan (no leaks, no errors).
* Output is identical at `-O0` and `-O2`.
