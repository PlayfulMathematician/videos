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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <windows.h>
#include <gl/GL.h>
#include <mmsystem.h>
#pragma comment(lib,"winmm.lib") 
#define EPSILON 0.0001

enum Result
{
    FAILURE,
    SUCCESS,
    NOOP
};

void reshape_fixed_ratio(int win_w, int win_h)
{
    if (win_h <= 0) win_h = 1;
    float target_aspect = 16.0f / 9.0f;   
    float window_aspect = (float)win_w / (float)win_h;
    int vp_x = 0, vp_y = 0;
    int vp_w = win_w, vp_h = win_h;
    if (window_aspect > target_aspect) {
        //wide
        vp_w = (int)(win_h * target_aspect);
        vp_x = (win_w - vp_w) / 2;
    } else {
        //tall
        vp_h = (int)(win_w / target_aspect);
        vp_y = (win_h - vp_h) / 2;
    }
    glViewport(vp_x, vp_y, vp_w, vp_h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

const char g_szClassName[] = "MyWindowClass";

typedef union 
{
    struct 
    { 
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };
    uint8_t c[3];
} 
Color;

Color make_color3(uint8_t r, uint8_t g, uint8_t b) 
{
    Color col = { .r = r, .g = g, .b = b };
    return col;
}

Color make_color_array(uint8_t arr[3]) 
{
    Color col;
    col.c[0] = arr[0];
    col.c[1] = arr[1];
    col.c[2] = arr[2];
    return col;
}

void glColor(Color c) 
{
    glColor3f(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f);
}

void print_color(Color c) 
{
    printf("Color: R=%u, G=%u, B=%u\n", c.r, c.g, c.b);
}

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
} 
PSLG;

typedef struct
{
    Vec3** triangles;
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

struct Animation
{
    int start_t;
    int end_t;
    void (*construct)(Animation*);
    void (*preproc)(struct Animation*, int t);
    void (*render)(struct Animation*, int t);
    void (*postproc)(struct Animation*, int t);
    void (*free)(struct Animation*);
    Dumpster dumpster;
};

typedef struct AnimationSection AnimationSection;  // forward declaration
typedef struct VideoData VideoData;  
typedef struct GlobalBuffer GlobalBuffer;  


struct AnimationSection
{
    char* name;
    Animation* animations;
    int animation_count;
    int start_t;
    int end_t;
    void (*render)(struct AnimationSection*, int t); // rendering handled here for order stuff
    void (*init)(struct AnimationSection*);
    Dumpster dumpster;
    VideoData* vd;
};

struct VideoData
{
    AnimationSection* animation_section;
    int section_count;
    GlobalBuffer* gb;
    void (*init)(struct VideoData*); 
};

typedef struct
{
   char*** sounds;
   int* sound_count;
   int channel_count;
   GlobalBuffer* gb;
}
SoundData;

struct GlobalBuffer
{
    SoundData* sounddata;
    VideoData* videodata;
};

Triangulation* empty_triangulation()
{
    Triangulation* tri = malloc(sizeof(Triangulation));
    if (!tri)
    {
        return NULL;
    }
    tri->triangle_count = 0;
    tri->triangles = NULL;
    return tri;

}

int add_triangle(Triangulation* tri, Vec3 a, Vec3 b, Vec3 c)
{
    if (!tri) 
    {
        return FAILURE;
    }

    tri->triangles = realloc(tri->triangles, (tri->triangle_count+1) * sizeof(Vec3*) );
    tri->triangles[tri->triangle_count] = malloc(3 * sizeof(Vec3));
    if(!tri->triangles[tri->triangle_count])
    {
        return FAILURE;
    }
    tri->triangles[tri->triangle_count][0] = a;
    tri->triangles[tri->triangle_count][1] = b;
    tri->triangles[tri->triangle_count][2] = c;
    tri->triangle_count++;
    return SUCCESS;
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

    Vec3 v1 = lerp_vec3(a, b, t);
    Vec3 v2 = lerp_vec3(c, d, u);
    if (fabs(v1.z - v2.z) < EPSILON)
    {
        *out = lerp_vec3(v1, v2, 0.5f);
        return 1;
    }
    return 0;
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
    pslg->vertices = realloc(pslg->vertices, sizeof(Vec3) * (pslg->vertex_count + 1));
    pslg->edges = realloc(pslg->edges, sizeof(int*) * (pslg->edge_count + 2));  
    pslg->edges[pslg->edge_count] = malloc(2 * sizeof(int));
    if(!pslg->edges[pslg->edge_count])
    {
        return FAILURE;
    }
    pslg->edges[pslg->edge_count + 1] = malloc(2 * sizeof(int));
    if(!pslg->edges[pslg->edge_count + 1])
    {
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

int free_triangulation(Triangulation* triangulation)
{
    for(int i = 0; i < triangulation->triangle_count; i++)
    {
        free(triangulation->triangles[i]);
    }
    free(triangulation->triangles);
    free(triangulation);
    return SUCCESS;
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

int merge_triangulations(Triangulation** triangulations, int tri_count, Triangulation* result)
{
    Triangulation* e = empty_triangulation();
    if (e == NULL)
    {
        return FAILURE;
    }
    result->triangle_count = e->triangle_count;
    result->triangles = e->triangles;
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
            int out = add_triangle(
                result, 
                triangulations[i]->triangles[j][0],
                triangulations[i]->triangles[j][1],
                triangulations[i]->triangles[j][2]
            );
            if (out == FAILURE)
            {
                return FAILURE;
            }
        }
    }
    return SUCCESS;
}

// a culmination of a lot of this project
int triangulate_polyhedra(Polyhedron* poly, Triangulation* result)
{
    Triangulation** tris = malloc(poly->face_count * sizeof(Triangulation*));
    if (!tris)
    {
        return FAILURE;
    }
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
        generate_triangulation(vertices, poly->face_sizes[i], t);
        free(vertices);
        tris[i] = t;
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

Polyhedron* polyhedra_from_off(FILE* fin)
{
    int nv, nf;
    read_off_header(fin, &nv, &nf);
    Polyhedron* poly = create_polyhedron(nv, nf);
    read_vertices(fin, poly);
    read_faces(fin, poly);
    return poly;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {   
        case WM_SIZE:
        {      
            int width  = LOWORD(lParam);
            int height = HIWORD(lParam);
            if (height == 0) // w/h
            {
                height = 1; 
            }
            reshape_fixed_ratio(width, height);
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void SetupPixelFormat(HDC hdc)
{
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = g_szClassName;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassEx(&wc)) return 0;

    HWND hwnd = CreateWindowEx(
        0,
        g_szClassName,
        "Window lol",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL);
    HDC hdc = GetDC(hwnd);      
    SetupPixelFormat(hdc);       
    HGLRC hglrc = wglCreateContext(hdc); 
    wglMakeCurrent(hdc, hglrc);  


    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    float t = 0;
    mciSendString("open \"../media/audio/music/untitled.mp3\" type mpegvideo alias mymusic", NULL, 0, NULL);
    mciSendString("play mymusic repeat", NULL, 0, NULL);
    while (1)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                goto cleanup;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        t+=0.0001;
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBegin(GL_TRIANGLES);
        glColor3f(1,0,0); 
        glVertex3f(t,-0.5f,0.0f);
        glColor3f(0,t,0); 
        glVertex3f(0.5f,-0.5f*t,0.0f);
        glColor3f(0,0,1); 
        glVertex3f(0.0f,0.5f,0.0f);
        glEnd();
        SwapBuffers(hdc);
    }
    cleanup:
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hglrc);
        ReleaseDC(hwnd, hdc);
    return 0;
}
