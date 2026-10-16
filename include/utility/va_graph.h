//
// Created by naumann on 11.12.25.
//

#ifndef GEOMGMD_VA_GRAPH_H
#define GEOMGMD_VA_GRAPH_H

#include "utility/geometric_graph.h"

// lightweight wrapper class to represent a graph with vanishing angles, making it simple to reduce a boost graph
// based on vanishing angle cutoff
class VAGraph {
public:
    //constructor
    VAGraph(GeometricGraph original_graph) : original_graph(original_graph) {}

    //function to filter graph based on the vanishing angle
    GeometricGraph& filter(double vanishing_angle);

private:
    GeometricGraph original_graph;

    //store last filtered graph as temporary memory in case it gets requested multiple times in a row
    std::optional<GeometricGraph> filtered_graph;
    double va_of_stored_filtered_graph = -1.0;


};

#endif //GEOMGMD_VA_GRAPH_H
