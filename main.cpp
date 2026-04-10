#include <windows.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <limits>
#include <algorithm>
#include <set>
#include <map>
#include <queue>
#include <cmath>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
using namespace std;

// Estructura para aristas en Kruskal
struct Edge {
    int u, v;
    double weight;

    bool operator<(const Edge& other) const {
        return weight < other.weight;
    }
};

// Estructura Union-Find
class UnionFind {
public:
    vector<int> parent, rank;
    UnionFind(int n) {
        parent.resize(n);
        rank.resize(n, 0);
        for (int i = 0; i < n; ++i)
            parent[i] = i;
    }

    int find(int x) {
        if (parent[x] != x)
            parent[x] = find(parent[x]);
        return parent[x];
    }

    void unite(int x, int y) {
        int rootX = find(x);
        int rootY = find(y);
        if (rootX == rootY) return;

        if (rank[rootX] < rank[rootY])
            parent[rootX] = rootY;
        else if (rank[rootX] > rank[rootY])
            parent[rootY] = rootX;
        else {
            parent[rootY] = rootX;
            rank[rootX]++;
        }
    }
};

struct Point {
    double lon;
    double lat;
};

// Sobrecarga del operador == necesario para usar std::find
bool operator==(const Point& a, const Point& b) {
    return a.lon == b.lon && a.lat == b.lat;
}

// Datos globales del grafo
vector<vector<Point>> routes;
vector<Point> nodes;

double minLonGlobal = numeric_limits<double>::max();
double maxLonGlobal = -numeric_limits<double>::max();
double minLatGlobal = numeric_limits<double>::max();
double maxLatGlobal = -numeric_limits<double>::max();

// Zoom y arrastre
double zoomFactor = 1.0;
int offsetX = 0;
int offsetY = 0;
bool isDragging = false;
int startDragX = 0;
int startDragY = 0;

// Selección de algoritmo y nodos
int selectedAlgorithm = 1; // Por defecto: Mostrar Grafo Base
int selectedStartNode = -1;
int selectedEndNode = -1;
vector<int> dijkstraPath;
vector<vector<int>> bellmanFordPaths;
vector<vector<int>> floydWarshallPaths;
vector<vector<int>> kruskalEdges;
vector<pair<int, int>> bridges; // Lista de puentes (u, v)
vector<int> articulationPoints;       // Puntos de articulación
vector<vector<int>> connectedComponents; // Componentes conectadas
map<int, int> nodeColors;             // Coloración Greedy
vector<int> bfsPath;                  // Recorrido BFS
vector<int> dfsPath;                  // Recorrido DFS

HMENU hMenuPopup;

// Conversión geográfica a pantalla
void GeoToScreen(double lon, double lat, int* x, int* y, int width, int height) {
    double centerX = (minLonGlobal + maxLonGlobal) / 2.0;
    double centerY = (minLatGlobal + maxLatGlobal) / 2.0;

    double screenCenterX = width / 2.0;
    double screenCenterY = height / 2.0;

    double scaledLonRange = (maxLonGlobal - minLonGlobal) / zoomFactor;
    double scaledLatRange = (maxLatGlobal - minLatGlobal) / zoomFactor;

    *x = static_cast<int>(screenCenterX + ((lon - centerX) / scaledLonRange) * width) + offsetX;
    *y = static_cast<int>(screenCenterY + ((centerY - lat) / scaledLatRange) * height) + offsetY;
}

// Distancia entre dos puntos
double distance(const Point& a, const Point& b) {
    double dx = a.lon - b.lon;
    double dy = a.lat - b.lat;
    return sqrt(dx*dx + dy*dy);
}

// Construcción del grafo
map<int, vector<pair<int, double>>> graph;

void BuildGraph() {
    graph.clear();
    for (const auto& route : routes) {
        for (size_t i = 1; i < route.size(); ++i) {
            auto itA = find(nodes.begin(), nodes.end(), route[i - 1]);
            auto itB = find(nodes.begin(), nodes.end(), route[i]);
            if (itA != nodes.end() && itB != nodes.end()) {
                int idxA = itA - nodes.begin();
                int idxB = itB - nodes.begin();
                double dist = distance(route[i - 1], route[i]);
                graph[idxA].push_back({idxB, dist});
                graph[idxB].push_back({idxA, dist}); // Grafo no dirigido
            }
        }
    }
}

