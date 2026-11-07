import argparse
import shlex
import time

import m5

from gem5.coherence_protocol import CoherenceProtocol
from gem5.components.boards.simple_board import SimpleBoard
from gem5.components.cachehierarchies.ruby.mesi_two_level_cache_hierarchy import (
    MESITwoLevelCacheHierarchy,
)
from gem5.components.memory import DualChannelDDR4_2400
from gem5.components.processors.cpu_types import CPUTypes
from gem5.components.processors.simple_processor import SimpleProcessor
from gem5.isas import ISA
from gem5.resources.resource import BinaryResource
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
parser.add_argument("--encryption_latency", type=int, default=53)
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

memory = DualChannelDDR4_2400(size="3GiB")

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
    env_list=["GEM5_AT_RANDOM_PAD=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"],
)

simulator = Simulator(board=board)

globalStart = time.time()
print("Running the simulation")
simulator.run()

print("Done with the simulation")
print(
    "Ran a total of", simulator.get_current_tick() / 1e12, "simulated seconds"
)
print("Total wallclock time: %.2fs" % (time.time() - globalStart))
