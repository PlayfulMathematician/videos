/*
    main.c - A program that handles most things
    Copyright (C) 2025 PlayfulMathematician

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/


/*
Hello, main.c contains about a thousand lines of code pertaining to many things that seem disconnected. 
Let me explain what this file is actually for. 
I am making an animation engine in C. It will animate polyhedra.
Thus there are like 4 main sections of code:
    OpenGL+Win32
    Polyhedra Triangulation
    OFF Parsing
    Animation Structs (mostly boilerplate)

This is a disorganized mess right now, but notice most functions return an int. 
An int in the Result enum. 
This will be restructured so the result is inserted as a parameter.
This allow for more useful outputs to functions.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <windows.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <GL/gl.h>
#include <stdio.h>
#define EPSILON 0.0001
// this handles bit alignment to use less mallocs
#define BIT_IGNORE 4
#define BIT_ALIGN(x) (((x) + ((1 << BIT_IGNORE) - 1)) & ~((1 << BIT_IGNORE) - 1))
#define REALIGN(a, b) BIT_ALIGN((a)) != BIT_ALIGN((b)) 
// error codes
#define STATUS_TYPE_SHIFT 24
#define STATUS_TYPE_MASK  0xFF000000  
#define STATUS_INFO_MASK  0x00FFFFFF  
#define SUCCESS 0x00
#define NOOP 0x01
#define NONFATAL 0x02 // consider removing
#define FATAL 0x03
#define STATUS_TYPE(code) (((code) & STATUS_TYPE_MASK) >> STATUS_TYPE_SHIFT)
#define IS_ERROR(x) ((STATUS_TYPE((x)) == FATAL) & (STATUS_TYPE((x)) == NONFATAL))
#define TRI_INIT_MALLOC_FAIL 0x03000000
#define TRI_NOT_FOUND 0x03000001
#define ADDING_TRI_REALLOC_FAILURE 0x03000002

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
    int (*edges)[2]; // diametrically opposed foes, previously closed bros
    int vertex_count;
    int edge_count;
} 
PSLG;

typedef struct
{
    Vec3 (*triangles)[3];
    int triangle_count;
}
Triangulation;

typedef struct
{
    PSLG* pslg;
    Triangulation* triangulation;
}
PSLGTriangulation;

typedef struct
{
    void** data;
    int dumpster_size;
}
Dumpster;

typedef struct Animation Animation;
typedef struct GlobalBuffer GlobalBuffer;
typedef struct VideoData VideoData;  
typedef struct AnimationSection AnimationSection;  



struct Animation
{
    int start_t;
    int end_t;
    void (*construct)(struct Animation*);
    void (*preproc)(struct Animation*, int t);
    void (*render)(struct Animation*, int t);
    void (*postproc)(struct Animation*, int t);
    void (*free)(struct Animation*);
    Dumpster dumpster;
};


struct AnimationSection
{
    char* name;
    Animation* animations;
    int animation_count;
    int start_t;
    int end_t;
    void (*init)(struct AnimationSection*);
    VideoData* vd;
};

struct VideoData
{
    AnimationSection* animation_section;
    int section_count;
    GlobalBuffer* gb;
    void (*init)(struct VideoData*); 
    void (*free)(struct VideoData*); 
};

typedef struct
{
   char*** sounds;
   int* sound_count;
   float** start_t;
   float** end_t;
   int channel_count;
   GlobalBuffer* gb;
}
SoundData;

struct GlobalBuffer
{
    SoundData* sounddata;
    VideoData* videodata;
};

Triangulation* empty_triangulation(int* result)
{
    Triangulation* tri = malloc(sizeof(Triangulation));
    if (!tri)
    {   
        *result = TRI_INIT_MALLOC_FAIL;
        return NULL; 
    }
    tri->triangle_count = 0;
    tri->triangles = NULL;
    return tri;

}

void add_triangle(int* result, Triangulation* tri, Vec3 a, Vec3 b, Vec3 c)
{
    if (!tri) 
    {
        *result = TRI_NOT_FOUND;
        return;
    }

    if (REALIGN(tri->triangle_count, tri->triangle_count + 1))
    {
        int new_capacity = BIT_ALIGN(tri->triangle_count + 1); 
        Vec3 (*temp)[3] = realloc(tri->triangles, new_capacity * sizeof(Vec3[3]));
        if (!temp)
        {
            *result = ADDING_TRI_REALLOC_FAILURE;
            return;
        }
        tri->triangles = temp;
    }
    tri->triangles[tri->triangle_count][0] = a;
    tri->triangles[tri->triangle_count][1] = b;
    tri->triangles[tri->triangle_count][2] = c;
    tri->triangle_count++;
    *result = SUCCESS;
}

void merge_triangulations(int* result, Triangulation** triangulations, int tri_count, Triangulation* output)
{
    
    Triangulation* e = empty_triangulation();
    if (e == NULL)
    {
        *result = TRI_INIT_MALLOC_FAIL;
        return;
    }
    output->triangle_count = e->triangle_count;
    output->triangles = e->triangles;
    /*
        Note we use free instead of free_triangulation because
        result->triangles points to the same memory as e->triangles and 
        free_triangulation(e) would free e->triangles.
    */
    free(e);
    
    for (int i = 0; i < tri_count; i++)
    {
        for (int j = 0; j < triangulations[i]->triangle_count; j++)
        {
            int* out;
            add_triangle(
                out,
                output, 
                triangulations[i]->triangles[j][0],
                triangulations[i]->triangles[j][1],
                triangulations[i]->triangles[j][2],
            );
            if (IS_ERROR(*out))
            {
                *result = *out;
                return;
            }
        }
    }
    *result = SUCCESS;
}

