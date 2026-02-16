#!/bin/bash
# Tile OBJ to C converter for RetroRacer (Dreamcast)
# Converts Kenney road tile OBJs to embedded C source with mesh data.
# Follows the same pattern as obj2c.sh.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OBJ_DIR="$PROJECT_ROOT/assets/tiles/obj"
HEADER="$PROJECT_ROOT/include/tile_data.h"
SOURCE="$PROJECT_ROOT/src/tile_data.c"

# XZ scale: Kenney tiles are 1 unit, track is 12 units wide
SCALE_XZ=12.0
SCALE_Y=1.0

echo "RetroRacer Tile OBJ-to-C Converter"

# Tile definitions: enum_name:filename:display_name
TILES=(
    "STRAIGHT:road-asphalt-straight.obj:Road Straight"
    "CORNER:road-asphalt-corner.obj:Road Corner"
    "DAMAGED:road-asphalt-damaged.obj:Road Damaged"
    "SIDE:road-asphalt-side.obj:Road Side"
    "PAVEMENT:road-asphalt-pavement.obj:Pavement"
    "GRASS:grass.obj:Grass"
)

# Material-to-color mapping (ARGB packed):
# asphalt    -> dark gray  0xFF404040
# concreteSmooth -> light gray 0xFF808080
# grass      -> green      0xFF228B22
# default    -> medium gray 0xFF606060

# Generate a single tile's C arrays using awk
# Only processes the FIRST group (Kenney OBJs have duplicate groups)
generate_tile() {
    local enum_name="$1"
    local obj_file="$2"
    local display_name="$3"
    local var_name=$(echo "$enum_name" | tr '[:upper:]' '[:lower:]')

    awk -v scale_xz="$SCALE_XZ" -v scale_y="$SCALE_Y" \
        -v var_name="$var_name" -v display_name="$display_name" '
    BEGIN {
        vc = 0; fc = 0;
        group_count = 0;
        current_material = "default";
        # Material count for face-to-material mapping
    }
    /^g / {
        group_count++;
    }
    /^v / && group_count <= 1 {
        vc++;
        # Kenney OBJ has trailing RGB: v x y z r g b
        vx[vc] = $2;
        vy[vc] = $3;
        vz[vc] = $4;
    }
    /^usemtl / && group_count <= 1 {
        current_material = $2;
    }
    /^f / && group_count <= 1 {
        fc++;
        # Parse v/vt/vn format
        split($2, a, "/"); fi0[fc] = a[1];
        split($3, b, "/"); fi1[fc] = b[1];
        split($4, c, "/"); fi2[fc] = c[1];
        face_mtl[fc] = current_material;
    }
    END {
        # Rotate 90 degrees around Y then scale:
        # Kenney tiles have road along X, but track system needs road along Z.
        # Rotation: (x,y,z) -> (-z, y, x)  (rotate_y(-90))
        # This puts road direction along +Z and curbs along +-X.
        for (i = 1; i <= vc; i++) {
            ox = vx[i];
            oz = vz[i];
            vx[i] = -oz * scale_xz;
            vy[i] = vy[i] * scale_y;
            vz[i] = ox * scale_xz;
        }

        printf "/* %s: %d vertices, %d triangles */\n", display_name, vc, fc;

        # Vertex array
        printf "static const float %s_verts[] = {\n", var_name;
        for (i = 1; i <= vc; i++) {
            comma = (i < vc) ? "," : "";
            printf "    %10.6ff, %10.6ff, %10.6ff%s\n", vx[i], vy[i], vz[i], comma;
        }
        printf "};\n\n";

        # Face index array (0-indexed)
        printf "static const int %s_faces[] = {\n", var_name;
        for (i = 1; i <= fc; i++) {
            comma = (i < fc) ? "," : "";
            printf "    %d, %d, %d%s\n", fi0[i]-1, fi1[i]-1, fi2[i]-1, comma;
        }
        printf "};\n\n";

        # Face normals (computed from cross product)
        printf "static const float %s_normals[] = {\n", var_name;
        for (i = 1; i <= fc; i++) {
            fa = fi0[i]; fb = fi1[i]; fcc = fi2[i];
            e1x = vx[fb] - vx[fa]; e1y = vy[fb] - vy[fa]; e1z = vz[fb] - vz[fa];
            e2x = vx[fcc] - vx[fa]; e2y = vy[fcc] - vy[fa]; e2z = vz[fcc] - vz[fa];
            nx = e1y * e2z - e1z * e2y;
            ny = e1z * e2x - e1x * e2z;
            nz = e1x * e2y - e1y * e2x;
            len = sqrt(nx*nx + ny*ny + nz*nz);
            if (len > 0.0001) { nx /= len; ny /= len; nz /= len; }
            comma = (i < fc) ? "," : "";
            printf "    %8.5ff, %8.5ff, %8.5ff%s\n", nx, ny, nz, comma;
        }
        printf "};\n\n";

        # Face material colors (ARGB packed uint32_t)
        printf "static const unsigned int %s_colors[] = {\n", var_name;
        for (i = 1; i <= fc; i++) {
            mtl = face_mtl[i];
            if (mtl == "asphalt") {
                color = "0xFF404040";
            } else if (mtl == "concreteSmooth") {
                color = "0xFF808080";
            } else if (mtl == "grass") {
                color = "0xFF228B22";
            } else {
                color = "0xFF606060";
            }
            comma = (i < fc) ? "," : "";
            printf "    %s%s\n", color, comma;
        }
        printf "};\n\n";
    }
    ' "$OBJ_DIR/$obj_file"
}

