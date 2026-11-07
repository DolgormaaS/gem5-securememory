import argparse
import shlex
import time

import m5

from gem5.coherence_protocol import CoherenceProtocol
from gem5.components.boards.simple_board import SimpleBoard
from gem5.components.cachehierarchies.ruby.mesi_two_level_cache_hierarchy import (
    MESITwoLevelCacheHierarchy,
)
from gem5.components.memory.secure_ddr4 import MACProtectedMemory
from gem5.components.processors.cpu_types import CPUTypes
from gem5.components.processors.simple_processor import SimpleProcessor
from gem5.isas import ISA
from gem5.resources.resource import BinaryResource
from gem5.simulate.exit_event import ExitEvent
from gem5.simulate.simulator import Simulator
from gem5.utils.requires import requires

requires(
    isa_required=ISA.RISCV,
    coherence_protocol_required=CoherenceProtocol.MESI_TWO_LEVEL,
)

parser = argparse.ArgumentParser()
parser.add_argument(
    "--binary", type=str, required=True, help="Path to the RISC-V SPEC binary"
)
parser.add_argument(
    "--args",
    type=str,
    default="",
    help='Benchmark arguments, e.g. --args "inp.in"',
)
parser.add_argument(
    "--fast_forward_insts",
    type=int,
    default=2_000_000_000,
    help="Instructions to skip before measuring (0 = none)",
)
parser.add_argument(
    "--max_insts",
    type=int,
    default=500_000_000,
    help="Instructions to measure (0 = run to completion)",
)
parser.add_argument("--metadata_cache_size", type=str, default="64kB")
parser.add_argument("--no_metadata_cache", action="store_true")
parser.add_argument("--arity", type=int, default=64)
parser.add_argument("--encryption_latency", type=int, default=53)
parser.add_argument("--cache_mac", action="store_true")
args = parser.parse_args()

cache_hierarchy = MESITwoLevelCacheHierarchy(
    l1d_size="32KiB",
    l1d_assoc=8,
    l1i_size="32KiB",
    l1i_assoc=8,
    l2_size="256KiB",
    l2_assoc=16,
    num_l2_banks=2,
)

memory = MACProtectedMemory(
    size="3GiB",
    latency=args.encryption_latency,
    cache=not args.no_metadata_cache,
    cache_size=args.metadata_cache_size,
    arity=args.arity,
    cache_mac=args.cache_mac,
)

processor = SimpleProcessor(cpu_type=CPUTypes.O3, isa=ISA.RISCV, num_cores=1)

board = SimpleBoard(
    clk_freq="3GHz",
    processor=processor,
    memory=memory,
    cache_hierarchy=cache_hierarchy,
)

board.set_se_binary_workload(
    binary=BinaryResource(local_path=args.binary),
    arguments=shlex.split(args.args),
    # Workaround: gem5's RISC-V SE setup overwrites the last string on the
    # stack with AT_RANDOM bytes; this pad absorbs it instead of an argument.
    env_list=["GEM5_AT_RANDOM_PAD=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"],
)

start_tick = 0


def handle_max_insts():
    global start_tick
    if args.fast_forward_insts > 0:
        print(
            f"Skipped {args.fast_forward_insts} instructions: resetting stats"
        )
        m5.stats.reset()
        start_tick = m5.curTick()
        if args.max_insts > 0:
            simulator.schedule_max_insts(args.max_insts)
        yield False  # Continue into the measured region.
    print(f"Measured {args.max_insts} instructions: dumping stats")
    m5.stats.dump()
    yield True  # Stop the simulation.


simulator = Simulator(
    board=board,
    on_exit_event={ExitEvent.MAX_INSTS: handle_max_insts()},
)

if args.fast_forward_insts > 0:
    simulator.schedule_max_insts(args.fast_forward_insts)
elif args.max_insts > 0:
    simulator.schedule_max_insts(args.max_insts)

globalStart = time.time()
print("Running the simulation")
simulator.run()
end_tick = m5.curTick()

print("Done with the simulation")
print(
    f"Simulated time in measured region: {(end_tick - start_tick) / 1e12:.6f}s"
)
print(
    "Ran a total of", simulator.get_current_tick() / 1e12, "simulated seconds"
)
print("Total wallclock time: %.2fs" % (time.time() - globalStart))
