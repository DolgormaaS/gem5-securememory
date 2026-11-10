#ifndef __MEM_SECURE_MEMORY_EPMP_HH__
#define __MEM_SECURE_MEMORY_EPMP_HH__

#include <vector>

#include "base/addr_range.hh"
#include "base/types.hh"
#include "params/EPMP.hh"
#include "sim/sim_object.hh"

namespace gem5
{
namespace memory
{

/**
 * Aggregated extended PMP table (Moolman & Lehman).
 * One system-wide table of N cores x P entries, kept in sync with the
 * per-core PMP tables via create/delete commands. Secure-memory stages
 * consult it by physical address, since Ruby and LLC writebacks don't
 * carry request flags or core IDs down to memory.
 */
class EPMP : public SimObject
{
  public:
    struct Entry
    {
        bool valid = false;
        AddrRange range;        // copied from the core's PMP entry
        uint64_t root = 0;      // on-chip BMT root (8B)
        uint8_t treeMode = 0;   // number of tree levels for this region
    };

    PARAMS(EPMP);
    EPMP(const Params &p);

    /** Paper's "create new entry" command. */
    void createEntry(unsigned core, unsigned idx, const AddrRange &range);

    /** Paper's "delete entry" command. */
    void deleteEntry(unsigned core, unsigned idx);

    /** Valid entry covering addr, or nullptr -> bypass. */
    const Entry *lookup(Addr addr) const;

    bool isSecure(Addr addr) const { return lookup(addr) != nullptr; }

  private:
    unsigned numCores;
    unsigned entriesPerCore;
    unsigned blockSize;
    unsigned counterArity;
    unsigned treeArity;

    std::vector<Entry> table;

    unsigned
    slot(unsigned core, unsigned idx) const
    {
        return core * entriesPerCore + idx;
    }

    /** Tree levels needed to cover a region of this size. */
    uint8_t computeTreeMode(Addr size) const;
};

} // namespace memory
} // namespace gem5

#endif // __MEM_SECURE_MEMORY_EPMP_HH__