// Dijkstra para encontrar camino más corto
vector<int> Dijkstra(int start, int end) {
    priority_queue<pair<double, int>, vector<pair<double, int>>, greater<>> pq;
    map<int, double> dist;
    map<int, int> prev;
    set<int> visited;

    for (size_t i = 0; i < nodes.size(); ++i)
        dist[i] = numeric_limits<double>::infinity();

    dist[start] = 0;
    pq.push({0, start});

    while (!pq.empty()) {
        int u = pq.top().second;
        pq.pop();

        if (visited.count(u)) continue;
        visited.insert(u);

        if (u == end) break;

        for (auto [v, cost] : graph[u]) {
            if (dist[v] > dist[u] + cost) {
                dist[v] = dist[u] + cost;
                prev[v] = u;
                pq.push({dist[v], v});
            }
        }
    }

    vector<int> path;
    for (int at = end; at != start; at = prev[at]) {
        path.push_back(at);
    }
    path.push_back(start);
    reverse(path.begin(), path.end());
    return path;
}

// Heurística para A*: distancia en km usando fórmula del Haversine
double heuristic(int u, int v) {
    const double R = 6371; // Radio de la Tierra en km
    double lat1 = nodes[u].lat * M_PI / 180.0;
    double lon1 = nodes[u].lon * M_PI / 180.0;
    double lat2 = nodes[v].lat * M_PI / 180.0;
    double lon2 = nodes[v].lon * M_PI / 180.0;

    double dlon = lon2 - lon1;
    double dlat = lat2 - lat1;

    double a = sin(dlat/2)*sin(dlat/2) + cos(lat1)*cos(lat2)*sin(dlon/2)*sin(dlon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    return R * c; // Distancia en kilómetros
}

// A* para encontrar camino más corto
vector<int> AStar(int start, int end) {
    priority_queue<pair<double, int>, vector<pair<double, int>>, greater<>> pq;
    map<int, double> gScore;
    map<int, double> fScore;
    map<int, int> prev;

    for (size_t i = 0; i < nodes.size(); ++i)
        gScore[i] = numeric_limits<double>::infinity();

    gScore[start] = 0;
    fScore[start] = heuristic(start, end);
    pq.push({fScore[start], start});

    set<int> visited;

    while (!pq.empty()) {
        int u = pq.top().second;
        pq.pop();

        if (u == end) break;

        if (visited.count(u)) continue;
        visited.insert(u);

        for (auto [v, cost] : graph[u]) {
            if (gScore[u] + cost < gScore[v]) {
                prev[v] = u;
                gScore[v] = gScore[u] + cost;
                fScore[v] = gScore[v] + heuristic(v, end);
                pq.push({fScore[v], v});
            }
        }
    }

    vector<int> path;
    if (prev.find(end) != prev.end() || end == start) {
        int current = end;
        while (current != start) {
            path.push_back(current);
            current = prev[current];
        }
        path.push_back(start);
        reverse(path.begin(), path.end());
    }

    return path;
}

// Bellman-Ford: encuentra caminos mínimos desde un nodo origen a todos los demás
vector<vector<int>> BellmanFord(int start) {
    vector<vector<int>> paths(nodes.size());
    int n = nodes.size();
    vector<double> dist(n, numeric_limits<double>::infinity());
    vector<int> prev(n, -1);
    dist[start] = 0;

    // Relajamos todas las aristas n - 1 veces
    for (int i = 0; i < n - 1; ++i) {
        bool updated = false;
        for (auto& edge : graph) {
            int u = edge.first;
            for (auto& [v, cost] : edge.second) {
                if (dist[u] != numeric_limits<double>::infinity() && dist[v] > dist[u] + cost) {
                    dist[v] = dist[u] + cost;
                    prev[v] = u;
                    updated = true;
                }
            }
        }
        if (!updated) break;
    }

    // Reconstruir caminos
    for (int v = 0; v < n; ++v) {
        if (v == start || prev[v] == -1) continue;
        vector<int> path;
        int current = v;
        while (current != start && current != -1) {
            path.push_back(current);
            current = prev[current];
        }
        if (current == start) {
            path.push_back(start);
            reverse(path.begin(), path.end());
            paths[v] = path;
        }
    }

    return paths;
}

// Floyd-Warshall: caminos mínimos entre todos los pares de nodos
vector<vector<int>> FloydWarshall(int n) {
    vector<vector<double>> dist(n, vector<double>(n, numeric_limits<double>::infinity()));
    vector<vector<int>> next(n, vector<int>(n, -1));

    // Inicializar distancia directa entre nodos conectados
    for (int u = 0; u < n; ++u) {
        dist[u][u] = 0;
        for (auto [v, cost] : graph[u]) {
            dist[u][v] = cost;
            next[u][v] = v;
        }
    }

    // Algoritmo principal
    for (int k = 0; k < n; ++k) {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (dist[i][j] > dist[i][k] + dist[k][j]) {
                    dist[i][j] = dist[i][k] + dist[k][j];
                    next[i][j] = next[i][k];
                }
            }
        }
    }

    // Reconstruir caminos
    vector<vector<int>> paths;

    for (int u = 0; u < n; ++u) {
        for (int v = 0; v < n; ++v) {
            if (u == v || next[u][v] == -1) continue;

            vector<int> path = {u};
            int current = u;
            while (current != v) {
                current = next[current][v];
                path.push_back(current);
            }

            paths.push_back(path);
        }
    }

    return paths;
}