void free_triangulation(int* result, Triangulation* triangulation)
{
    free(triangulation->triangles);   
    triangulation->triangles = NULL;
    triangulation->triangle_count = 0;
    free(triangulation);              
    *result = SUCCESS;
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

Vec3 subtract_vec3(Vec3 a, Vec3 b)
{
    return add_vec3(a, multiply_vec3(b, -1.0f));
}

Vec3 lerp_vec3(Vec3 a, Vec3 b, float t)
{
    return add_vec3(a, multiply_vec3(subtract_vec3(b, a), t));
}

void intersecting_segments(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3* out)
{
    
    if (a.x == b.x && a.y == b.y && c.x == d.x && c.y == d.y)
    {
        return;
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
            return;
        }
        if (ty < 0 || ty > 1)
        {
            return;
        }
        float t_avg;
        if (fabs(tx - ty) < EPSILON)
        {
            t_avg = (tx + ty) / 2;
        }
        else
        {
            return;
        }
        if (vertical == 1)
        {
            *out = lerp_vec3(c, d, t_avg);
        }
        else if (vertical == 2)
        {
            *out = lerp_vec3(a, b, t_avg);
        }
        return 1;
    }
    float denom = (a.x - b.x)*(c.y - d.y) - (a.y - b.y)*(c.x - d.x);

    if (fabs(denom) < EPSILON)
    {
        return;
    }
    float t = ((a.x - c.x)*(c.y - d.y) - (a.y - c.y)*(c.x-d.x)) / denom;
    float u = -((a.x - b.x)*(a.y - c.y) - (a.y - b.y)*(a.x-c.x)) / denom;
    if (t < 0 || t > 1)
    {
        return 0;
    }
    if (u < 0 || u > 1)
    {
        return;
    }

    Vec3 v1 = lerp_vec3(a, b, t);
    Vec3 v2 = lerp_vec3(c, d, u);
    if (fabs(v1.z - v2.z) < EPSILON)
    {
        *out = lerp_vec3(v1, v2, 0.5f);
        return;
    }
    return;
}

