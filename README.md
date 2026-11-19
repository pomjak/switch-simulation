# Switch simulation

This repository contains a discrete-event simulation of an Ethernet switch,
implemented in C++ with [SIMLIB](https://www.simlib.eu/). The model represents
packet processing through ingress ports, MAC extraction, CAM-table learning and
lookup, encapsulation, buffering, and egress ports.

The simulation can be used to study contention and packet loss caused by:

- a shared CAM table,
- limited MAC extractors and encapsulators,
- finite shared-buffer capacity,
- unavailable ingress or egress ports,
- CAM-table overflow and fail-open flooding,
- different traffic loads, packet sizes, and numbers of generators.

## Repository contents

| Path | Description |
| --- | --- |
| `model.cc`, `model.h` | Simulation implementation and model declarations |
| `Makefile` | Build and experiment targets |
| `docs/` | Petri-net diagrams for packet arrival, CAM processing, lookup, and sending |
| `simulation_study.pdf` | Study/report accompanying the model |

## Requirements

- GNU Make
- A C++11-compatible compiler
- SIMLIB development headers and library

The Makefile expects the SIMLIB library to be available to the linker as
`-lsimlib`, together with the standard math library.

## Build and run

Build the simulator:

```sh
make
```

Run the default configuration:

```sh
make run
```

The executable is named `model`. By default it runs for 1 second of simulation
time and writes SIMLIB statistics to `model.out`.

Run the command directly with custom parameters:

```sh
./model -n 5 -t 1 -g 5 -l 0.5 -c 5 -o experiments/custom.out
```

Create the predefined experiment outputs:

```sh
make exp
```

The experiment targets write their results to `experiments/`:

| Target | Scenario |
| --- | --- |
| `exp1` | Small-run datasheet validation |
| `exp2` | MAC-address flooding with many generated addresses |
| `exp3` | Normal traffic |
| `exp4` | One versus two encapsulator/MAC-extractor resources |
| `exp5` | Tail-drop behavior with different resource counts |

Individual scenarios can also be run with `make exp1`, `make exp2`, and so on.

## Command-line options

```text
-n <num_ports>       Number of switch ports (default: 5)
-t <seconds>         Simulation duration (default: 1)
-b <capacity>        Shared-buffer capacity in bytes (default: 131072)
-m <entries>         CAM-table capacity (default: 2048)
-c <count>           Number of generated MAC addresses (default: 5)
-s <bytes>           Fixed packet size; 0 uses a traffic-size distribution
-l <load>            Offered load factor (default: 0.5)
-g <count>           Number of packet generators (default: 5)
-e <count>           Number of encapsulator and MAC-extractor resources
-o <file>            SIMLIB output file (default: model.out)
```

With the default packet-size setting (`-s 0`), generated packets are
distributed across minimum-size Ethernet frames, medium frames, and frames up
to 1518 bytes. If the destination MAC address is not present in the CAM table,
the packet is flooded to all ports except its source port. When the CAM table
reaches its configured capacity, the model enters fail-open mode and floods
subsequent traffic.

## Output

The output file contains SIMLIB resource statistics and model counters,
including:

- total and unfinished packets,
- total generated and processed bytes,
- packet lifetime and packet-size statistics,
- resource utilization for the buffer, MAC extractor, encapsulator, and CAM table,
- drops caused by unavailable ports, tail drop, and processing/buffer contention,
- total flood count,
- whether the CAM table entered fail-open mode.

Generated executables, `.out` files, and the `experiments/` directory are
ignored by Git. Remove generated files with:

```sh
make clean
```

## Visual documentation

The packet-processing flow is also documented as Petri-net diagrams:

- [Packet arrival](docs/petri-net-arrival.drawio.png)
- [CAM-table processing](docs/petri-net-cam.drawio.png)
- [CAM lookup](docs/petri-net-lookup.drawio.png)
- [Packet sending](docs/petri-net-send.drawio.png)

See [`simulation_study.pdf`](simulation_study.pdf) for the full simulation
study and analysis.