vector<vector<int>> Kruskal() {
    vector<Edge> edges;

    // Añadir todas las aristas al vector global
    for (auto [u, neighbors] : graph) {
        for (auto [v, cost] : neighbors) {
            if (u < v) { // Evitar duplicados
                edges.push_back({u, v, cost});
            }
        }
    }

    sort(edges.begin(), edges.end());

    UnionFind uf(nodes.size());
    vector<vector<int>> mstEdges;

    for (auto& e : edges) {
        if (uf.find(e.u) != uf.find(e.v)) {
            uf.unite(e.u, e.v);
            mstEdges.push_back({e.u, e.v});
        }
    }

    return mstEdges;
}

int timeDFS = 0;

void FindBridgesDFS(int u, int parent, const map<int, vector<pair<int, double>>>& graph,
                    vector<int>& disc, vector<int>& low, vector<bool>& visited,
                    vector<pair<int, int>>& bridgesFound) {
    static_cast<void>(visited[u] = true);
    disc[u] = low[u] = ++timeDFS;

    for (auto [v, cost] : graph.at(u)) {
        if (v == parent) continue;

        if (!visited[v]) {
            FindBridgesDFS(v, u, graph, disc, low, visited, bridgesFound);
            low[u] = min(low[u], low[v]);
            if (low[v] > disc[u]) {
                bridgesFound.push_back({u, v});
            }
        } else {
            low[u] = min(low[u], disc[v]);
        }
    }
}

// Devuelve lista de puentes (u, v)
vector<pair<int, int>> FindBridges() {
    int n = nodes.size();
    vector<int> disc(n), low(n);
    vector<bool> visited(n, false);
    vector<pair<int, int>> bridgesFound;

    for (int i = 0; i < n; ++i) {
        if (!visited[i]) {
            FindBridgesDFS(i, -1, graph, disc, low, visited, bridgesFound);
        }
    }

    return bridgesFound;
}


void FindArticulationPointsDFS(int u, int parent, vector<bool>& visited,
                               vector<int>& disc, vector<int>& low, vector<bool>& isArticulation,
                               const map<int, vector<pair<int, double>>>& graph) {
    visited[u] = true;
    disc[u] = low[u] = ++timeDFS;

    int children = 0;

    for (auto [v, cost] : graph.at(u)) {
        if (v == parent) continue;

        if (!visited[v]) {
            children++;
            FindArticulationPointsDFS(v, u, visited, disc, low, isArticulation, graph);

            low[u] = min(low[u], low[v]);

            if (parent == -1 && children > 1)
                isArticulation[u] = true;

            if (parent != -1 && low[v] >= disc[u])
                isArticulation[u] = true;

        } else {
            low[u] = min(low[u], disc[v]);
        }
    }
}

