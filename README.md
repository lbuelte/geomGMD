# geomGMD

**Translation- and Rotation-Invariant Geometric Graph Mapping Distance for Building Footprint Similarity**

geomGMD is a similarity measure for geometric graphs: graphs whose nodes are points in the plane and whose edges are straight line segments. In the paper below, it is used to compare building footprints through their medial axis graphs, but any connected, simple geometric graphs can be compared. geomGMD is invariant under translation and rotation.

This repository contains the C++ implementation of the paper:

> Laura Bülte, Alexander Naumann, Johanna Schlurmann, Jan-Henrik Haunert, and Petra Mutzel. 2026. **geomGMD: Translation- and Rotation-Invariant Geometric Graph Mapping Distance for Building Footprint Similarity.** In *SIGSPATIAL '26*, Riverside, CA, USA. https://doi.org/10.1145/3841645.3844193

The medial axis graphs of polygons are computed with the separate repository [polygon-medial-axis-graph](https://github.com/lbuelte/polygon-medial-axis-graph), see [Preparing your own data](#preparing-your-own-data).

**Reproducing the paper:** the code used for the results in the paper is the tag [`sigspatial26`](https://github.com/lbuelte/geomGMD/tree/sigspatial26), see [Paper version](#paper-version). The `main` branch is a cleaned-up and documented version of that code that computes the same distances; [CHANGELOG.md](CHANGELOG.md) lists all changes.

## What geomGMD computes

geomGMD compares two geometric graphs *G* and *H* by mapping edges of *G* to edges of *H*:

- A **constrained edge map** is a bijection between a subset of the edges of *G* and a subset of the edges of *H* that preserves adjacency: two distinct mapped edges are adjacent in *G* if and only if their images are adjacent in *H*. Edges of *G* that are not mapped are deleted, edges of *H* that are not mapped are inserted.
- Its **cost** consists of
  - *c*<sub>id</sub> · *l*(*e*) for every deleted or inserted edge *e* of length *l*(*e*),
  - *c*<sub>m</sub> · |*l*(*e*) − *l*(*ψ*(*e*))| for every mapped edge *e* (edge mapping cost),
  - *c*<sub>r</sub> · |*α*(*q*) − *α*(*ρ*(*q*))| for every path *q* of two mapped edges, where *α* is the angle enclosed at the middle node and *ρ*(*q*) is the image of the path (path mapping cost).
- **geomGMD** is the minimum cost over all constrained edge maps. It is a metric for *c*<sub>m</sub> = *c*<sub>id</sub>.

Computing geomGMD is NP-hard. It is computed exactly by a binary linear program (BLP), solved with Gurobi. Besides the distance, the computed edge map is returned. If the time limit is reached, the result is an interval: the best lower bound and the cost of the best edge map found.

## Requirements

| Dependency | Version | Notes |
|---|---|---|
| C++ compiler | C++20 | tested with GCC 11.4 |
| CMake | ≥ 3.16 | tested with 3.22 |
| [Gurobi Optimizer](https://www.gurobi.com/) | ≥ 12.0 | paper results: 13.0; tested with 12.0.3. Needs a licence, free for academic use, see below |
| [GDAL](https://gdal.org/) | ≥ 3.0 | only for the clustering (`geomGMD_cluster`); tested with 3.8.4 |

[nlohmann/json](https://github.com/nlohmann/json) 3.12.0 and [toml++](https://github.com/marzer/tomlplusplus) 3.4.0 are included in `external/`.

On Ubuntu/Debian, everything except Gurobi can be installed with

```bash
sudo apt install build-essential cmake libgdal-dev
```

### Gurobi and its licence

1. [Create a free Gurobi account](https://portal.gurobi.com/iam/login/).
2. [Request a licence](https://portal.gurobi.com/iam/licenses). Academic licences are free; for a single user, the Named-User Academic licence fits best. See also the [Getting Started guide](https://support.gurobi.com/hc/en-us/articles/14799677517585-Getting-Started-with-Gurobi-Optimizer).
3. [Download the Gurobi Optimizer](https://www.gurobi.com/product/download-center/optimizer-software) and [install it](https://support.gurobi.com/hc/en-us/articles/4534161999889-How-do-I-install-Gurobi-Optimizer).
4. Set the environment variable `GUROBI_HOME` (or `GUROBI_DIR`) to the installation directory, e.g. in your `.bashrc`:
   ```bash
   export GUROBI_HOME=/opt/gurobi1300/linux64
   ```
   If the licence file `gurobi.lic` is not in one of Gurobi's default locations (e.g. your home directory), set `GRB_LICENSE_FILE` to its path.

## Build

From the repository root:

```bash
cmake -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j 8
```

This builds the executables `geomGMD_pair`, `geomGMD_matrix`, `geomGMD_plot_edgemaps` and `geomGMD_cluster` and the tests in `build/release`. Options:

| Option | Default | Meaning |
|---|---|---|
| `-DGEOMGMD_BUILD_CLUSTERING=OFF` | `ON` | build without the clustering (`geomGMD_cluster`), then GDAL is not needed |
| `-DGEOMGMD_BUILD_TESTS=OFF` | `ON` | do not build the tests |

Run the tests with

```bash
ctest --test-dir build/release --output-on-failure
```

The tests that compute geomGMD need a Gurobi licence. Without one, they are reported as skipped. One of them checks that the distances of the example graphs are those of the paper version.

## Quick start

The folder [`examples/tiny`](examples/tiny) contains four small hand-made graphs: the medial axis graphs of a 10 m × 6 m rectangle (graph 0), of the same rectangle rotated and moved (graph 1), of an L-shaped polygon (graph 2) and of an 8 m × 8 m square (graph 3). From the repository root:

```bash
# geomGMD between graphs 0 and 2
./build/release/geomGMD_pair 0 2 examples/config_tiny.toml

# all pairs (distance matrix) and SVG plots of the computed edge maps
./build/release/geomGMD_matrix examples/config_tiny.toml
./build/release/geomGMD_plot_edgemaps examples/config_tiny.toml
```

The results are written to `output/tiny/tiny/`. `intervaldistance_matrix.csv` contains:

```
row,col,lb,ub,optimal
0,1,0.000000,0.000000,true
0,2,18.798447,18.798447,true
0,3,21.769010,21.769010,true
1,2,18.798447,18.798447,true
1,3,21.769010,21.769010,true
2,3,25.626874,25.626874,true
```

Graphs 0 and 1 have distance 0, because geomGMD does not depend on position and orientation.

## Using geomGMD as a library

The core is the function `compute_geomGMD` in [`include/distance/geomGMD.h`](include/distance/geomGMD.h) (library target `geomGMD_distance`). Graphs can be read from a file (`read_graph_vector_from_json` in [`include/io/graph_io.h`](include/io/graph_io.h), target `geomGMD_io`) or built in code:

```cpp
#include "distance/geomGMD.h"

#include <cmath>
#include <iostream>
#include <vector>

// A geometric graph from node coordinates and edges (pairs of node indices); edge length = Euclidean length
GeometricGraph make_graph(std::vector<Point> const &points, std::vector<std::pair<NodeID, NodeID>> const &edges)
{
    std::vector<GeometricGraph::Node> nodes;
    for (NodeID i = 0; i < points.size(); ++i) nodes.push_back({i, points[i]});
    std::vector<GeometricGraph::Edge> graph_edges;
    for (auto const &[u, v] : edges) {
        graph_edges.push_back({u, v, std::hypot(points[v].x - points[u].x, points[v].y - points[u].y)});
    }
    return GeometricGraph(std::move(nodes), std::move(graph_edges));
}

int main()
{
    // Medial axis graphs of a 10 x 6 rectangle and of an 8 x 8 square
    GeometricGraph const G = make_graph({{3, 3}, {7, 3}, {0, 0}, {0, 6}, {10, 0}, {10, 6}},
                                        {{0, 1}, {0, 2}, {0, 3}, {1, 4}, {1, 5}});
    GeometricGraph const H = make_graph({{4, 4}, {0, 0}, {8, 0}, {8, 8}, {0, 8}},
                                        {{0, 1}, {0, 2}, {0, 3}, {0, 4}});

    ModelParameters const model;     // c_id = c_m = c_r = 1 (the settings of the paper)
    SolverSettings const settings;   // time limit 300 s, 1 thread
    GeomGMDResult const r = compute_geomGMD(G, H, model, settings);
    if (not r.distance) {
        std::cerr << distance_result_error_message(r.result) << "\n";
        return 1;
    }
    std::cout << "geomGMD = " << r.distance->ub << " (lower bound " << r.distance->lb << ")\n";
    for (auto const &pair : r.edge_map.edge_pairs) {
        if (pair.is_substitution()) std::cout << "  edge " << *pair.g << " -> edge " << *pair.h << "\n";
    }
}
```

This prints `geomGMD = 21.769 (lower bound 21.769)` and the mapped edges. In your own CMake project:

```cmake
set(GEOMGMD_BUILD_CLUSTERING OFF CACHE BOOL "" FORCE)
set(GEOMGMD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(geomGMD)
target_link_libraries(my_program PRIVATE geomGMD_distance)
```

The cost of a given edge map can be computed with `compute_mapping_cost` ([`include/distance/mapping_cost.h`](include/distance/mapping_cost.h)). Distances of many pairs, computed in parallel and on demand, are provided by `DistanceMatrix` ([`include/io/distance_matrix.h`](include/io/distance_matrix.h)).

## Preparing your own data

geomGMD reads geometric graphs from a JSON file (see [Input format](#input-format)). To compare polygons, e.g. building footprints, compute their medial axis graphs with [polygon-medial-axis-graph](https://github.com/lbuelte/polygon-medial-axis-graph). It reads polygons from GeoPackage files and writes the graphs in the input format of geomGMD:

```bash
# in polygon-medial-axis-graph (see its README for building it; it needs CGAL)
./build/medial_axis_graph --input <polygon folder> --output <graph folder>
```

For each GeoPackage `<polygon folder>/<dataset>/<dataset>.gpkg`, this writes `<graph folder>/<dataset>/<dataset>_graphs.json`. Then set in the geomGMD config

```toml
[IO]
dataset = "<dataset>"
input_path = "<graph folder>"
output_path = "<result folder>"
```

For the clustering, also copy the GeoPackage to `<graph folder>/<dataset>/<dataset>.gpkg`: `geomGMD_cluster` then writes a copy of it with the assigned clusters. Without it, the clustering is still computed and written to `clustering.csv` and `clustering.json`.

The paper used the version of polygon-medial-axis-graph with the tag [`sigspatial26`](https://github.com/lbuelte/polygon-medial-axis-graph/tree/sigspatial26).

## Input format

The input of a dataset `<dataset>` is the file `<input_path>/<dataset>/<dataset>_graphs.json` with an array `graphs`. Each graph has an entry `medial_axis` with two arrays:

- `vertices`: the nodes of the graph, each given by its coordinates `x` and `y`,
- `edges`: the edges, each given by the indices `source` and `target` of its end nodes in `vertices`.

```json
{
  "graphs": [
    {
      "id": 1,
      "medial_axis": {
        "vertices": [ {"x": 3, "y": 3}, {"x": 7, "y": 3}, {"x": 0, "y": 0},
                      {"x": 0, "y": 6}, {"x": 10, "y": 0}, {"x": 10, "y": 6} ],
        "edges": [ {"source": 0, "target": 1}, {"source": 0, "target": 2}, {"source": 0, "target": 3},
                   {"source": 1, "target": 4}, {"source": 1, "target": 5} ]
      }
    }
  ]
}
```

The graphs must be connected and simple; edge lengths are the Euclidean lengths. In all outputs, graphs are referred to by their position in the array (**graph id**, starting at 0). The optional entry `id` (**polygon id**) links a graph to its polygon in the GeoPackage and appears in the clustering results. Other entries (e.g. `polygon`, written by polygon-medial-axis-graph) are ignored.

## Configuration

All executables read their parameters from a TOML file, given as a command line argument. If it is omitted, `config_dist.toml` (or `config_clustering.toml` for `geomGMD_cluster`) in the current working directory is used. Start from [`config_dist.toml.example`](config_dist.toml.example) (distance computations) or [`config_clustering.toml.example`](config_clustering.toml.example) (clustering), or from [`examples/config_tiny.toml`](examples/config_tiny.toml).

Required are the `[IO]` keys `dataset`, `input_path` and `output_path`. All other parameters have defaults; the defaults of the model parameters and the time limit are the settings of the paper. The most important parameters:

| Section | Key | Default | Meaning |
|---|---|---|---|
| `[Gurobi]` | `cost_factor_deletion_insertion` | 1.0 | *c*<sub>id</sub> |
| | `cost_factor_length_difference` | 1.0 | *c*<sub>m</sub> |
| | `cost_factor_angle_difference` | 1.0 | *c*<sub>r</sub> (angles in radians) |
| | `timelimit_sec` | 300 | Gurobi time limit per pair |
| | `threads` | 1 | Gurobi threads per pair |
| | `mip_gap` | 1e-4 | relative MIP gap at which a solution counts as optimal (Gurobi's default) |
| `[IO]` | `run_name` | `default_run` (clustering: e.g. `PAM_10`) | results are written to `<output_path>/<dataset>/<run_name>/` |
| | `starting_instance`, `num_instances` | all graphs | use only the graphs `starting_instance`, ..., `starting_instance + num_instances − 1` |
| | `overwrite` | false | replace the results of an earlier run in the same folder (also `--overwrite`) |
| `[Parallel]` | `num_threads` | 4 | number of pairs computed in parallel by `geomGMD_matrix` |
| `[Logging]` | `level`, `file_level` | `info`, `debug` | log levels of console and `run.log` (also `--log-level`, `--file-log-level`) |
| | `gurobi_log` | false | write Gurobi's own log to `run.log` (also `--gurobi-log`) |

All parameters, their defaults and meanings are listed in [`include/io/config.h`](include/io/config.h) and [`include/distance/parameters.h`](include/distance/parameters.h). The config is checked strictly: unknown sections or keys (e.g. typos), wrong types and invalid values are errors. Every run copies its config to `config_input.toml` in the output folder.

## Executables and output

All results of a run are written to `<output_path>/<dataset>/<run_name>/`, together with `config_input.toml` and the log file `run.log`. To protect earlier results, `geomGMD_matrix` and `geomGMD_cluster` stop if the folder already contains results, unless `overwrite` is set.

### `geomGMD_pair <g> <h> [config]`

Computes geomGMD between the graphs with graph ids `g` and `h`, prints the result and the cost of the edge map to the console and writes an SVG of the edge map to the subfolder `pair/`.

### `geomGMD_matrix [config]`

Computes geomGMD for all pairs of graphs, in parallel. It writes

- `intervaldistance_matrix.csv`: per pair the graph ids (`row`, `col`), a lower and an upper bound (`lb`, `ub`, 6 decimals) and whether the solution is `optimal`. "Optimal" means optimal within Gurobi's relative MIP gap (`mip_gap`): then `lb = ub` is the cost of the computed edge map, which is at most 0.01 % above the true minimum. If the time limit is reached, `lb < ub` and `optimal` is `false`. Pairs that could not be computed are written as `nan,nan,false`.
- `computation_info.csv`: per pair the graph ids `G`, `H`, the graph sizes, the bounds, the running time, the Gurobi status, the final MIP gap and the number of branch-and-bound nodes,
- `edgemaps.json`: the computed edge maps.

### `geomGMD_plot_edgemaps [config]`

Reads the results of a `geomGMD_matrix` run (same `dataset`, `output_path` and `run_name`) and writes an SVG of each edge map to the subfolder `edgemaps/`.

### `geomGMD_cluster [config]`

Clusters the graphs with PAM (k-medoids, as in the paper; default), CLARA or CLARANS (`[Clustering] clustering_algo`, see [`config_clustering.toml.example`](config_clustering.toml.example)). It writes `clustering.csv` and `clustering.json` (cluster and medoid of every graph), plots of the clusters, the distances and edge maps as `geomGMD_matrix` does, and, if `<input_path>/<dataset>/<dataset>.gpkg` exists, a copy of that GeoPackage with the assigned clusters.

- Distances computed earlier by `geomGMD_matrix` can be reused: `[Clustering] distance_matrix = "<run folder>/intervaldistance_matrix.csv"`. Pairs missing there are computed, or the run stops if `compute_missing_distances = false`. The run also stops if the model parameters in the `config_input.toml` of that run differ from those of the clustering.
- If the distance of a pair cannot be computed (solver error), `[Clustering] failed_distances` decides: `"abort"` (default) stops the clustering, `"infinite"` treats the distance as infinite, and `"exclude"` leaves out as few graphs as possible so that no failed pair remains (they are listed in the results without a cluster).

## Paper version

The tag [`sigspatial26`](https://github.com/lbuelte/geomGMD/tree/sigspatial26) is the code that produced the results of the paper. It is published unmodified, apart from removing machine-specific files. The paper used c<sub>id</sub> = c<sub>m</sub> = c<sub>r</sub> = 1 (lengths in metres, angles in radians), a Gurobi time limit of 300 s per pair and Gurobi 13.0; the distance matrices were computed with 96 parallel threads.

To use the tagged version, note:

- Its executables have other names: `SingleDistance` (now `geomGMD_pair`), `modGEDComputations` (`geomGMD_matrix`), `PlotNodemaps` (`geomGMD_plot_edgemaps`) and `PolygonClustering` (`geomGMD_cluster`).
- Its built-in defaults differ from the paper settings (e.g. `cost_factor_angle_difference = 2.0`, `timelimit_sec = 60`), so set all parameters in the config file, as in its `config_dist.toml.example`.

The current version computes the same distances as the tagged version; the tests check this for the example graphs. The changes, e.g. in the output formats, are listed in [CHANGELOG.md](CHANGELOG.md).

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

The code in this repository is licensed under the [MIT License](LICENSE).

Third-party components:

- Gurobi is proprietary software with its own licence; it is not included in this repository.
- GDAL: MIT.
- nlohmann/json and toml++: MIT (licence files in `external/`).

## Use of AI

The code used for the paper (tag `sigspatial26`) was written by the authors without AI assistance. The preparation of this repository for publication was done with the help of the AI model Claude Opus 5.5 (Anthropic), using Claude Code: restructuring and commenting the code, the configuration and command line interface, the bug fixes listed in [CHANGELOG.md](CHANGELOG.md) and in the commit history, the tests and example data, and the documentation. All changes were directed and reviewed by the authors, and the computed distances were checked against those of the paper's code.
