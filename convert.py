# convert_distrital_to_segments.py
import json
import sys

def round_coords(val):
    return round(val, 6)

def extract_segments_from_ring(ring):
    segments = []
    for i in range(len(ring) - 1):
        pt1, pt2 = ring[i], ring[i + 1]
        if len(pt1) >= 2 and len(pt2) >= 2:
            # Redondear para evitar nodos duplicados en fronteras compartidas
            lon1, lat1 = round_coords(pt1[0]), round_coords(pt1[1])
            lon2, lat2 = round_coords(pt2[0]), round_coords(pt2[1])
            segments.append(f"LINESTRING ({lon1} {lat1}, {lon2} {lat2})")
    return segments

def extract_from_feature(feature):
    geom = feature.get("geometry")
    if not geom:
        return []
    
    geom_type = geom["type"]
    coords = geom["coordinates"]
    all_segments = []
    
    if geom_type == "Polygon":
        all_segments.extend(extract_segments_from_ring(coords[0]))
    elif geom_type == "MultiPolygon":
        for polygon in coords:
            all_segments.extend(extract_segments_from_ring(polygon[0]))
            
    return all_segments

def convert(geojson_path, csv_path):
    with open(geojson_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    all_segments = []
    for feature in data.get("features", []):
        all_segments.extend(extract_from_feature(feature))
    
    with open(csv_path, 'w', encoding='utf-8') as f:
        for segment in all_segments:
            f.write(segment + '\n')
    
    print(f"✅ Listo: {len(all_segments)} segmentos distritales → {csv_path}")
    print(f"📌 Se redondearon coordenadas a 6 decimales para garantizar conexiones en el grafo.")

if __name__ == "__main__":
    if len(sys.argv) == 3:
        convert(sys.argv[1], sys.argv[2])
    else:
        print("Uso: python convert_distrital_to_segments.py entrada.geojson salida.csv")