vector<int> FindArticulationPoints() {
    int n = nodes.size();
    vector<int> disc(n), low(n);
    vector<bool> visited(n, false), isArticulation(n, false);
    vector<int> points;

    for (int i = 0; i < n; ++i) {
        if (!visited[i])
            FindArticulationPointsDFS(i, -1, visited, disc, low, isArticulation, graph);
    }

    for (int i = 0; i < n; ++i)
        if (isArticulation[i])
            points.push_back(i);

    return points;
}

void DFS_Component(int u, vector<bool>& visited, vector<int>& component) {
    visited[u] = true;
    component.push_back(u);

    for (auto [v, cost] : graph[u]) {
        if (!visited[v])
            DFS_Component(v, visited, component);
    }
}

vector<vector<int>> FindConnectedComponents() {
    int n = nodes.size();
    vector<bool> visited(n, false);
    vector<vector<int>> components;

    for (int i = 0; i < n; ++i) {
        if (!visited[i]) {
            vector<int> component;
            DFS_Component(i, visited, component);
            components.push_back(component);
        }
    }

    return components;
}

map<int, int> GreedyGraphColoring() {
    map<int, int> colors;

    for (size_t u = 0; u < nodes.size(); ++u) {
        vector<bool> used(nodes.size(), false);

        for (auto [v, cost] : graph[static_cast<int>(u)]) {
            if (colors.find(v) != colors.end())
                used[colors[v]] = true;
        }

        for (int c = 0; c < static_cast<int>(nodes.size()); ++c) {
            if (!used[c]) {
                colors[static_cast<int>(u)] = c;
                break;
            }
        }
    }

    return colors;
}

vector<int> BFS(int start) {
    vector<int> path;
    vector<bool> visited(nodes.size(), false);
    queue<int> q;

    visited[start] = true;
    q.push(start);

    while (!q.empty()) {
        int u = q.front(); q.pop();
        path.push_back(u);

        for (auto [v, cost] : graph[u]) {
            if (!visited[v]) {
                visited[v] = true;
                q.push(v);
            }
        }
    }

    return path;
}

void DFS_Recursive(int u, vector<bool>& visited, vector<int>& path) {
    visited[u] = true;
    path.push_back(u);

    for (auto [v, cost] : graph[u]) {
        if (!visited[v])
            DFS_Recursive(v, visited, path);
    }
}

vector<int> DFS(int start) {
    vector<bool> visited(nodes.size(), false);
    vector<int> path;

    DFS_Recursive(start, visited, path);

    return path;
}


// Cargar rutas desde archivo CSV
void LoadRoutes(const string& filename) {
    ifstream file(filename);
    string line;
    set<pair<double, double>> uniquePoints;

    while (getline(file, line)) {
        size_t start = line.find("LINESTRING (");
        if (start == string::npos) continue;

        start += 12; // "LINESTRING (" tiene 12 caracteres
        size_t end = line.find(")", start);
        string coords = line.substr(start, end - start);

        istringstream iss(coords);
        vector<Point> route;
        char comma;

        while (!iss.eof()) {
            Point p;
            iss >> p.lon >> p.lat;
            if (!iss.fail()) {
                route.push_back(p);

                auto pr = make_pair(p.lon, p.lat);
                if (uniquePoints.insert(pr).second) {
                    nodes.push_back(p);
                }

                minLonGlobal = min(minLonGlobal, p.lon);
                maxLonGlobal = max(maxLonGlobal, p.lon);
                minLatGlobal = min(minLatGlobal, p.lat);
                maxLatGlobal = max(maxLatGlobal, p.lat);
                iss >> comma;
            }
        }

        if (!route.empty()) routes.push_back(route);
    }

    BuildGraph();
}

// WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    LoadRoutes("distritos.csv");

    const char CLASS_NAME[] = "MapWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "Mapa de Rutas con Dijkstra, A* y Bellman-Ford",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 0;

    // Menú desplegable
    hMenuPopup = CreatePopupMenu();
    AppendMenu(hMenuPopup, MF_STRING, 1, "Mostrar Grafo Base");
    AppendMenu(hMenuPopup, MF_STRING, 2, "Dijkstra (Origen -> Destino)");
    AppendMenu(hMenuPopup, MF_STRING, 3, "A* (Origen -> Destino)");
    AppendMenu(hMenuPopup, MF_STRING, 4, "Bellman-Ford (Desde Origen)");
    AppendMenu(hMenuPopup, MF_STRING, 5, "Floyd-Warshall (Todos los Pares)");
    AppendMenu(hMenuPopup, MF_STRING, 6, "Kruskal (MST)");
    AppendMenu(hMenuPopup, MF_STRING, 7, "Floyd-Warshall (Todos los Pares)");
    AppendMenu(hMenuPopup, MF_STRING, 8, "Encontrar Puntos de Articulación");
    AppendMenu(hMenuPopup, MF_STRING, 9, "Componentes Conectadas");
    AppendMenu(hMenuPopup, MF_STRING, 10, "Coloración de Grafo (Greedy)");
    AppendMenu(hMenuPopup, MF_STRING, 11, "Recorrido BFS (Desde Origen)");
    AppendMenu(hMenuPopup, MF_STRING, 12, "Recorrido DFS (Desde Origen)");
    AppendMenu(hMenuPopup, MF_STRING, 13, "Limpiar Selecciones/Caminos");
    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