PSLG* generate_pslg(Vec3* vertices, int vertex_count)
{
    PSLG* new = malloc(sizeof(PSLG));
    if(!new)
    {
        return new;
    }
    new->vertex_count = vertex_count;
    new->edge_count = vertex_count;
    new->vertices = malloc(vertex_count * sizeof(Vec3));
    if (!new->vertices) 
    {
        new->vertex_count = 0;
        new->edge_count = 0;
        new->edges = NULL;
        return new;
    }
    for (int i = 0; i < vertex_count; i++)
    {
        new->vertices[i] = vertices[i];
    }

    new->edges = malloc(new->edge_count * sizeof(int*));
    if (!new->edges) 
    {
        free(new->vertices);
        new->vertex_count = 0;
        new->edge_count = 0;
        return new;
    }

    for (int i = 0; i < vertex_count; i++)
    {
        new->edges[i] = malloc(2 * sizeof(int));
        if (!new->edges[i]) 
        {
            for (int j = 0; j < i; j++) 
            {
                free(new->edges[j]);
            }
            free(new->edges);
            free(new->vertices);
            new->vertex_count = 0;
            new->edge_count = 0;
            new->edges = NULL;
            new->vertices = NULL;
            return new;
        }
        new->edges[i][0] = i;
        new->edges[i][1] = (i + 1) % vertex_count; 
    }

    return new;
}
int splitPSLG(PSLG* pslg, int edge1, int edge2)
{
    Vec3 out;
    if
    (
        pslg->edges[edge1][0] == pslg->edges[edge2][0] ||
        pslg->edges[edge1][0] == pslg->edges[edge2][1] ||
        pslg->edges[edge1][1] == pslg->edges[edge2][0] ||
        pslg->edges[edge1][1] == pslg->edges[edge2][1]
    )
    {
        return NOOP;
    }

    Vec3 v1 = pslg->vertices[pslg->edges[edge1][0]];
    Vec3 v2 = pslg->vertices[pslg->edges[edge1][1]];
    Vec3 v3 = pslg->vertices[pslg->edges[edge2][0]];
    Vec3 v4 = pslg->vertices[pslg->edges[edge2][1]];
    if(!intersecting_segments(
        v1,
        v2,
        v3,
        v4,
        &out
    ))
    {
        return NOOP;
    }
    // TODO: HANDLE REALLOC FAILURE
    {
        Vec3* temp_ptr = realloc(pslg->vertices, sizeof(Vec3) * (pslg->vertex_count + 1));
        if (temp_ptr == NULL)
        {
            printf("SOB\n");
            return FAILURE;
        }
        pslg->vertices = temp_ptr;  
    }
    int** temp_ptr = realloc(pslg->edges, sizeof(int*) * (pslg->edge_count + 2));
    if (temp_ptr == NULL)
    {
        printf("DE\n");
        return FAILURE;
    }
    pslg->edges = temp_ptr;
    pslg->edges[pslg->edge_count] = malloc(2 * sizeof(int));
    if(!pslg->edges[pslg->edge_count])
    {
        printf("DO\n");
        return FAILURE;
    }
    pslg->edges[pslg->edge_count + 1] = malloc(2 * sizeof(int));
    if(!pslg->edges[pslg->edge_count + 1])
    {
        printf("asdfasdfasfd\n");
        return FAILURE;
    }
    pslg->vertices[pslg->vertex_count] = out;
    pslg->edges[pslg->edge_count][0] = pslg->edges[edge1][1];
    pslg->edges[pslg->edge_count][1] = pslg->vertex_count;
    pslg->edges[pslg->edge_count+1][0] = pslg->edges[edge2][1];
    pslg->edges[pslg->edge_count+1][1] = pslg->vertex_count;
    pslg->edges[edge1][1] = pslg->vertex_count;
    pslg->edges[edge2][1] = pslg->vertex_count;
    pslg->edge_count+=2;
    pslg->vertex_count+=1;
    return SUCCESS;
}

int remove_single_edge(PSLG* pslg)
{
    for(int i = 0; i < pslg->edge_count; i++)
    {
        for(int j = 0; j < pslg->edge_count; j++)
        {
            int result = splitPSLG(pslg, i, j);
            if(result == NOOP)
            {
                continue;
            }
            return result;
        }
    }
    return NOOP;
}

