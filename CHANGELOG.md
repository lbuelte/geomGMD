# Changelog

Changes compared to the tag [`sigspatial26`](https://github.com/lbuelte/geomGMD/tree/sigspatial26), the code that produced the results of the paper. The commit history has one commit per change; commits that change behaviour start with `Fix:` or `Change:`.

## 2.0.0 (2026-10-29)

Corrections that make the computation follow the definitions of the paper.

**geomGMD values change** compared to `sigspatial26` and 1.0.0. Most changed distances become smaller. For 408 pairs of real building footprints, 42 distances changed by more than Gurobi's relative MIP gap of 10⁻⁴; all of them became smaller, by up to 19 %. The distances of the example graphs in `examples/tiny` do not change. To reproduce the values of the paper, use the tag `sigspatial26`, or `v1.0.0` for the cleaned-up version of the same code.

### Fixed

- **Path angles** are measured anti-clockwise in [0, 2π), as defined in the paper. Before, they were measured in (−π, π]. With the old range, a path that bends slightly to the left and one that bends slightly to the right got angles near π and −π. Mapping one onto the other then cost almost 2π instead of almost 0. This affects `angle_type = "signed"` (the default); `"unsigned"` is unchanged.
- **geomGMD is minimized over all constrained edge maps**, as defined in the paper. Before, the BLP contained the constraints z<sub>ij,kl</sub> ≥ x<sub>ik</sub> + x<sub>jl</sub> − 1. These forced every edge whose end nodes are mapped onto the end nodes of an edge to be mapped as well, which restricted the minimum to induced edge maps (paper, Remark 5.6). With path mapping costs, deleting such an edge and inserting its counterpart can be cheaper, so some distances were too high. Also, the triangle inequality was not guaranteed for the old values.
- **Time limit without a solution:** if Gurobi found no edge map within the time limit, the upper bound was 10¹⁰⁰ and the edge map was empty. Now the upper bound is the cost of the trivial edge map, which deletes all edges of *G* and inserts all edges of *H*. That edge map is returned.

### Added

- Tests for the path angles and for a pair whose optimal edge map is not induced.

## 1.0.0 (2026-10-26)

Cleaned-up and documented version of the `sigspatial26` code.

**geomGMD values are unchanged**: with the same parameters, the computed distances (lower and upper bounds) are identical to those of `sigspatial26`. This was checked for 408 pairs of real building footprints at full precision, and the tests check it for the example graphs. Edge maps may differ when several edge maps are optimal.

### Changed

- **Defaults are the settings of the paper**: `cost_factor_angle_difference = 1.0` (was 2.0) and `timelimit_sec = 300` (was 60). Configs that set all parameters explicitly, like the `config_dist.toml.example` of `sigspatial26`, give the same results as before.
- Executables renamed: `SingleDistance` → `geomGMD_pair`, `modGEDComputations` → `geomGMD_matrix`, `PlotNodemaps` → `geomGMD_plot_edgemaps`, `PolygonClustering` → `geomGMD_cluster`. The default config file of the clustering is `config_clustering.toml` (was `config.toml`).
- `geomGMD_pair` writes its output to the subfolder `pair/` of the run folder.
- The config is parsed strictly: unknown sections or keys, wrong types and invalid values are errors (before, they were ignored or crashed later). Sections of the other programs give a warning.
- Results of an earlier run in the same folder are no longer overwritten: `geomGMD_matrix` and `geomGMD_cluster` stop with an error, unless `[IO] overwrite = true` or `--overwrite` is given.
- Output files:
  - Edge maps instead of node maps: `edgemaps.json` (was `nodemaps.json`, config key `write_edgemaps`, was `write_nodemaps`); the SVG plots show the edge maps.
  - Edge maps and `computation_info.csv` list the two graphs in the order of the call (before, the larger graph came first).
  - All outputs use the graph ids (positions in the input file); before, `computation_info.csv` and the SVG file names used the polygon ids.
  - `intervaldistance_matrix.csv` and `computation_info.csv`: distances with 6 decimals (was 3).
  - The reported lower bound is at least 0 (Gurobi's bound can be slightly negative).
  - "Optimal" means optimal within Gurobi's relative MIP gap, now configurable as `[Gurobi] mip_gap` (default 10⁻⁴, as before).
- Clustering:
  - PAM is the default algorithm (was CLARA); PAM is the algorithm of the paper.
  - CLARA and CLARANS write a line to `computation_info.csv` for every computed pair.
- On infeasible models, no IIS is computed and no `infeasible.ilp` is written.
- The SVG legends say "Nodes" (was "Vertices").
- One progress bar, only shown if the output is a terminal.

### Added

- Log levels for the console and the log file (`[Logging] level`, `file_level`, `--log-level`, `--file-log-level`); log lines in `run.log` have a time stamp.
- Gurobi's own log can be written to `run.log` (`[Logging] gurobi_log`, `--gurobi-log`), optionally only for some pairs (`gurobi_log_pairs`, `gurobi_log_min_gap`).
- `computation_info.csv`: Gurobi status, final MIP gap and number of branch-and-bound nodes per pair.
- `[Gurobi] threads` and `seed` (defaults 1 and 1, as before).
- Clustering:
  - `clustering.csv` and `clustering.json` with the cluster and medoid of every graph.
  - Distances of a `geomGMD_matrix` run can be reused (`[Clustering] distance_matrix`, `compute_missing_distances`); the run stops if its model parameters differ.
  - `[Clustering] failed_distances = "abort" | "infinite" | "exclude"` decides what happens when the distance of a pair cannot be computed.
- Library: `compute_geomGMD` (`include/distance/geomGMD.h`) as the single entry point, `compute_mapping_cost` for the cost of a given edge map, `DistanceMatrix` for many pairs in parallel. The code is split into the CMake libraries `geomGMD_utility`, `geomGMD_distance`, `geomGMD_io` and `geomGMD_clustering`; the clustering (and with it GDAL) is optional (`-DGEOMGMD_BUILD_CLUSTERING=OFF`).
- Tests (CTest), an example dataset (`examples/tiny`), documented example configs, README, LICENSE (MIT), CITATION.cff.

### Fixed

- `geomGMD_matrix` ignored `starting_instance`.
- Pairs whose distance could not be computed were written as `-1,-1,true` (i.e. optimal); now `nan,nan,false`, and `geomGMD_matrix` exits with an error code.
- `geomGMD_cluster` wrote the cluster ids to the wrong polygons of the GeoPackage if `starting_instance > 0`.
- Clustering:
  - A failed distance (`-1`) counted as the smallest distance; now the clustering stops by default (see `failed_distances`).
  - `k` larger than the number of graphs crashed (also CLARA with a sample smaller than `k`); now an error.
  - CLARA and CLARANS crashed without a `[Clara]` / `[Clarans]` section in the config.
  - CLARA crashed if no sample could be clustered.
  - CLARA wrote a matrix of `nan` if the sample contained all graphs.
  - Data race in the parallel distance computation of CLARANS.
  - Error messages of PAM named internal indices instead of graph ids.
  - Wrong thread count in the CLARANS log.
- A model that becomes infeasible because of a cutoff (CLARA's assignment) was reported as an error instead of a cutoff.
- `write_distance_matrix` and `write_edgemaps` did not work in all executables.
- The SVG of a graph contained a stray text node after every edge.
- Polygons in the GeoPackage without a graph in the input file made `geomGMD_cluster` abort with an unclear message; now they are left out with a warning.
- Without a GeoPackage, `geomGMD_cluster` printed an error only on the console, after the log file was closed; now it writes a warning to the console and to `run.log` (the GeoPackage is optional). Messages of the GeoPackage output are also written to `run.log`.
- `IntervalDistance::operator==` was never true.

### Removed

- The "vanishing angle" clustering modes (`[Clustering] mode`, `starting_va`, `va_diff`, `max_va_depth`, `fixed_k`, `max_iterations`, `single_run_va`). They had no effect, because the input contains no vanishing angles.
- `[Clara] reuse_distances` (distances are always reused now) and the unused `[IO] run_parallel`.
- Other unused code.
