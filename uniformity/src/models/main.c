#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

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
    return 0;
    error:
        fprintf(stderr, "Error");
        if (fin)
        {
            fclose(fin);
        }
        return 1;
}
