# geomGMD

**Translation- and Rotation-Invariant Geometric Graph Mapping Distance for Building Footprint Similarity**

This repository contains the C++ implementation associated with the paper:

> **geomGMD: Translation- and Rotation-Invariant Geometric Graph Mapping Distance for Building Footprint Similarity**

## Status

This version of the code (tag `sigspatial26`) is the code that was used to produce the results in the paper. It is published unmodified, apart from removing machine-specific files.
A cleaned-up and documented version will follow on the `main` branch.

geomGMD compares geometric graphs. In the paper, these are the medial axis graphs of building footprints, generated with [polygon-medial-axis-graph](https://github.com/lbuelte/polygon-medial-axis-graph).

## Dependencies

* C++20 compiler and CMake (≥ 3.5)
* [Gurobi Optimizer](https://www.gurobi.com/) (the paper results were computed with Gurobi 13.0)
* [GDAL](https://gdal.org/) (e.g. `libgdal-dev` on Debian/Ubuntu)
* [nlohmann/json](https://github.com/nlohmann/json) and [toml++](https://github.com/marzer/tomlplusplus) are included in `external/`.

## Setting up Gurobi

If you already have Gurobi installed, you can skip this step.

* You first need to [create a free Gurobi account](https://portal.gurobi.com/iam/login/).
* Then you can [request a license](https://portal.gurobi.com/iam/licenses). Academic licenses are free; most fitting for a student is the Named-User Academic license.
* You can also just follow these [support guidelines](https://support.gurobi.com/hc/en-us/articles/14799677517585-Getting-Started-with-Gurobi-Optimizer).
* Download the [Gurobi Optimizer](https://www.gurobi.com/product/download-center/optimizer-software).
* Follow these [install guidelines](https://support.gurobi.com/hc/en-us/articles/4534161999889-How-do-I-install-Gurobi-Optimizer).
* On Linux, the environment variable `GUROBI_HOME` (or `GUROBI_DIR`) has to point to the Gurobi installation directory, e.g. `/opt/gurobi1300/linux64` (set it e.g. in your `.bashrc`).

## Compiling with CMake

From the project source directory (compiling with 4 threads):

```
cmake -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j 4
```

For a debug build, use `-DCMAKE_BUILD_TYPE=Debug` and e.g. `build/debug`.

This creates the executables `geomGMD_pair`, `geomGMD_matrix`, `geomGMD_plot_edgemaps` and `geomGMD_cluster` in the build folder.

## Input format

The input is a file `<input_path>/<dataset>/<dataset>_graphs.json`.
It contains a JSON array `graphs`. Each entry has an entry `medial_axis` with two arrays `vertices` and `edges`:
* `vertices` is an array of 2D points, each given by the coordinates `x` and `y` (doubles).
* `edges` is an array of index pairs, each given by `source` and `target`.

Example:
```
graphs :
    0 :
        medial_axis :
            edges :
                0  { source: 5, target: 0 }
                1  { source: 3, target: 1 }
                2  { source: 5, target: 2 }
                3  { source: 4, target: 3 }
                4  { source: 5, target: 3 }
            vertices :
                0  { x: 763.007, y: 104.66 }
                1  { x: 769.714, y: 108.474 }
                2  { x: 760.334, y: 109.08 }
                3  { x: 766.1675716491843, y: 109.47072828728383 }
                4  { x: 767.157, y: 113.091 }
                5  { x: 763.92229977309, y: 108.17246238713469 }
    1 :
        medial_axis : ...
```

Graphs are referred to by their index in this array.

## Configuration

All executables read their parameters from a TOML file, given as an optional command line argument.
If it is omitted, `config_dist.toml` (resp. `config_clustering.toml` for `geomGMD_cluster`) in the **current working directory** is used.

Start from `config_dist.toml.example` (distance computations) or `config_clustering.toml.example` (clustering) and set at least
* `dataset`: name of the dataset folder (and file prefix), see [Input format](#input-format),
* `input_path`: directory containing the dataset folder,
* `output_path`: directory for the results. Output is written to `<output_path>/<dataset>/<run_name>/`.

The settings used in the paper are those in `config_dist.toml.example`: `cost_factor_deletion_insertion = cost_factor_length_difference = cost_factor_angle_difference = 1.0` (lengths in meters, angles in radians) and `timelimit_sec = 300`.
All parameters and their defaults are defined in `include/io/config.h`.

## Running the code

### Single geomGMD computation

```
./build/release/geomGMD_pair <g> <h> [config_dist.toml]
```

Computes geomGMD between the graphs with indices `g` and `h`, prints the result to the console and writes an SVG of the edge map to `<output_path>/<dataset>/<run_name>/pair/`.

### Distance matrix

```
./build/release/geomGMD_matrix [config_dist.toml]
```

Computes geomGMD for all pairs of graphs in the dataset (or the subset given by `starting_instance` and `num_instances`), in parallel with `num_threads` threads. The output folder contains:
* `intervaldistance_matrix.csv`: for each pair, a lower and an upper bound (`lb`, `ub`, 6 decimals) and whether the solution is `optimal` (`lb = ub`). If the Gurobi time limit is reached, the bounds may differ. "Optimal" means optimal within Gurobi's relative MIP gap (`[Gurobi] mip_gap`, default 10⁻⁴): then `lb = ub` is the value of the computed edge map, which is at most 0.01 % above the true minimum. Pairs that could not be computed are written as `nan,nan,false`.
* `computation_info.csv`: graph sizes, bounds, running time, Gurobi status, final MIP gap and number of branch-and-bound nodes per pair,
* `edgemaps.json`: the computed edge maps,
* `config_input.toml` (copy of the config) and `run.log`.

### Plotting edge maps

```
./build/release/geomGMD_plot_edgemaps [config_dist.toml]
```

Reads the output of a `geomGMD_matrix` run (same `dataset`, `output_path` and `run_name`) and writes an SVG of each mapping to the `edgemaps/` subfolder.

### Clustering

```
./build/release/geomGMD_cluster [config_clustering.toml]
```

Clusters the graphs of a dataset using geomGMD (PAM, CLARA or CLARANS; see `config_clustering.toml.example`). It additionally expects the polygons as `<input_path>/<dataset>/<dataset>.gpkg` and writes a copy with the assigned cluster IDs.

## Citation

If you use this code or geomGMD in your research, please cite:

> Laura Bülte, Alexander Naumann, Johanna Schlurmann, Jan-Henrik Haunert, and Petra Mutzel. 2026. geomGMD: Translation- and Rotation-Invariant Geometric Graph Mapping Distance for Building Footprint Similarity. In *The 34th ACM International Conference on Advances in Geographic Information Systems (SIGSPATIAL '26)*, November 03–06, 2026, Riverside, CA, USA. ACM, New York, NY, USA, 13 pages. https://doi.org/10.1145/3841645.3844193

```bibtex
@inproceedings{buelte2026geomgmd,
  author    = {B{\"u}lte, Laura and Naumann, Alexander and Schlurmann, Johanna and Haunert, Jan-Henrik and Mutzel, Petra},
  title     = {{geomGMD}: Translation- and Rotation-Invariant Geometric Graph Mapping Distance for Building Footprint Similarity},
  booktitle = {The 34th ACM International Conference on Advances in Geographic Information Systems (SIGSPATIAL '26)},
  year      = {2026},
  month     = nov,
  address   = {Riverside, CA, USA},
  publisher = {ACM},
  numpages  = {13},
  doi       = {10.1145/3841645.3844193}
}
```

## License

The license information will be added with the public code release.