int split_entirely(PSLG* pslg)
{
    while(1)
    {
        int result = remove_single_edge(pslg);
        if(result == NOOP) 
        {
            return SUCCESS;
        }
        if(result == FAILURE)
        {
            return FAILURE;
        }
    }
}

int free_pslg(PSLG* pslg)
{
    if (!pslg)
    {
        return NOOP;
    }
    for (int i = 0; i < pslg->edge_count; i++)
    {
        free(pslg->edges[i]);
    }

    free(pslg->edges);
    free(pslg->vertices);
    free(pslg);
    return SUCCESS;
}

PSLGTriangulation* create_pslg_triangulation(PSLG* pslg, int* result)
{
    PSLGTriangulation* pslgtri = malloc(sizeof(PSLGTriangulation));
    if (!pslgtri)
    {
        *result = FAILURE;
    }
    pslgtri->triangulation = empty_triangulation();
    if (!pslgtri->triangulation)
    {
        free(pslgtri);
        return NULL;
    }
    pslgtri->pslg = pslg;
    *result = SUCCESS;
    return pslgtri;
}



int attack_vertex(PSLGTriangulation* pslgtri, int vertex_idx)
{   
    PSLG* pslg = pslgtri->pslg;
    Triangulation* tri = pslgtri->triangulation;
    int d = 0;
    int e1;
    int e2;
    for(int i = 0; i < pslg->edge_count; i++)
    {
        if
        (
            pslg->edges[i][0] == vertex_idx ||
            pslg->edges[i][1] == vertex_idx
        )
        {
            d++;
            if (d > 2)
            {
                return NOOP;
            }
            if (d > 1)
            {
                e2 = i;
            }
            else
            {
                e1 = i;
            }
        }
    }
    if (d != 2)
    {
        return NOOP;
    }
    int v1 = pslg->edges[e1][0];
    int v2 = pslg->edges[e2][0];
    int v3 = pslg->edges[e2][1];
    if (v1 == vertex_idx)
    {
        v1 = pslg->edges[e1][1];
    }
    if (v2 == vertex_idx)
    {
        v2 = pslg->edges[e2][1];
        v3 = pslg->edges[e2][0];
    }
    add_triangle(tri, pslg->vertices[v1], pslg->vertices[v2], pslg->vertices[v3]);
    int e3_exists = 0;
    for(int i = 0; i < pslg->edge_count; i++)
    {
        if
        (
            (pslg->edges[i][0] == v1 && pslg->edges[i][1] == v2) ||
            (pslg->edges[i][0] == v2 && pslg->edges[i][1] == v1)
        )
        {   
            e3_exists = 1;
            break;
        }
    }
    free(pslg->edges[e1]);
    free(pslg->edges[e2]);
    int** temp_ptr;
    int ecount;
    if(e3_exists)
    {
        ecount = (pslg->edge_count - 1);
    }
    else
    {
        ecount = (pslg->edge_count - 2);
    }
    temp_ptr = malloc(ecount * sizeof(int*));
    if (!temp_ptr)
    {
        return FAILURE;
    }
    int ei = 0;
    for(int i = 0; i < pslg->edge_count; i++)
    {
        if(i == e2 || i == e1)
        {
            continue;
        }
        temp_ptr[ei] = pslg->edges[i];
        ei++;
    }
    if(!e3_exists)
    {
        temp_ptr[ei] = malloc(2 * sizeof(int));
        if (!temp_ptr[ei])
        {
            return FAILURE; // i am so tired of checking if malloc fails JUST WORK
        }
        temp_ptr[ei][0] = v1;
        temp_ptr[ei][1] = v2;
    }
    free(pslg->edges);
    pslg->edges = temp_ptr;
    pslg->edge_count = ecount;
    return SUCCESS;
}

int attack_single_vertex(PSLGTriangulation* pslgtri)
{
    for(int i = 0; i < pslgtri->pslg->vertex_count; i++)
    {
        int result = attack_vertex(pslgtri, i);
        if(result == NOOP)
        {
            continue;
        }
        return result;
    }
    return NOOP;
}