# Count faces in first group only
count_first_group_faces() {
    local obj_file="$1"
    awk '
    BEGIN { fc = 0; group_count = 0; }
    /^g / { group_count++; }
    /^f / && group_count <= 1 { fc++; }
    END { print fc; }
    ' "$OBJ_DIR/$obj_file"
}

count_first_group_verts() {
    local obj_file="$1"
    awk '
    BEGIN { vc = 0; group_count = 0; }
    /^g / { group_count++; }
    /^v / && group_count <= 1 { vc++; }
    END { print vc; }
    ' "$OBJ_DIR/$obj_file"
}

# ----- Generate Header -----
cat > "$HEADER" << 'HEADER_EOF'
/*
 * RetroRacer - Embedded Road Tile Data
 * Auto-generated by tools/tiles2c.sh - do not edit manually
 */

#ifndef TILE_DATA_H
#define TILE_DATA_H

#include "render.h"

/* Available road tile types */
typedef enum {
HEADER_EOF

for entry in "${TILES[@]}"; do
    IFS=':' read -r enum_name obj_file display_name <<< "$entry"
    echo "    TILE_${enum_name}," >> "$HEADER"
done

cat >> "$HEADER" << 'HEADER_EOF'
    TILE_COUNT
} tile_id_t;

/* Initialize shared tile meshes (call once at startup) */
void tile_init(void);

/* Free shared tile meshes (call at shutdown) */
void tile_shutdown(void);

/* Get the shared mesh for a tile type */
mesh_t *tile_get_mesh(tile_id_t id);

#endif /* TILE_DATA_H */
HEADER_EOF

echo "  Generated $HEADER"

# ----- Generate Source -----
cat > "$SOURCE" << 'SOURCE_HEADER'
/*
 * RetroRacer - Embedded Road Tile Data
 * Auto-generated by tools/tiles2c.sh - do not edit manually
 *
 * Road tile models from Kenney Retro Urban Kit
 * https://kenney.nl/
 */

#include "tile_data.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

SOURCE_HEADER

# Generate each tile's data
for entry in "${TILES[@]}"; do
    IFS=':' read -r enum_name obj_file display_name <<< "$entry"
    echo "  Processing $obj_file..."
    generate_tile "$enum_name" "$obj_file" "$display_name" >> "$SOURCE"
done

