#ifndef ERL_MEDICAL_NEED_HPP
#define ERL_MEDICAL_NEED_HPP

// Unit IV: request record plus the custom functors that order urgent medical needs.

#include <string>

namespace ReliefOps {
namespace Logistics {

struct MedicalNeed {
    std::string site;
    std::string item;
    int victims;
    int severity;   // 1 (minor) to 5 (critical)
};

// "a comes before b" when a is more urgent: used with std::sort
struct MoreUrgent {
    bool operator()(const MedicalNeed &a, const MedicalNeed &b) const {
        if (a.severity != b.severity) return a.severity > b.severity;
        if (a.victims != b.victims) return a.victims > b.victims;
        return a.site < b.site;
    }
};

// "a has lower priority than b": used as the Compare of PriorityDispatchQueue
struct LessUrgent {
    bool operator()(const MedicalNeed &a, const MedicalNeed &b) const {
        return MoreUrgent()(b, a);
    }
};

}  // namespace Logistics
}  // namespace ReliefOps

#endif
