#ifndef ERL_INVENTORY_HPP
#define ERL_INVENTORY_HPP

#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

#include "Exceptions.hpp"

namespace ReliefOps {
namespace Inventory {

enum class Category { Ration, Medicine, Water, Blanket };
const char *categoryName(Category c);

class IncidentCommander;   // friend class, defined below

// ---------------------------------------------------------------- ReliefCargo
// Abstract data type: all data is private, access goes through const-correct methods.
class ReliefCargo {
private:
    std::string name_;
    Category category_;
    int quantity_;
    double unitKg_;
    int reserved_;   // units locked for triage; only IncidentCommander may release them

    friend class IncidentCommander;

public:
    ReliefCargo(const std::string &name = "unnamed", Category c = Category::Ration, int qty = 0,
                double unitKg = 0.0);

    const std::string &name() const { return name_; }
    Category category() const { return category_; }
    int quantity() const { return quantity_; }
    double unitKg() const { return unitKg_; }
    int available() const { return quantity_ - reserved_; }
    double totalKg() const { return quantity_ * unitKg_; }

    void add(int units);
    void remove(int units);   // honours the reserve, throws InsufficientStockException
    void reserve(int units);  // locking is public, releasing is not

    bool sameItem(const ReliefCargo &other) const;     // duplicate cargo check
    bool operator==(const ReliefCargo &other) const;   // identical line (item and amount)

    friend std::ostream &operator<<(std::ostream &os, const ReliefCargo &c);
    friend std::istream &operator>>(std::istream &is, ReliefCargo &c);
};

// ------------------------------------------------------------------- Manifest
// Owns a dynamically allocated array of cargo lines, so it needs the "rule of three":
// dynamic constructor, deep copy constructor, copy assignment and destructor.
struct ManifestAuditRecord {   // plain data, written to disk by Persistence
    int manifestId;
    int category;
    int quantity;
    double unitKg;
    char item[24];
};

class Manifest {
private:
    int id_;
    ReliefCargo *lines_;
    int count_;
    int capacity_;

    void grow();

public:
    explicit Manifest(int id = 0, int capacity = 4);   // dynamic constructor
    Manifest(const Manifest &other);                   // deep copy
    Manifest &operator=(const Manifest &other);
    ~Manifest();

    bool add(const ReliefCargo &c);   // true: new line, false: merged into a duplicate
    int id() const { return id_; }
    int size() const { return count_; }
    const ReliefCargo &line(int index) const;
    double totalKg() const;

    bool operator==(const Manifest &other) const;   // manifest identity check

    void printTable(std::ostream &os) const;
    std::vector<ManifestAuditRecord> toAudit() const;
    static Manifest fromAudit(int id, const std::vector<ManifestAuditRecord> &records);

    friend std::ostream &operator<<(std::ostream &os, const Manifest &m);
    friend std::istream &operator>>(std::istream &is, Manifest &m);
};

// -------------------------------------------------------------------- Depot
class DistributionDepot {
private:
    std::string name_;
    std::unordered_map<std::string, ReliefCargo> stock_;

    friend class IncidentCommander;

public:
    explicit DistributionDepot(const std::string &name) : name_(name), stock_() {}

    const std::string &name() const { return name_; }

    DistributionDepot &operator+=(const ReliefCargo &c);   // restock
    DistributionDepot &operator-=(const ReliefCargo &c);   // deduct on vehicle departure

    ReliefCargo &item(const std::string &name);              // return by reference
    const ReliefCargo &item(const std::string &name) const;
    bool has(const std::string &name) const;
    std::vector<std::string> itemNames() const;
    void print(std::ostream &os) const;
};

// ------------------------------------------------- overloaded allocation routines
// Volume based payload: take up to volumeKg of an item. Returns the kg allocated.
double allocate(DistributionDepot &depot, const std::string &item, double volumeKg, Manifest &out);

// Priority medical triage: units scale with victims and priority (4 or 5 get double).
// Returns the units allocated.
int allocate(DistributionDepot &depot, const std::string &item, int victims, int priority, Manifest &out);

// ------------------------------------------------------------ IncidentCommander
// Friend of ReliefCargo and DistributionDepot: may bypass the triage reserve.
class IncidentCommander {
public:
    int reservedOf(const ReliefCargo &c) const { return c.reserved_; }
    int overrideReserve(DistributionDepot &depot, const std::string &item);   // returns units released
};

}  // namespace Inventory
}  // namespace ReliefOps

#endif
