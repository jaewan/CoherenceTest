# Federated Coherence Experiment

This project simulates and evaluates different memory coherence models in a multi-socket NUMA architecture. It uses a MapReduce-style workload to measure the performance impact of three models:

*   **Fully Coherent:** A traditional hardware-based cache coherence protocol where all sockets maintain coherence through hardware. This model incurs a "coherence tax" on inter-socket communication.
*   **Federated Coherence:** A hybrid model where intra-socket communication is hardware-coherent, but inter-socket communication uses software-based messaging (via Bakery locks), avoiding expensive global coherence protocols.
*   **Fully Non-Coherent:** All synchronization is done via software locks (Bakery algorithm), simulating a system with no hardware coherence support between or within sockets.

The experiment is designed to highlight the performance trade-offs associated with the "coherence tax" in multi-socket systems.

## Project Structure

```
federated_coherence/
├── src/                    # Source code
│   ├── experiment.c       # Main experiment driver
│   ├── workload.c         # Map, Shuffle, Reduce workload implementation
│   ├── emulation.c        # System emulation and NUMA topology detection
│   ├── sync.c             # Generic synchronization primitives (HW and SW locks)
│   └── timer.c            # High-precision timing
├── include/                # Header files
│   ├── workload.h
│   ├── emulation.h
│   ├── sync.h
│   └── timer.h
├── Makefile                # Build configuration
└── README.md               # This file
```

## Workload: Map, Shuffle, Reduce

The experiment uses a three-phase workload to simulate a common parallel computing pattern:

1.  **Map Phase:** Threads perform parallel computation on local data within their socket. This phase is designed to generate coherence traffic in the "Fully Coherent" model as threads from different sockets access a shared resource.
2.  **Shuffle Phase:** A barrier synchronization phase where threads wait for all other threads to complete the Map phase before proceeding.
3.  **Reduce Phase:** Threads from all sockets collaboratively work on a globally shared data structure, requiring inter-socket synchronization.

## Building the Experiment

The project uses a `Makefile`. To build the experiment, simply run:

```bash
make
```

This will compile the source files and create an executable named `experiment` in the root directory.

## Running the Experiment

To run the experiment with default settings (evaluating all system types for 5 trials each), execute the binary:

```bash
./experiment
```

### Command-Line Options

The experiment's behavior can be customized with the following options:

```
Usage: ./experiment [options]
Options:
  -s <system>     System type: coherent, federated, non-coherent, all (default: all)
  -t <threads>    Threads per socket (default: 4)
  -i <increments> Increments per thread during the Map phase (default: 1000)
  -c <cycles>     Compute cycles between increments in the Map phase (default: 100000)
  -n <trials>     Number of trials to run and average (default: 5)
  -v              Verbose output
  -h              Show this help
```

### Example Usage

*   Run only the "federated" coherence model with 8 threads per socket:
    ```bash
    ./experiment -s federated -t 8
    ```

*   Run a quick, single-trial experiment for all systems with a smaller workload:
    ```bash
    ./experiment -n 1 -i 100 -c 1000
    ```
