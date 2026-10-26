# Generates tiny_graphs.json: python3 make_tiny_graphs.py tiny_graphs.json
import json, sys

def graph(gid, outer, nodes, edges):
    return {"id": gid,
            "polygon": {"outer": [{"x": x, "y": y} for x, y in outer], "holes": []},
            "medial_axis": {"vertices": [{"x": x, "y": y} for x, y in nodes],
                            "edges": [{"source": s, "target": t} for s, t in edges]}}

# Rotation with cos = 4/5, sin = 3/5 (about 36.87 degrees): the coordinates stay exact decimals
def transform(points, dx, dy):
    c, s = 0.8, 0.6
    return [(round(c * x - s * y + dx, 6), round(s * x + c * y + dy, 6)) for x, y in points]

# Rectangle 10 m x 6 m
rect_outer = [(0, 0), (10, 0), (10, 6), (0, 6)]
rect_nodes = [(3, 3), (7, 3), (0, 0), (0, 6), (10, 0), (10, 6)]
rect_edges = [(0, 1), (0, 2), (0, 3), (1, 4), (1, 5)]

# L-shape
l_outer = [(0, 0), (12, 0), (12, 4), (4, 4), (4, 10), (0, 10)]
l_nodes = [(2, 2), (10, 2), (2, 8), (0, 0), (12, 0), (12, 4), (0, 10), (4, 10)]
l_edges = [(0, 3), (0, 1), (0, 2), (1, 4), (1, 5), (2, 6), (2, 7)]

# Square 8 m x 8 m
sq_outer = [(0, 0), (8, 0), (8, 8), (0, 8)]
sq_nodes = [(4, 4), (0, 0), (8, 0), (8, 8), (0, 8)]
sq_edges = [(0, 1), (0, 2), (0, 3), (0, 4)]

graphs = [
    graph(1, rect_outer, rect_nodes, rect_edges),
    # the same rectangle, rotated by about 36.87 degrees and translated: geomGMD 0 to graph 0
    graph(2, transform(rect_outer, 100, 50), transform(rect_nodes, 100, 50), rect_edges),
    graph(3, l_outer, l_nodes, l_edges),
    graph(4, sq_outer, sq_nodes, sq_edges),
]
json.dump({"graphs": graphs}, open(sys.argv[1], "w"), indent=2)
