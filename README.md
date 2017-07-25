# HSRNet

## `mperf` packet

*: Current version(1.0.0039 Beta) is a __pre-release__ version, which means until next __release__ version: 

- The files now existing will NOT be removed(but may be modified anyway, like this file).
- The calling interface will NOT be modified.
- New files may be added to the packet(like Copyright files of the libraries we used for implementing).
- __You may not distribute this packet since copyright information can be incomplete!__

`mperf` is a tool that enables you to use TCP protocol to send or receive some amount of data or just keep sending as much data as possible for some time. `mperf` is specially designed for _tough_ networks with high RTT, high loss rate, high out-of-order delay, etc, it works robustly and never gives unexpected outputs under those circumstances. We designed this tool in order to provide a way to measure the performance of those networks, we hope to do our measurements in an all-non-interactive and robust way and `mperf` is our solution.