int attack_all_vertices(PSLGTriangulation* pslgtri)
{
    while(1)
    {
        int result = attack_single_vertex(pslgtri);
        if(result == NOOP)
        {
            return SUCCESS;
        }
        if(result == FAILURE)
        {
            return FAILURE;
        }
    }
}

int generate_triangulation(Vec3* vertices, int vertex_count, Triangulation* tri)
{
    PSLG* pslg = generate_pslg(vertices, vertex_count);
    if (split_entirely(pslg) == FAILURE)
    {
        return FAILURE;
    }
    int result;
    PSLGTriangulation* pslgtri = create_pslg_triangulation(pslg, &result);
    if (result == FAILURE)
    {
        free_pslg(pslg);
        return FAILURE;
    }
    if(attack_all_vertices(pslgtri) == FAILURE)
    {
        return FAILURE;
    }
    for(int i = 0; i < pslgtri->triangulation->triangle_count; i++)
    {
        add_triangle(tri, 
            pslgtri->triangulation->triangles[i][0],
            pslgtri->triangulation->triangles[i][1],
            pslgtri->triangulation->triangles[i][2]
        );
    }
    tri->triangle_count = pslgtri->triangulation->triangle_count;
    free_pslg(pslgtri->pslg);
    free_triangulation(pslgtri->triangulation);
    free(pslgtri);
    return SUCCESS;
}

// a culmination of a lot of this project
int triangulate_polyhedra(Polyhedron* poly, Triangulation* result)
{
    printf("G\n");
    Triangulation** tris = malloc(poly->face_count * sizeof(Triangulation*));
    if (!tris)
    {
        return FAILURE;
    }
    printf("DO YOU DIE TODAY\n");
    for (int i = 0; i < poly->face_count; i++)
    {
        Triangulation* t = empty_triangulation();
        Vec3* vertices = malloc(poly->face_sizes[i] * sizeof(Vec3));
        if (!vertices)
        {
            return FAILURE;
        }
        for (int j = 0; j < poly->face_sizes[i]; j++)
        {
            vertices[j] = poly->vertices[poly->faces[i][j]];
        }
        int result = generate_triangulation(vertices, poly->face_sizes[i], t);
        if (result == FAILURE)
        {
            printf("DO YOU YOYO\n");
            return FAILURE;
        }
        free(vertices);
        tris[i] = t;
        printf("fas\n");
    }
    merge_triangulations(tris, poly->face_count, result);
    for(int i = 0; i < poly->face_count; i++)
    {
        free_triangulation(tris[i]);
    }
    free(tris);
    return SUCCESS;
}

// t
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
    if (!poly)
    {
        exit(1);
    }
    poly->vertex_count = nv;
    poly->face_count = nf;
    poly->vertices = malloc(nv * sizeof(Vec3));
    if (!poly->vertices)
    {
        exit(1);
    }
    poly->faces = malloc(nf * sizeof(int*));
    if (!poly->faces)
    {
        exit(1);
    }
    poly->face_sizes = malloc(nf * sizeof(int));
    if (!poly->face_sizes)
    {
        exit(1);
    }
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

int read_off_header (FILE* fin, int* nv, int* nf)
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

int read_vertex (FILE* fin, Polyhedron* poly, int vertex_idx)
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

int read_face (FILE* fin, Polyhedron* poly, int face_index)
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
    char* ptr = line;

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

int read_faces(FILE* fin, Polyhedron* poly)
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



int read_vertices (FILE* fin, Polyhedron* poly) 
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

