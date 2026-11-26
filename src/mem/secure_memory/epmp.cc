#include "mem/secure_memory/epmp.hh"

#include "base/intmath.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/EPMP.hh"

namespace gem5
{
namespace memory
{

EPMP::EPMP(const Params &p)
    : SimObject(p),
      numCores(p.num_cores),
      entriesPerCore(p.entries_per_core),
      blockSize(p.block_size),
      counterArity(p.counter_arity),
      treeArity(p.tree_arity),
      regions(p.num_cores * p.entries_per_core),
      slotRegion(p.num_cores * p.entries_per_core, -1)
{
    fatal_if(numCores == 0 || entriesPerCore == 0,
             "EPMP: num_cores and entries_per_core must be > 0");
    fatal_if(blockSize == 0 || counterArity == 0,
             "EPMP: block_size and counter_arity must be > 0");
    fatal_if(treeArity < 2, "EPMP: tree_arity must be >= 2");
}

uint8_t
EPMP::computeTreeMode(Addr size) const
{
    // data blocks -> counter blocks -> tree levels up to a single root
    Addr nodes = divCeil(size, (Addr)blockSize);
    nodes = divCeil(nodes, (Addr)counterArity);
    uint8_t levels = 0;
    while (nodes > 1) {
        nodes = divCeil(nodes, (Addr)treeArity);
        levels++;
    }
    return levels;
}

void
EPMP::createEntry(unsigned core, unsigned idx, const AddrRange &range)
{
    panic_if(core >= numCores || idx >= entriesPerCore,
             "EPMP::createEntry: bad core %u / idx %u", core, idx);

    const unsigned s = slot(core, idx);
    if (slotRegion[s] >= 0)
        deleteEntry(core, idx);    // slot reused: drop the old mapping

    // Share an existing region with an identical range.
    for (size_t r = 0; r < regions.size(); r++) {
        Entry &e = regions[r];
        if (!e.valid)
            continue;
        if (e.range == range) {
            e.refCount++;
            slotRegion[s] = r;
            DPRINTF(EPMP, "create core %u idx %u: share region %d %s "
                    "(refs %u)\n", core, idx, r, range.to_string(),
                    e.refCount);
            return;
        }
        if (e.range.intersects(range)) {
            warn("EPMP: core %u idx %u range %s overlaps region %s; "
                 "lookup() will return the first match\n",
                 core, idx, range.to_string(), e.range.to_string());
        }
    }

    // Otherwise allocate a fresh region.
    for (size_t r = 0; r < regions.size(); r++) {
        Entry &e = regions[r];
        if (e.valid)
            continue;
        e.valid = true;
        e.range = range;
        e.root = 0;                          // fresh tree
        e.treeMode = computeTreeMode(range.size());
        e.refCount = 1;
        slotRegion[s] = r;
        DPRINTF(EPMP, "create core %u idx %u: new region %d %s, "
                "treeMode %u\n", core, idx, r, range.to_string(),
                e.treeMode);
        return;
    }

    panic("EPMP::createEntry: no free region for core %u idx %u",
          core, idx);
}

void
EPMP::deleteEntry(unsigned core, unsigned idx)
{
    panic_if(core >= numCores || idx >= entriesPerCore,
             "EPMP::deleteEntry: bad core %u / idx %u", core, idx);

    const unsigned s = slot(core, idx);
    const int r = slotRegion[s];
    if (r < 0)
        return;                              // nothing mapped

    slotRegion[s] = -1;
    Entry &e = regions[r];

    if (--e.refCount == 0) {
        DPRINTF(EPMP, "delete core %u idx %u: region %d %s freed\n",
                core, idx, r, e.range.to_string());
        e = Entry();                         // root & mode -> 0, invalid
    } else {
        DPRINTF(EPMP, "delete core %u idx %u: region %d %s "
                "(refs %u)\n", core, idx, r, e.range.to_string(),
                e.refCount);
    }
}

const EPMP::Entry *
EPMP::lookup(Addr addr) const
{
    for (const auto &e : regions) {
        if (e.valid && e.range.contains(addr))
            return &e;
    }
    return nullptr;
}

EPMP::Entry *
EPMP::lookup(Addr addr)
{
    return const_cast<Entry *>(
        static_cast<const EPMP *>(this)->lookup(addr));
}

} // namespace memory
} // namespace gem5
