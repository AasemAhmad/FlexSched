# FlexSched: MRCPSP-Based HPC Workload Scheduler

## Overview
FlexSched is a holistic scheduling tool that leverages the Multi-Mode Resource-Constrained Project Scheduling Problem [MRCPSP](https://github.com/AasemAhmad/MRCPSPs) framework to optimize workload scheduling in High-Performance Computing (HPC) environments. By incorporating complex resource constraints and multi-mode operations, FlexSched aims to achieve multiple objectives, including maximizing throughput, minimizing execution time for HPC workloads, and others.

FlexSched is being developed to be fully compatible with the [Batsim](https://batsim.readthedocs.io/en/latest/) simulator, enabling detailed simulation and evaluation of scheduling strategies.

**Note: FlexSched is currently under active development. The code will be made publicly available ASAP**

## Features
- **Resource Constraints**: Effectively manage compute and I/O resources.
- **Multi-Mode Operations**: Model different execution modes.
- **Scheduling Optimization**: Optimize workload scheduling concerning compute and IO resource limitations and dependencies.
- **Release Time Constraints**: Consider when workloads become available for execution.
- **Dependencies**: Manage job dependencies to ensure that jobs are executed in parallel and in the correct order.
- **Preemption**: Allow jobs to be temporarily halted and resumed, enabling more flexible scheduling.
- **Reconfiguration at runtime**: Enable jobs to be reconfigured during execution to adapt to changing conditions.
- **Scalability**: Suitable for both small-scale and large-scale HPC systems.

## Simulation
For evaluation purposes, we use **exclusively** [Batsim](https://batsim.readthedocs.io/en/latest/) simulator.

## License
This project is licensed under the GNU General Public License v3.0. See the LICENSE file for details.