# Generate tile info struct for mesh creation
cat >> "$SOURCE" << 'FUNCS'
/* Apply basic directional shading to a base color */
static uint32_t tile_shade_color(uint32_t base, float nx, float ny, float nz) {
    float dot = nx * 0.3f + ny * 0.7f + nz * 0.5f;
    if (dot < 0) dot = 0;
    float intensity = 0.4f + 0.6f * dot;
    if (intensity > 1.0f) intensity = 1.0f;

    uint32_t a = (base >> 24) & 0xFF;
    uint32_t r = (uint32_t)(((base >> 16) & 0xFF) * intensity);
    uint32_t g = (uint32_t)(((base >> 8) & 0xFF) * intensity);
    uint32_t b = (uint32_t)((base & 0xFF) * intensity);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

/* Shared tile meshes */
static mesh_t *tile_meshes[TILE_COUNT];

static mesh_t *create_tile_mesh(const float *verts, const int *faces,
                                const float *normals, const unsigned int *colors,
                                int face_count) {
    mesh_t *mesh = (mesh_t *)malloc(sizeof(mesh_t));
    if (!mesh) return NULL;

    mesh->tri_count = face_count;
    mesh->triangles = (triangle_t *)malloc(sizeof(triangle_t) * face_count);
    if (!mesh->triangles) { free(mesh); return NULL; }
    memset(mesh->triangles, 0, sizeof(triangle_t) * face_count);
    mesh->base_color = 0xFF404040;

    for (int i = 0; i < face_count; i++) {
        int i0 = faces[i * 3 + 0];
        int i1 = faces[i * 3 + 1];
        int i2 = faces[i * 3 + 2];

        float nx = normals[i * 3 + 0];
        float ny = normals[i * 3 + 1];
        float nz = normals[i * 3 + 2];
        uint32_t shaded = tile_shade_color(colors[i], nx, ny, nz);

        mesh->triangles[i].v[0].pos.x = verts[i0 * 3 + 0];
        mesh->triangles[i].v[0].pos.y = verts[i0 * 3 + 1];
        mesh->triangles[i].v[0].pos.z = verts[i0 * 3 + 2];
        mesh->triangles[i].v[0].color = shaded;

        mesh->triangles[i].v[1].pos.x = verts[i1 * 3 + 0];
        mesh->triangles[i].v[1].pos.y = verts[i1 * 3 + 1];
        mesh->triangles[i].v[1].pos.z = verts[i1 * 3 + 2];
        mesh->triangles[i].v[1].color = shaded;

        mesh->triangles[i].v[2].pos.x = verts[i2 * 3 + 0];
        mesh->triangles[i].v[2].pos.y = verts[i2 * 3 + 1];
        mesh->triangles[i].v[2].pos.z = verts[i2 * 3 + 2];
        mesh->triangles[i].v[2].color = shaded;
    }

    return mesh;
}

FUNCS

# Generate tile_init function
echo "void tile_init(void) {" >> "$SOURCE"
for entry in "${TILES[@]}"; do
    IFS=':' read -r enum_name obj_file display_name <<< "$entry"
    var_name=$(echo "$enum_name" | tr '[:upper:]' '[:lower:]')
    fc=$(count_first_group_faces "$obj_file")
    echo "    tile_meshes[TILE_${enum_name}] = create_tile_mesh(${var_name}_verts, ${var_name}_faces, ${var_name}_normals, ${var_name}_colors, ${fc});" >> "$SOURCE"
done
echo "}" >> "$SOURCE"
echo "" >> "$SOURCE"

# Generate tile_shutdown function
echo "void tile_shutdown(void) {" >> "$SOURCE"
echo "    for (int i = 0; i < TILE_COUNT; i++) {" >> "$SOURCE"
echo "        if (tile_meshes[i]) {" >> "$SOURCE"
echo "            mesh_destroy(tile_meshes[i]);" >> "$SOURCE"
echo "            tile_meshes[i] = NULL;" >> "$SOURCE"
echo "        }" >> "$SOURCE"
echo "    }" >> "$SOURCE"
echo "}" >> "$SOURCE"
echo "" >> "$SOURCE"

# Generate tile_get_mesh function
echo "mesh_t *tile_get_mesh(tile_id_t id) {" >> "$SOURCE"
echo "    if (id < 0 || id >= TILE_COUNT) return tile_meshes[0];" >> "$SOURCE"
echo "    return tile_meshes[id];" >> "$SOURCE"
echo "}" >> "$SOURCE"

echo "  Generated $SOURCE"
echo "  Done!"