// Procedimiento de ventana
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_MOUSEWHEEL: {
            short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (zDelta > 0) zoomFactor *= 1.1;
            else if (zDelta < 0) zoomFactor /= 1.1;
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt;
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);

            RECT rc;
            GetClientRect(hwnd, &rc);
            int width = rc.right - rc.left;
            int height = rc.bottom - rc.top;

            int closestIndex = -1;
            int minDistSq = numeric_limits<int>::max();
            int radius = 6;

            for (size_t i = 0; i < nodes.size(); ++i) {
                int x, y;
                GeoToScreen(nodes[i].lon, nodes[i].lat, &x, &y, width, height);
                int dx = x - pt.x;
                int dy = y - pt.y;
                int distSq = dx*dx + dy*dy;

                if (distSq < radius*radius && distSq < minDistSq) {
                    minDistSq = distSq;
                    closestIndex = i;
                }
            }

            if (closestIndex != -1) {
                if (selectedAlgorithm == 2 || selectedAlgorithm == 3) {
                    if (selectedStartNode == -1) {
                        selectedStartNode = closestIndex;
                    } else if (selectedEndNode == -1 && closestIndex != selectedStartNode) {
                        selectedEndNode = closestIndex;
                        if (selectedAlgorithm == 2)
                            dijkstraPath = Dijkstra(selectedStartNode, selectedEndNode);
                        else if (selectedAlgorithm == 3)
                            dijkstraPath = AStar(selectedStartNode, selectedEndNode);
                    }
                } else if (selectedAlgorithm == 4) {
                    selectedStartNode = closestIndex;
                    bellmanFordPaths = BellmanFord(selectedStartNode);
                    selectedEndNode = -1;
                }
                else if (selectedAlgorithm == 5) {
                    floydWarshallPaths = FloydWarshall(nodes.size());
                    selectedStartNode = -1;
                    selectedEndNode = -1;
                }
                else if (selectedAlgorithm == 6) {
                kruskalEdges = Kruskal(); // No requiere nodo inicial
                selectedStartNode = -1;
                selectedEndNode = -1;
                }
                else if (selectedAlgorithm == 7) {
                bridges = FindBridges(); // Encuentra puentes automáticamente
                selectedStartNode = -1;
                selectedEndNode = -1;
                }
                else if (selectedAlgorithm == 8) {
                    articulationPoints = FindArticulationPoints();
                    selectedStartNode = -1;
                    selectedEndNode = -1;
                }
                else if (selectedAlgorithm == 9) {
                    connectedComponents = FindConnectedComponents();
                    selectedStartNode = -1;
                    selectedEndNode = -1;
                }
                else if (selectedAlgorithm == 10) {
                    nodeColors = GreedyGraphColoring();
                    selectedStartNode = -1;
                    selectedEndNode = -1;
                }
                else if (selectedAlgorithm == 11 || selectedAlgorithm == 12) {
                    if (closestIndex != -1) {
                        selectedStartNode = closestIndex;
                        if (selectedAlgorithm == 11)
                            bfsPath = BFS(selectedStartNode);
                        else if (selectedAlgorithm == 12)
                            dfsPath = DFS(selectedStartNode);
                    }
                }
                else if (selectedAlgorithm == 13) {
                    selectedStartNode = -1;
                    selectedEndNode = -1;
                    dijkstraPath.clear();
                    bellmanFordPaths.clear();
                    floydWarshallPaths.clear();
                    kruskalEdges.clear();
                    bridges.clear();
                    articulationPoints.clear();
                    connectedComponents.clear();
                    nodeColors.clear();
                    bfsPath.clear();
                    dfsPath.clear();
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }

            if (selectedAlgorithm != 2 && selectedAlgorithm != 3 && selectedAlgorithm != 4 && selectedAlgorithm != 5) {
                isDragging = true;
                startDragX = LOWORD(lParam);
                startDragY = HIWORD(lParam);
                SetCapture(hwnd);
            }

            return 0;
        }

        case WM_MOUSEMOVE: {
            if (isDragging) {
                int currentX = LOWORD(lParam);
                int currentY = HIWORD(lParam);
                offsetX += currentX - startDragX;
                offsetY += currentY - startDragY;
                startDragX = currentX;
                startDragY = currentY;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            isDragging = false;
            ReleaseCapture();
            return 0;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) >= 1 && LOWORD(wParam) <= 13) {
                selectedAlgorithm = LOWORD(wParam);
                selectedStartNode = -1;
                selectedEndNode = -1;
                dijkstraPath.clear();
                bellmanFordPaths.clear();
                floydWarshallPaths.clear();
                kruskalEdges.clear();
                bridges.clear();
                articulationPoints.clear();
                connectedComponents.clear();
                nodeColors.clear();
                bfsPath.clear();
                dfsPath.clear();
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_CONTEXTMENU: {
            POINT pt;
            GetCursorPos(&pt);
            TrackPopupMenu(hMenuPopup, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);
            int width = rc.right - rc.left;
            int height = rc.bottom - rc.top;

            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 255));
            SelectObject(hdc, hPen);

            for (const auto& route : routes) {
                for (size_t i = 1; i < route.size(); ++i) {
                    int x1, y1, x2, y2;
                    GeoToScreen(route[i - 1].lon, route[i - 1].lat, &x1, &y1, width, height);
                    GeoToScreen(route[i].lon, route[i].lat, &x2, &y2, width, height);
                    MoveToEx(hdc, x1, y1, NULL);
                    LineTo(hdc, x2, y2);
                }
            }

            HBRUSH hNodeBrush = CreateSolidBrush(RGB(0, 0, 255));
            SelectObject(hdc, hNodeBrush);

            int nodeRadius = 2;
            for (size_t i = 0; i < nodes.size(); ++i) {
                int x, y;
                GeoToScreen(nodes[i].lon, nodes[i].lat, &x, &y, width, height);
                Ellipse(hdc, x - nodeRadius, y - nodeRadius, x + nodeRadius, y + nodeRadius);
            }

            // Nodo inicio (verde)
            if (selectedStartNode != -1) {
                int x, y;
                GeoToScreen(nodes[selectedStartNode].lon, nodes[selectedStartNode].lat, &x, &y, width, height);
                HBRUSH hStartBrush = CreateSolidBrush(RGB(0, 255, 0));
                SelectObject(hdc, hStartBrush);
                Ellipse(hdc, x - 5, y - 5, x + 5, y + 5);
                DeleteObject(hStartBrush);
            }

            // Camino de Dijkstra o A*
            if ((selectedAlgorithm == 2 || selectedAlgorithm == 3) && !dijkstraPath.empty()) {
                HPEN hAlgoPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0)); // Rojo
                SelectObject(hdc, hAlgoPen);
                for (size_t i = 1; i < dijkstraPath.size(); ++i) {
                    int x1, y1, x2, y2;
                    GeoToScreen(nodes[dijkstraPath[i - 1]].lon, nodes[dijkstraPath[i - 1]].lat, &x1, &y1, width, height);
                    GeoToScreen(nodes[dijkstraPath[i]].lon, nodes[dijkstraPath[i]].lat, &x2, &y2, width, height);
                    MoveToEx(hdc, x1, y1, NULL);
                    LineTo(hdc, x2, y2);
                }
                DeleteObject(hAlgoPen);
            }

            // Camino de Bellman-Ford (desde un nodo a todos)
            if (selectedAlgorithm == 4 && !bellmanFordPaths.empty()) {
                HPEN hBFSPen = CreatePen(PS_SOLID, 2, RGB(255, 165, 0)); // Naranja
                SelectObject(hdc, hBFSPen);
                for (const auto& path : bellmanFordPaths) {
                    if (path.size() < 2) continue;
                    for (size_t i = 1; i < path.size(); ++i) {
                        int x1, y1, x2, y2;
                        GeoToScreen(nodes[path[i - 1]].lon, nodes[path[i - 1]].lat, &x1, &y1, width, height);
                        GeoToScreen(nodes[path[i]].lon, nodes[path[i]].lat, &x2, &y2, width, height);
                        MoveToEx(hdc, x1, y1, NULL);
                        LineTo(hdc, x2, y2);
                    }
                }
                DeleteObject(hBFSPen);
            }
            // Camino de Floyd-Warshall (todos los pares)
            if (selectedAlgorithm == 5 && !floydWarshallPaths.empty()) {
                HPEN hFWPen = CreatePen(PS_SOLID, 2, RGB(128, 0, 128)); // Morado
                SelectObject(hdc, hFWPen);

                for (const auto& path : floydWarshallPaths) {
                    if (path.size() < 2) continue;
                    for (size_t i = 1; i < path.size(); ++i) {
                        int x1, y1, x2, y2;
                        GeoToScreen(nodes[path[i - 1]].lon, nodes[path[i - 1]].lat, &x1, &y1, width, height);
                        GeoToScreen(nodes[path[i]].lon, nodes[path[i]].lat, &x2, &y2, width, height);
                        MoveToEx(hdc, x1, y1, NULL);
                        LineTo(hdc, x2, y2);
                    }
                }

                DeleteObject(hFWPen);
            }
            // Camino de Kruskal (MST)
            if (selectedAlgorithm == 6 && !kruskalEdges.empty()) {
                HPEN hKruskalPen = CreatePen(PS_SOLID, 2, RGB(139, 69, 19)); // Marrón
                SelectObject(hdc, hKruskalPen);

                for (const auto& edge : kruskalEdges) {
                    int u = edge[0];
                    int v = edge[1];

                    int x1, y1, x2, y2;
                    GeoToScreen(nodes[u].lon, nodes[u].lat, &x1, &y1, width, height);
                    GeoToScreen(nodes[v].lon, nodes[v].lat, &x2, &y2, width, height);

                    MoveToEx(hdc, x1, y1, NULL);
                    LineTo(hdc, x2, y2);
                }

                DeleteObject(hKruskalPen);
            }
            // Dibujar puentes
            if (selectedAlgorithm == 7 && !bridges.empty()) {
                HPEN hBridgePen = CreatePen(PS_SOLID, 2, RGB(255, 105, 180)); // Rosado
                SelectObject(hdc, hBridgePen);

                for (const auto& [u, v] : bridges) {
                    int x1, y1, x2, y2;
                    GeoToScreen(nodes[u].lon, nodes[u].lat, &x1, &y1, width, height);
                    GeoToScreen(nodes[v].lon, nodes[v].lat, &x2, &y2, width, height);
                    MoveToEx(hdc, x1, y1, NULL);
                    LineTo(hdc, x2, y2);
                }

                DeleteObject(hBridgePen);
            }
            // Puntos de articulación
            if (selectedAlgorithm == 8 && !articulationPoints.empty()) {
                HBRUSH hAPBrush = CreateSolidBrush(RGB(139, 0, 0)); // Rojo oscuro
                SelectObject(hdc, hAPBrush);

                for (int idx : articulationPoints) {
                    int x, y;
                    GeoToScreen(nodes[idx].lon, nodes[idx].lat, &x, &y, width, height);
                    Ellipse(hdc, x - 6, y - 6, x + 6, y + 6);
                }

                DeleteObject(hAPBrush);
            }
            // Componentes conectadas
            if (selectedAlgorithm == 9 && !connectedComponents.empty()) {
                vector<COLORREF> colors = {RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255),
                                           RGB(255, 255, 0), RGB(255, 0, 255), RGB(0, 255, 255)};

                HPEN hPenComp = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
                SelectObject(hdc, hPenComp);

                for (size_t compIdx = 0; compIdx < connectedComponents.size(); ++compIdx) {
                    COLORREF color = colors[compIdx % colors.size()];
                    HBRUSH hBrush = CreateSolidBrush(color);
                    SelectObject(hdc, hBrush);

                    for (int idx : connectedComponents[compIdx]) {
                        int x, y;
                        GeoToScreen(nodes[idx].lon, nodes[idx].lat, &x, &y, width, height);
                        Ellipse(hdc, x - 6, y - 6, x + 6, y + 6);
                    }

                    DeleteObject(hBrush);
                }

                DeleteObject(hPenComp);
            }
            // Coloración Greedy
            if (selectedAlgorithm == 10 && !nodeColors.empty()) {
                vector<COLORREF> palette = {
                    RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255),
                    RGB(255, 255, 0), RGB(255, 0, 255), RGB(0, 255, 255)
                };

                for (const auto& [idx, colorId] : nodeColors) {
                    int x, y;
                    GeoToScreen(nodes[idx].lon, nodes[idx].lat, &x, &y, width, height);
                    HBRUSH hBrush = CreateSolidBrush(palette[colorId % palette.size()]);
                    SelectObject(hdc, hBrush);
                    Ellipse(hdc, x - 6, y - 6, x + 6, y + 6);
                    DeleteObject(hBrush);
                }
            }
            // Recorrido BFS
            if (selectedAlgorithm == 11 && !bfsPath.empty()) {
                HPEN hBFSPen = CreatePen(PS_SOLID, 2, RGB(0, 128, 0)); // Verde oscuro
                SelectObject(hdc, hBFSPen);

                for (size_t i = 1; i < bfsPath.size(); ++i) {
                    int x1, y1, x2, y2;
                    GeoToScreen(nodes[bfsPath[i - 1]].lon, nodes[bfsPath[i - 1]].lat, &x1, &y1, width, height);
                    GeoToScreen(nodes[bfsPath[i]].lon, nodes[bfsPath[i]].lat, &x2, &y2, width, height);
                    MoveToEx(hdc, x1, y1, NULL);
                    LineTo(hdc, x2, y2);
                }

                DeleteObject(hBFSPen);
            }

            // Recorrido DFS
            if (selectedAlgorithm == 12 && !dfsPath.empty()) {
                HPEN hDFSPen = CreatePen(PS_SOLID, 2, RGB(128, 0, 0)); // Azul oscuro
                SelectObject(hdc, hDFSPen);

                for (size_t i = 1; i < dfsPath.size(); ++i) {
                    int x1, y1, x2, y2;
                    GeoToScreen(nodes[dfsPath[i - 1]].lon, nodes[dfsPath[i - 1]].lat, &x1, &y1, width, height);
                    GeoToScreen(nodes[dfsPath[i]].lon, nodes[dfsPath[i]].lat, &x2, &y2, width, height);
                    MoveToEx(hdc, x1, y1, NULL);
                    LineTo(hdc, x2, y2);
                }

                DeleteObject(hDFSPen);
            }

            DeleteObject(hNodeBrush);
            DeleteObject(hPen);

            EndPaint(hwnd, &ps);
            return 0;
        }

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}