Polyhedron* polyhedron_from_off(FILE* fin)
{
    int nv, nf;
    int result = read_off_header(fin, &nv, &nf);
    if (result == FAILURE)
    {
        return NULL;
    }

    Polyhedron* poly = create_polyhedron(nv, nf);
    result = read_vertices(fin, poly);
    if (result == FAILURE)
    {
        return NULL;
    }
    result = read_faces(fin, poly);
    if (result == FAILURE)
    {
        return NULL;
    }
    return poly;
}
// WARNING THIS FUNCTION IS DEEPLY NESTED
void render_gb(GlobalBuffer* gb, int t)
{
    for(int i = 0; i < gb->videodata->section_count; i++)
    {
        if (gb->videodata->animation_section[i].end_t <= t || gb->videodata->animation_section[i].start_t >= t)
        {
            if (t == gb->videodata->animation_section[i].start_t)
            {
                gb->videodata->animation_section[i].init(&gb->videodata->animation_section[i]);
            }
            if (t == gb->videodata->animation_section[i].end_t)
            {
                free(gb->videodata->animation_section[i].animations);
                free(&gb->videodata->animation_section[i]);
            }
            else
            {
                for (int j = 0; j < gb->videodata->animation_section[i].animation_count; j++)
                {
                    Animation* a = &gb->videodata->animation_section[i].animations[j];
                    if (a->start_t == t)
                    {
                        a->construct(a);
                    }
                }
                for (int j = 0; j < gb->videodata->animation_section[i].animation_count; j++)
                {
                    Animation* a = &gb->videodata->animation_section[i].animations[j];
                    if (a->start_t <= t && a->end_t >= t)
                    {
                        a->preproc(a,t);
                    }
                }
                for (int j = 0; j < gb->videodata->animation_section[i].animation_count; j++)
                {
                    Animation* a = &gb->videodata->animation_section[i].animations[j];
                    if (a->start_t <= t && a->end_t >= t)
                    {
                        a->render(a, t);
                    }
                }
                for (int j = 0; j < gb->videodata->animation_section[i].animation_count; j++)
                {
                    Animation* a = &gb->videodata->animation_section[i].animations[j];
                    if (a->start_t <= t && a->end_t >= t)
                    {
                        a->postproc(a, t);
                    }
                }
                for (int j = 0; j < gb->videodata->animation_section[i].animation_count; j++)
                {
                    Animation* a = &gb->videodata->animation_section[i].animations[j];
                    if (a->end_t == t)
                    {
                        a->free(a);
                    }
                }
            }
        }    
    }
}


float angle = 0.0f;

void draw_tri(Triangulation* tri) {
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < tri->triangle_count; i++) {
        Vec3* t = tri->triangles[i];
        glColor3f(1.0f, 0.5f, 0.2f);
        glVertex3f(t[0].x, t[0].y, t[0].z);
        glVertex3f(t[1].x, t[1].y, t[1].z);
        glVertex3f(t[2].x, t[2].y, t[2].z);
    }
    glEnd();
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window *win = SDL_CreateWindow("Rotating Cube",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    glEnable(0x809D);
    glEnable(GL_DEPTH_TEST);
    printf("D\n");
    FILE* fin = fopen("../media/models/off/gad.off", "r");
    if(!fin)
    {
        printf("DO\n");
        return 1;
    }
    printf("DFDS\n");
    Polyhedron* poly = polyhedron_from_off(fin);
    if (poly == NULL) 
    {
        printf("DIE\n");
        return 1;
    }
    printf("DFdDS\n");


    Triangulation* tri = empty_triangulation();
    if (tri == NULL)
    {
        printf("DO YOU DIE\n");
        return 1;
    }
    printf("DFdddDS\n");

    if(triangulate_polyhedra(poly, tri) == FAILURE)
    {
        printf("TODAY YOU MUST DIE\n");
        return 1;
    }

    printf("Data: %d\n", tri->triangle_count);
    SDL_Event e;
    int running = 1;
    while (running) {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT) 
            {
                running = 0;
            }
        }

        glClearColor(0.0f,0.0f,0.0f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = 800.0f/600.0f;
        glFrustum(-1*aspect, 1*aspect, -1, 1, 1, 10);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0,0,-3);
        glRotatef(angle, 1, 1, 0);

        draw_tri(tri);

        SDL_GL_SwapWindow(win);
        angle += 0.1f;
    }

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
