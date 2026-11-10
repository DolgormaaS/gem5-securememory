#include "mem/secure_memory/epmp.hh"

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
      table(p.num_cores * p.entries_per_core)
{
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

    Entry &e = table[slot(core, idx)];
    e.valid = true;
    e.range = range;
    e.root = 0;                          // fresh tree
    e.treeMode = computeTreeMode(range.size());

    DPRINTF(EPMP, "create core %u idx %u: %s, treeMode %u\n",
            core, idx, range.to_string(), e.treeMode);
}

void
EPMP::deleteEntry(unsigned core, unsigned idx)
{
    panic_if(core >= numCores || idx >= entriesPerCore,
             "EPMP::deleteEntry: bad core %u / idx %u", core, idx);

    Entry &e = table[slot(core, idx)];
    DPRINTF(EPMP, "delete core %u idx %u: %s\n",
            core, idx, e.range.to_string());
    e = Entry();                          // root & mode -> 0, invalid
}

const EPMP::Entry *
EPMP::lookup(Addr addr) const
{
    for (const auto &e : table) {
        if (e.valid && e.range.contains(addr))
            return &e;
    }
    return nullptr;
}

} // namespace memory
} // namespace gem5
