/*
main.c - A program that does off parsing and rendering and stuff
    Copyright (C) 2025 PlayfulMathematician

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#define EPSILON 0.0001
typedef struct 
{
    float x, y, z;
} 
Vec3;

typedef struct 
{
    int vertex_count;
    int face_count;
    Vec3* vertices;
    int** faces;
    int* face_sizes;
} 
Polyhedron;

typedef struct 
{
    Vec3* vertices;
    int** edges;
    int vertex_count;
    int edge_count;
} PSLG;

Vec3 subtract_vec3(Vec3 a, Vec3 b)
{
    Vec3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

Vec3 add_vec3(Vec3 a, Vec3 b)
{
    Vec3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

Vec3 multiply_vec3(Vec3 a, float scalar)
{
    Vec3 result;
    result.x = a.x * scalar;
    result.y = a.y * scalar;
    result.z = a.z * scalar;
    return result;
}

PSLG generate_plsg(Vec3* vertices, int vertex_count)
{
    PSLG new;
    new.vertex_count = vertex_count;
    new.edge_count = vertex_count;
    new.vertices = malloc(vertex_count * sizeof(Vec3));
    if (!new.vertices) 
    {
        new.vertex_count = 0;
        new.edge_count = 0;
        new.edges = NULL;
        return new;
    }
    for (int i = 0; i < vertex_count; i++)
    {
        new.vertices[i] = vertices[i];
    }

    new.edges = malloc(new.edge_count * sizeof(int*));
    if (!new.edges) 
    {
        free(new.vertices);
        new.vertex_count = 0;
        new.edge_count = 0;
        return new;
    }

    for (int i = 0; i < vertex_count; i++)
    {
        new.edges[i] = malloc(2 * sizeof(int));
        if (!new.edges[i]) 
        {
            for (int j = 0; j < i; j++) 
            {
                free(new.edges[j]);
            }
            free(new.edges);
            free(new.vertices);
            new.vertex_count = 0;
            new.edge_count = 0;
            new.edges = NULL;
            new.vertices = NULL;
            return new;
        }
        new.edges[i][0] = i;
        new.edges[i][1] = (i + 1) % vertex_count; 
    }

    return new;
}

int intersecting_segments(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3* out)
{
    
    if (a.x == b.x && a.y == b.y && c.x == d.x && c.y == d.y)
    {
        return 0;
    }
    float tx; 
    float ty;
    int vertical = 0;
    if (a.x == b.x && a.y == b.y)
    {
        tx = (a.x - c.x) / (d.x - c.x);
        ty = (a.y - c.y) / (d.y - c.y);
        vertical = 1;
    }
    if (c.x == d.x && c.y == d.y)
    {
        tx = (c.x - a.x) / (b.x - a.x);
        ty = (c.y - a.y) / (b.y - a.y);
        vertical = 2;
    }
    if (vertical > 0)
    {
        if (tx < 0 || tx > 1)
        {
            return 0;
        }
        if (ty < 0 || ty > 1)
        {
            return 0;
        }
        float t_avg;
        if (fabs(tx - ty) < EPSILON)
        {
            t_avg = (tx + ty) / 2;
        }
        else
        {
            return 0;
        }
        if (vertical == 1)
        {
            *out = add_vec3(c, multiply_vec3(subtract_vec3(d, c), t_avg));
        }
        else if (vertical == 2)
        {
            *out = add_vec3(a, multiply_vec3(subtract_vec3(b, a), t_avg));
        }
        return 1;
    }
    float denom = (a.x - b.x)*(c.y - d.y) - (a.y - b.y)*(c.x - d.x);

    if (fabs(denom) < EPSILON)
    {
        return 0;
    }
    float t = ((a.x - c.x)*(c.y - d.y) - (a.y - c.y)*(c.x-d.x)) / denom;
    float u = -((a.x - b.x)*(a.y - c.y) - (a.y - b.y)*(a.x-c.x)) / denom;
    if (t < 0 || t > 1)
    {
        return 0;
    }
    if (u < 0 || u > 1)
    {
        return 0;
    }
    Vec3 v1 = add_vec3(a, multiply_vec3(subtract_vec3(b, a), t));
    Vec3 v2 = add_vec3(c, multiply_vec3(subtract_vec3(d, c), u));
    if (fabs(v1.z - v2.z) < EPSILON)
    {
        *out = multiply_vec3(add_vec3(v1, v2), 0.5f);
        return 1;
    }
    return 0;
}

void print_vertex(Vec3 v)
{
    printf("\t\tX: %f\n", v.x);
    printf("\t\tY: %f\n", v.y);
    printf("\t\tZ: %f\n", v.z);
}
void print_polyhedron(Polyhedron* poly)
{
    printf("Vertices:\n");
    for (int i = 0; i < poly->vertex_count; i++)
    {   
        printf("\tVertex %d: \n", i);
        print_vertex(poly->vertices[i]);
    }
    printf("Faces:\n");
    for (int i = 0; i < poly->face_count; i++)
    {   
        printf("\tFace %d: \n", i);
        for(int j = 0; j < poly->face_sizes[i]; j++)
        {
            printf("\t\t%d\n",poly->faces[i][j]);
        }
    }
}

Polyhedron* create_polyhedron(int nv, int nf) 
{
    Polyhedron* poly = malloc(sizeof(Polyhedron));
    poly->vertex_count = nv;
    poly->face_count = nf;
    poly->vertices = malloc(nv * sizeof(Vec3));
    poly->faces = malloc(nf * sizeof(int*));
    poly->face_sizes = malloc(nf * sizeof(int));
    return poly;
}

void free_polyhedron(Polyhedron* poly)
{
    for(int i = 0; i < poly->face_count; i++)
    {
        free(poly->faces[i]);
    }
    free(poly->face_sizes);
    free(poly->faces);
    free(poly->vertices);
    free(poly);
}

int read_off_header (FILE *fin, int *nv, int *nf)
{
    char line[256];
    if (!fgets(line, sizeof(line), fin))
    {
        return 0;
    }
    if (strncmp(line, "OFF", 3) != 0)
    {
        return 0;
    }
    do 
    {
        if (!fgets(line, sizeof(line), fin))
        {
            return 0;
        }
    }
    while (line[0] == '#' || line[0] == '\n');
    int _;
    int* __ = &_;
    
    if (sscanf(line, "%d %d %d", nv, nf, __) != 3)
    {
        return 0;
    }
    return 1;
}

int read_vertex (FILE *fin, Polyhedron *poly, int vertex_idx)
{
    char line[256];
    do 
    {
        if (!fgets(line, sizeof(line), fin))
        {
            return 0;
        }
    } 
    while (line[0] == '#' || line[0] == '\n');
    float x, y, z;
    if (sscanf(line, "%f %f %f", &x, &y, &z) != 3) 
    {
        return 0;
    }
    poly->vertices[vertex_idx].x = x;
    poly->vertices[vertex_idx].y = y;
    poly->vertices[vertex_idx].z = z;
    return 1;
}

int read_face (FILE *fin, Polyhedron *poly, int face_index)
{

    char line[1024];
    do 
    {
        if (!fgets(line, sizeof(line), fin))
        {
            return 0;
        }
    } 
    while (line[0] == '#' || line[0] == '\n');
    int n;
    char *ptr = line;

    if (sscanf(ptr, "%d", &n) != 1)
    {
        return 0;
    }
    poly->face_sizes[face_index] = n;
    poly->faces[face_index] = malloc(n * sizeof(int));
    if (!poly->faces[face_index])
    {
        return 0;
    }
    ptr = strchr(ptr, ' ');
    if (!ptr)
    {
        return 0;        
    }
    for (int j = 0; j < n; j++)
    {
        while(*ptr == ' ') 
        {
            ptr++; 
        }
        if (sscanf(ptr, "%d", &poly->faces[face_index][j]) != 1)
        {
            return 0;
        }
        ptr = strchr(ptr, ' ');
        if (!ptr && j < n - 1)
        {
            return 0;
        }
    }
    return 1;

}

int read_faces(FILE *fin, Polyhedron* poly)
{
    for (int i = 0; i < poly->face_count; i++)
    {
        if(!read_face(fin, poly, i))
        {
            return 0;
        }
    }
    return 1;
}

int read_vertices (FILE *fin, Polyhedron* poly) 
{
    for (int i = 0; i < poly->vertex_count; i++) 
    {
        if (!read_vertex(fin, poly, i)) 
        {
            return 0; 
        }
    }
    return 1;
}

int main (int argc, char *argv[])
{
    if (argc < 2) 
    {
        fprintf(stderr, "Usage: %s filename.off\n", argv[0]);
        return 1;
    }
    const char *filename = argv[1];
    FILE *fin = fopen(filename, "r");
    if (!fin) 
    {
        fprintf(stderr, "Error: Could not open file.\n");
        return 1;   
    }
    int nv, nf;
    if (!read_off_header(fin, &nv, &nf)) 
    {
        goto error;
    }

    Polyhedron* poly = create_polyhedron(nv, nf);
    if (!read_vertices(fin, poly)) 
    {
        goto error;
    }
    if (!read_faces(fin, poly)) 
    {
        goto error;
    }
    print_polyhedron(poly);
    fclose(fin);
    free_polyhedron(poly);
    return 0;
    error:
        fprintf(stderr, "Error");
        if (fin)
        {
            fclose(fin);
        }
        if (poly)
        {
            free_polyhedron(poly);
        }
        return 1;
}
