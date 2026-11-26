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

class EPMP : public SimObject
{
  public:
    struct Entry
    {
        bool valid = false;
        AddrRange range;        // copied from the core's PMP entry
        uint64_t root = 0;      // on-chip BMT root (8B)
        uint8_t treeMode = 0;   // number of tree levels for this region
        unsigned refCount = 0;  // slots currently mapped to this region
    };

    PARAMS(EPMP);
    EPMP(const Params &p);

    /** Paper's "create new entry" command. */
    void createEntry(unsigned core, unsigned idx, const AddrRange &range);

    /** Paper's "delete entry" command. */
    void deleteEntry(unsigned core, unsigned idx);

    /** Valid region covering addr, or nullptr -> bypass. */
    const Entry *lookup(Addr addr) const;
    Entry *lookup(Addr addr);

    bool isSecure(Addr addr) const { return lookup(addr) != nullptr; }

    unsigned getNumCores() const { return numCores; }
    unsigned getEntriesPerCore() const { return entriesPerCore; }

  private:
    unsigned numCores;
    unsigned entriesPerCore;
    unsigned blockSize;
    unsigned counterArity;
    unsigned treeArity;

    /** Region records; at most N x P distinct regions. */
    std::vector<Entry> regions;

    /** (core, idx) slot -> index into regions, -1 if unmapped. */
    std::vector<int> slotRegion;

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
