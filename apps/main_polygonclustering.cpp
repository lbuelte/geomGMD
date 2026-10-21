#include <iostream>
#include <fstream>
#include <chrono>
#include <numeric>
#include <filesystem>

#include "clustering/gpkg_io.h"
#include "io/command_line.h"
#include "clustering/plots_and_stats.h"
#include "io/config.h"
#include "io/graph_io.h"
#include "io/output_context.h"
#include "io/run_setup.h"

#include "clustering/clustering_base.h"
#include "clustering/clustering_output.h"
#include "utility/log.h"


namespace logger = geomgmd::log;

namespace {

int run(int argc, char* argv[]) {   

    auto const command_line = parse_command_line(argc, argv);
    std::string config_path = "config_clustering.toml";
    if (command_line.positional.size() >= 1) {
        config_path = command_line.positional[0];
    }
    auto config = load_config(config_path);

    std::filesystem::path output_path = run_output_path(config.io);
    auto out = OutputContext(output_path, config.io);

    std::filesystem::path log_file = log_file_path(config.io, output_path);

    config.logging = resolve_logging(config.logging, command_line);
    configure_logging(config.logging, log_file);
    for (auto const &warning : config.warnings) logger::warning(warning);
    logger::info("Writing log file to ", log_file, "\n");
    logger::info(config);

    // Log config in extra file
    copy_config(config_path, output_path);

    // Reading input graphs
    std::filesystem::path json_path = graphs_json_path(config.io);

    std::vector<GeometricGraph> geometricgraphs;
    read_graph_vector_from_json(geometricgraphs, json_path.string());

    auto const [starting_instance, num_instances] = select_instances(config.io, geometricgraphs.size());
    logger::debug("Reduced dataset: ", num_instances, " graphs starting at index ", starting_instance);

    std::vector<const GeometricGraph*> graphs;
    for (GraphID i = starting_instance; i < starting_instance + num_instances; i++){
        geometricgraphs[i].set_graph_id(i);
        graphs.push_back(&geometricgraphs[i]);
    }

    auto begin = std::chrono::high_resolution_clock::now();
    ClusteringResult clustering_result = 
        run_clustering(graphs, config.cluster.k, config, out);
    if (clustering_result._distance_matrix_opt.has_value()){
        out.write_interval_distance_matrix(clustering_result._distance_matrix_opt.value());                 
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
    logger::info("Overall Clustering took ", (double)dur.count() / 1000.0, " [s].");
    
    if (config.io.generate_plots){
        // Write graphs and medoids color-coded by clusters to svg
        std::filesystem::path plot_path = output_path / "plots_final_clustering";
        std::filesystem::create_directories(plot_path);
        delete_svgs(plot_path);

        for (ClusterID c = 0; c < clustering_result.clusters.size(); c++){
            std::filesystem::path medoid_path =
                    plot_path / ("medoid_" + std::to_string(c) + ".svg");
            write_instance_and_cluster_svg(*graphs[clustering_result.medoid_indices[c]], c, medoid_path);
            for(const GraphID &g : clustering_result.clusters[c]) {
                auto const &graph = *graphs[g];
                std::filesystem::path graph_path =
                        plot_path / ("graph_" + std::to_string(graph.polygon_id()) + ".svg");
                write_instance_and_cluster_svg(graph, c, graph_path);
            }
        }
    }

    // Logging final clustering
    logger::info(clustering_result_to_string(clustering_result, std::string("Final clustering"), starting_instance));
    
    logger::close_file();

    // Result files (only the clustered graphs, with graph id and polygon id)
    write_clustering_csv(output_path / "clustering.csv", clustering_result, graphs);
    write_clustering_json(output_path / "clustering.json",
                          clustering_to_json(clustering_result, graphs, config.io.dataset, config.io.run_name,
                                             clusteringAlgoToStr(config.cluster.clustering_algo), config.cluster.k));

    // write geopackage with assigned cluster ids and information about medoids
    // (cluster members and medoids are local indices into graphs; the vectors are indexed by graph id)
    std::vector<int> cluster_ids(geometricgraphs.size(), -1), is_medoid(geometricgraphs.size(), -1);
    for (ClusterID c = 0; c < clustering_result.clusters.size(); c++){
        for (const GraphID &g : clustering_result.clusters[c]) {
            cluster_ids[graphs[g]->graph_id()] = c;
        }
    }
    //mark medoid polygons
    for (int cluster_id = 0; cluster_id < clustering_result.medoid_indices.size(); cluster_id++) {
        is_medoid[graphs[clustering_result.medoid_indices[cluster_id]]->graph_id()] = cluster_id;
    }
    std::filesystem::path input_gpkg =
            std::filesystem::path(config.io.input_path)
            / config.io.dataset
            / (config.io.dataset + ".gpkg");
    std::filesystem::path output_gpkg =
            output_path
            / (config.io.dataset + ".gpkg");

    std::unordered_map<PolygonID, GraphID> polygon_id_to_graph_id;
    for (auto const &graph : geometricgraphs){
        polygon_id_to_graph_id.emplace(graph.polygon_id(), graph.graph_id());
    }

    assignClustersToPolygons(input_gpkg.string(), output_gpkg.string(), polygon_id_to_graph_id, cluster_ids, is_medoid);


    return EXIT_SUCCESS;
}

} // end of anonymous namespace


int main(int argc, char* argv[])
{
    return run_main(run, argc, argv);
}
