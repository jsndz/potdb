# PTDB Benchmark Evolution History

This living document tracks empirical benchmark changes across feature implementations and refactors.

| Date & Time | Feature / Milestone | Write (500 pts) | Write (50k pts) | Read (pts/s) | Read Bandwidth | 2D Accessor |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 2026-10-30 17:48:13 | **baseline_v1** | 92257 (-) | 3029428 (-) | 9782451 (-) | 149.68 MB/s (-) | 26.88 ns (-) |
| 2026-11-02 20:09:51 | **feat_durability_modes** | 107716 (**+16.76%** 🚀) | 4019535 (**+32.68%** 🚀) | 16927942 (**+73.04%** 🚀) | 259.01 MB/s (**+73.04%** 🚀) | 10.67 ns (**-60.31%** 🚀) |
| 2026-11-03 12:12:49 | **mode_sync_always** | 96668 (-10.26%) | 3049760 (-24.13%) | 9105305 (-46.21%) | 139.32 MB/s (-46.21%) | 25.31 ns (+137.21%) |
| 2026-11-03 12:13:27 | **mode_sync_periodic** | 813048 (**+741.07%** 🚀) | 4746433 (**+55.63%** 🚀) | 9626675 (**+5.73%** 🚀) | 147.30 MB/s (**+5.73%** 🚀) | 24.43 ns (**-3.48%** 🚀) |
| 2026-11-03 12:13:39 | **mode_sync_none** | 4365202 (**+436.89%** 🚀) | 5403534 (**+13.84%** 🚀) | 9291438 (-3.48%) | 142.17 MB/s (-3.48%) | 25.29 ns (+3.52%) |
