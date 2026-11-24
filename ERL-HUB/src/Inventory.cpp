#include "Inventory.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "Manipulators.hpp"

namespace ReliefOps {
namespace Inventory {

const char *categoryName(Category c) {
    switch (c) {
        case Category::Ration: return "Ration";
        case Category::Medicine: return "Medicine";
        case Category::Water: return "Water";
        case Category::Blanket: return "Blanket";
    }
    return "Unknown";
}

static bool parseCategory(const std::string &text, Category &out) {
    const Category all[] = {Category::Ration, Category::Medicine, Category::Water, Category::Blanket};
    for (Category c : all) {
        if (text == categoryName(c)) {
            out = c;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------- ReliefCargo
ReliefCargo::ReliefCargo(const std::string &name, Category c, int qty, double unitKg)
    : name_(name), category_(c), quantity_(qty < 0 ? 0 : qty), unitKg_(unitKg), reserved_(0) {}

void ReliefCargo::add(int units) {
    if (units > 0)
        quantity_ += units;
}

void ReliefCargo::remove(int units) {
    if (units > available())
        throw InsufficientStockException(name_, units, available());
    quantity_ -= units;
}

void ReliefCargo::reserve(int units) {
    reserved_ = std::min(std::max(units, 0), quantity_);
}

bool ReliefCargo::sameItem(const ReliefCargo &o) const {
    return name_ == o.name_ && category_ == o.category_;
}

bool ReliefCargo::operator==(const ReliefCargo &o) const {
    return sameItem(o) && quantity_ == o.quantity_ && std::fabs(unitKg_ - o.unitKg_) < 1e-9;
}

std::ostream &operator<<(std::ostream &os, const ReliefCargo &c) {
    return os << c.name_ << ' ' << categoryName(c.category_) << ' ' << c.quantity_ << ' ' << c.unitKg_;
}

std::istream &operator>>(std::istream &is, ReliefCargo &c) {
    std::string name, cat;
    int qty;
    double kg;
    if (!(is >> name >> cat >> qty >> kg))
        return is;
    Category parsed;
    if (!parseCategory(cat, parsed) || qty < 0 || kg < 0) {
        is.setstate(std::ios::failbit);
        return is;
    }
    c = ReliefCargo(name, parsed, qty, kg);
    return is;
}

// ------------------------------------------------------------------- Manifest
Manifest::Manifest(int id, int capacity)
    : id_(id), lines_(nullptr), count_(0), capacity_(capacity > 0 ? capacity : 1) {
    lines_ = new ReliefCargo[capacity_];
}

Manifest::Manifest(const Manifest &o)
    : id_(o.id_), lines_(new ReliefCargo[o.capacity_]), count_(o.count_), capacity_(o.capacity_) {
    for (int i = 0; i < count_; ++i)
        lines_[i] = o.lines_[i];
}

Manifest &Manifest::operator=(const Manifest &o) {
    if (this != &o) {
        ReliefCargo *fresh = new ReliefCargo[o.capacity_];   // allocate first: safe if new throws
        for (int i = 0; i < o.count_; ++i)
            fresh[i] = o.lines_[i];
        delete[] lines_;
        lines_ = fresh;
        id_ = o.id_;
        count_ = o.count_;
        capacity_ = o.capacity_;
    }
    return *this;
}

Manifest::~Manifest() {
    delete[] lines_;
}

void Manifest::grow() {
    const int newCapacity = capacity_ * 2;
    ReliefCargo *bigger = new ReliefCargo[newCapacity];
    for (int i = 0; i < count_; ++i)
        bigger[i] = lines_[i];
    delete[] lines_;
    lines_ = bigger;
    capacity_ = newCapacity;
}

bool Manifest::add(const ReliefCargo &c) {
    for (int i = 0; i < count_; ++i) {
        if (lines_[i].sameItem(c)) {   // duplicate cargo: merge instead of adding a second line
            lines_[i].add(c.quantity());
            return false;
        }
    }
    if (count_ == capacity_)
        grow();
    lines_[count_++] = c;
    return true;
}

const ReliefCargo &Manifest::line(int index) const {
    if (index < 0 || index >= count_)
        throw std::out_of_range("manifest line index out of range");
    return lines_[index];
}

double Manifest::totalKg() const {
    double sum = 0.0;
    for (int i = 0; i < count_; ++i)
        sum += lines_[i].totalKg();
    return sum;
}

bool Manifest::operator==(const Manifest &o) const {
    if (id_ != o.id_ || count_ != o.count_)
        return false;
    for (int i = 0; i < count_; ++i)
        if (!(lines_[i] == o.lines_[i]))
            return false;
    return true;
}

void Manifest::printTable(std::ostream &os) const {
    using namespace ReliefOps::IO;
    const std::ios_base::fmtflags savedFlags = os.flags();
    const std::streamsize savedPrecision = os.precision();

    os << "Manifest #" << id_ << '\n' << rule;
    os << cell(18) << "ITEM" << cell(12) << "CATEGORY" << cell(8) << "QTY" << cell(10) << "KG/UNIT"
       << cell(10) << "TOTAL KG" << '\n' << rule;
    for (int i = 0; i < count_; ++i) {
        os << cell(18) << lines_[i].name() << cell(12) << categoryName(lines_[i].category()) << cell(8)
           << lines_[i].quantity() << oneDecimal << cell(10) << lines_[i].unitKg() << cell(10)
           << lines_[i].totalKg() << '\n';
    }
    os << rule << cell(48) << "TOTAL" << oneDecimal << totalKg() << '\n';

    os.flags(savedFlags);
    os.precision(savedPrecision);
}

std::vector<ManifestAuditRecord> Manifest::toAudit() const {
    std::vector<ManifestAuditRecord> out;
    for (int i = 0; i < count_; ++i) {
        ManifestAuditRecord r;
        std::memset(&r, 0, sizeof(r));
        r.manifestId = id_;
        r.category = static_cast<int>(lines_[i].category());
        r.quantity = lines_[i].quantity();
        r.unitKg = lines_[i].unitKg();
        std::strncpy(r.item, lines_[i].name().c_str(), sizeof(r.item) - 1);
        out.push_back(r);
    }
    return out;
}

Manifest Manifest::fromAudit(int id, const std::vector<ManifestAuditRecord> &records) {
    Manifest m(id, records.empty() ? 1 : static_cast<int>(records.size()));
    for (const ManifestAuditRecord &r : records) {
        if (r.manifestId == id)
            m.add(ReliefCargo(r.item, static_cast<Category>(r.category), r.quantity, r.unitKg));
    }
    return m;
}

std::ostream &operator<<(std::ostream &os, const Manifest &m) {
    os << "MANIFEST " << m.id_ << ' ' << m.count_ << '\n';
    for (int i = 0; i < m.count_; ++i)
        os << m.lines_[i] << '\n';
    return os;
}

std::istream &operator>>(std::istream &is, Manifest &m) {
    std::string tag;
    int id, n;
    if (!(is >> tag >> id >> n))
        return is;
    if (tag != "MANIFEST" || n < 0) {
        is.setstate(std::ios::failbit);
        return is;
    }
    Manifest temp(id, n > 0 ? n : 1);
    for (int i = 0; i < n; ++i) {
        ReliefCargo c;
        if (!(is >> c))
            return is;
        temp.add(c);
    }
    m = temp;
    return is;
}

// -------------------------------------------------------------------- Depot
DistributionDepot &DistributionDepot::operator+=(const ReliefCargo &c) {
    std::unordered_map<std::string, ReliefCargo>::iterator it = stock_.find(c.name());
    if (it == stock_.end())
        stock_.insert(std::make_pair(c.name(), c));
    else
        it->second.add(c.quantity());
    return *this;
}

DistributionDepot &DistributionDepot::operator-=(const ReliefCargo &c) {
    std::unordered_map<std::string, ReliefCargo>::iterator it = stock_.find(c.name());
    if (it == stock_.end())
        throw InsufficientStockException(c.name(), c.quantity(), 0);
    it->second.remove(c.quantity());
    return *this;
}

ReliefCargo &DistributionDepot::item(const std::string &name) {
    std::unordered_map<std::string, ReliefCargo>::iterator it = stock_.find(name);
    if (it == stock_.end())
        throw InsufficientStockException(name, 1, 0);
    return it->second;
}

const ReliefCargo &DistributionDepot::item(const std::string &name) const {
    std::unordered_map<std::string, ReliefCargo>::const_iterator it = stock_.find(name);
    if (it == stock_.end())
        throw InsufficientStockException(name, 1, 0);
    return it->second;
}

bool DistributionDepot::has(const std::string &name) const {
    return stock_.find(name) != stock_.end();
}

std::vector<std::string> DistributionDepot::itemNames() const {
    std::vector<std::string> names;
    for (std::unordered_map<std::string, ReliefCargo>::const_iterator it = stock_.begin(); it != stock_.end(); ++it)
        names.push_back(it->first);
    std::sort(names.begin(), names.end());
    return names;
}

void DistributionDepot::print(std::ostream &os) const {
    using namespace ReliefOps::IO;
    os << "Depot: " << name_ << '\n' << rule;
    os << cell(18) << "ITEM" << cell(12) << "CATEGORY" << cell(10) << "ON HAND" << cell(10) << "RESERVED"
       << cell(8) << "FREE" << '\n' << rule;
    std::vector<std::string> names = itemNames();
    for (std::size_t i = 0; i < names.size(); ++i) {
        const ReliefCargo &c = item(names[i]);
        os << cell(18) << c.name() << cell(12) << categoryName(c.category()) << cell(10) << c.quantity()
           << cell(10) << (c.quantity() - c.available()) << cell(8) << c.available() << '\n';
    }
    os << rule;
}

// --------------------------------------------------------- allocation overloads
double allocate(DistributionDepot &depot, const std::string &itemName, double volumeKg, Manifest &out) {
    ReliefCargo &stock = depot.item(itemName);   // reference: no copy of the stock record
    if (stock.unitKg() <= 0.0)
        return 0.0;
    const int wanted = static_cast<int>(volumeKg / stock.unitKg());
    const int units = std::min(wanted, stock.available());
    if (units <= 0)
        return 0.0;
    ReliefCargo taken(stock.name(), stock.category(), units, stock.unitKg());
    depot -= taken;
    out.add(taken);
    return units * taken.unitKg();
}

int allocate(DistributionDepot &depot, const std::string &itemName, int victims, int priority, Manifest &out) {
    ReliefCargo &stock = depot.item(itemName);
    const int perVictim = (priority >= 4) ? 2 : 1;
    const int units = std::min(victims * perVictim, stock.available());
    if (units <= 0)
        return 0;
    ReliefCargo taken(stock.name(), stock.category(), units, stock.unitKg());
    depot -= taken;
    out.add(taken);
    return units;
}

// ------------------------------------------------------------ IncidentCommander
int IncidentCommander::overrideReserve(DistributionDepot &depot, const std::string &itemName) {
    std::unordered_map<std::string, ReliefCargo>::iterator it = depot.stock_.find(itemName);
    if (it == depot.stock_.end())
        return 0;
    const int released = it->second.reserved_;   // direct access to a private member
    it->second.reserved_ = 0;
    return released;
}

}  // namespace Inventory
}  // namespace ReliefOps
