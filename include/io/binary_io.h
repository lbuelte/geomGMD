#ifndef POLYGON_CLUSTERING_BINARY_IO_H
#define POLYGON_CLUSTERING_BINARY_IO_H


// #include <fstream>
// #include <iostream>
// #include <vector>
// #include "va_graph.h"

// void writeGraphVectorToBin(const std::vector<Graph_boost> &graphVector, const std::string &filepath) {
//     std::ofstream outFile(filepath, std::ios::binary);

//     if (outFile.is_open()) {
//         // Write the number of graphs in the vector
//         size_t numGraphs = graphVector.size();
//         outFile.write(reinterpret_cast<const char *>(&numGraphs), sizeof(numGraphs));

//         // Serialize each graph in the vector
//         for (const auto &g: graphVector) {
//             // Write the number of vertices
//             size_t numVertices = num_vertices(g);
//             outFile.write(reinterpret_cast<const char *>(&numVertices), sizeof(numVertices));

//             // Write each vertex's properties
//             graph_traits<Graph_boost>::vertex_iterator vi, vi_end;
//             for (boost::tie(vi, vi_end) = vertices(g); vi != vi_end; ++vi) {
//                 const VertexProps_boost &vp = g[*vi];

//                 // Write the 2D coordinates (x, y)
//                 long double x = vp.p.a;
//                 long double y = vp.p.b;
//                 outFile.write(reinterpret_cast<const char *>(&x), sizeof(x));
//                 outFile.write(reinterpret_cast<const char *>(&y), sizeof(y));

//                 // Write the weight (double)
//                 double minRadius = vp.minRadius;
//                 double maxRadius = vp.maxRadius;
//                 outFile.write(reinterpret_cast<const char *>(&minRadius), sizeof(minRadius));
//                 outFile.write(reinterpret_cast<const char *>(&maxRadius), sizeof(maxRadius));
//             }

//             // Write the number of edges
//             size_t numEdges = num_edges(g);
//             outFile.write(reinterpret_cast<const char *>(&numEdges), sizeof(numEdges));

//             // Write each edge (source and target vertices)
//             graph_traits<Graph_boost>::edge_iterator ei, ei_end;
//             for (boost::tie(ei, ei_end) = edges(g); ei != ei_end; ++ei) {
//                 Vertex_boost src = source(*ei, g);
//                 Vertex_boost tgt = target(*ei, g);
//                 outFile.write(reinterpret_cast<const char *>(&src), sizeof(src));
//                 outFile.write(reinterpret_cast<const char *>(&tgt), sizeof(tgt));

//                 // Write the edge properties
//                 const VAEdgeProps_boost &ep = g[*ei];
//                 outFile.write(reinterpret_cast<const char *>(&ep.weight), sizeof(ep.weight));
//                 outFile.write(reinterpret_cast<const char *>(&ep.label), sizeof(ep.label));
//                 outFile.write(reinterpret_cast<const char *>(&ep.vanishingAngle), sizeof(ep.vanishingAngle));
//             }
//         }

//         outFile.close();
//         std::cout << "Vector of Boost graphs has been written to the binary file." << std::endl;
//     } else {
//         std::cerr << "Could not open the file for writing." << std::endl;
//     }
// }

// void readGraphVectorFromBin(std::vector<VAGraph> &graphVector, const std::string &filePath) {
//     std::ifstream inFile(filePath, std::ios::binary);

//     if (inFile.is_open()) {

//         // Read the number of graphs
//         size_t numGraphs;
//         inFile.read(reinterpret_cast<char *>(&numGraphs), sizeof(numGraphs));

//         // Deserialize each graph
//         for (size_t i = 0; i < numGraphs; ++i) {
//             Graph_boost g;

//             // Read the number of vertices
//             size_t numVertices;
//             inFile.read(reinterpret_cast<char *>(&numVertices), sizeof(numVertices));

//             // Read and add vertices with properties
//             for (size_t j = 0; j < numVertices; ++j) {
//                 // Read the 2D coordinates
//                 long double x, y;
//                 inFile.read(reinterpret_cast<char *>(&x), sizeof(x));
//                 inFile.read(reinterpret_cast<char *>(&y), sizeof(y));

//                 //read the radius
//                 double minRadius, maxRadius;
//                 inFile.read(reinterpret_cast<char *>(&minRadius), sizeof(minRadius));
//                 inFile.read(reinterpret_cast<char *>(&maxRadius), sizeof(maxRadius));

//                 // Add the vertex to the graph with the read properties
//                 boost::add_vertex({minRadius, maxRadius, {x, y}}, g);
//             }

//             // Read the number of edges
//             size_t numEdges;
//             inFile.read(reinterpret_cast<char *>(&numEdges), sizeof(numEdges));

//             // Read and add edges with properties
//             for (size_t j = 0; j < numEdges; ++j) {
//                 Vertex_boost src, tgt;

//                 // Read the source and target vertices
//                 inFile.read(reinterpret_cast<char *>(&src), sizeof(src));
//                 inFile.read(reinterpret_cast<char *>(&tgt), sizeof(tgt));

//                 // Read the edge properties
//                 double weight;
//                 int label;
//                 double vanishingAngle;
//                 inFile.read(reinterpret_cast<char *>(&weight), sizeof(weight));
//                 inFile.read(reinterpret_cast<char *>(&label), sizeof(label));
//                 inFile.read(reinterpret_cast<char *>(&vanishingAngle), sizeof(vanishingAngle));

//                 // Add the edge to the graph with the read properties
//                 add_edge(src, tgt, {weight, label, vanishingAngle}, g);
//             }

//             graphVector.emplace_back(Graph(g, i)); // Add the graph to the vector
//         }

//         inFile.close();

//     } else {
//         std::cerr << "Could not open the file for reading." << std::endl;
//     }

// }

#endif //POLYGON_CLUSTERING_BINARY_IO_H
