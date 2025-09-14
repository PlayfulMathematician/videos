/// @file main.c
/// @brief Core single-file engine for polyhedra animation.
/// @details
/// Sections:
/// - OpenGL/SDL2 runtime (Win32 + OpenGL)
/// - PSLG generation and splitting
/// - Triangulation storage & merge helpers
/// - OFF parsing
/// - Animation structs & callbacks
///
/// Most functions return a status code; errors are those whose high byte
/// maps to NONFATAL or FATAL (see macros).
///
/// @author
///   PlayfulMathematician
/// @version 0.1
/// @date 2025-09-03
/// @license
///   AGPL-3.0-or-later
/* SPDX-License-Identifier: AGPL-3.0-or-later */

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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_opengl.h>
#ifdef __APPLE__
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif
/// @def max
/// @brief The maximizer
#ifndef max
    #define max(a, b) (((a > b)) ? (a) : (b))
#endif

/// @def EPSILON
/// @brief Tolerance for floating-point comparisons.
#define EPSILON 0.000001

/// @def null
/// @brief I do not want to capitalize NULL
#define null ((void*)0)
#undef NULL

/// @def LINE_LENGTH
/// @brief The Length of a line 
#define LINE_LENGTH 1024

/// @def BIT_IGNORE
/// @brief Alignment granularity in bits (round up to 2^BIT_IGNORE).
#define BIT_IGNORE 4
/// @def BIT_ALIGN(x)
/// @brief Round x up to nearest aligned multiple.
#define BIT_ALIGN(x) max((((x) + ((1 << BIT_IGNORE) - 1)) & ~((1 << BIT_IGNORE) - 1)),1)

/// @def REALIGN(a,b)
/// @brief True if a and b land in different aligned capacity buckets.
#define REALIGN(a, b) BIT_ALIGN((a)) != BIT_ALIGN((b)) 

// status code helpers
#define STATUS_TYPE_SHIFT 24
#define STATUS_TYPE_MASK  0xFF000000  
#define STATUS_INFO_MASK  0x00FFFFFF  

/// @brief Operation succeeded.
#define SUCCESS 0x00
/// @brief No operation performed.
#define NOOP 0x01
/// @brief Non-fatal error.
#define NONFATAL 0x02
/// @brief Fatal error.
#define FATAL 0x03

/// @brief Extracts the status type (SUCCESS/NOOP/NONFATAL/FATAL).
#define STATUS_TYPE(code) (((code) & STATUS_TYPE_MASK) >> STATUS_TYPE_SHIFT)

/// @brief True if code is an error (NONFATAL or FATAL).
#define IS_AN_ERROR(x) ((STATUS_TYPE((x)) == FATAL) || (STATUS_TYPE((x)) == NONFATAL))

// Error codes
#define TRI_INIT_MALLOC_FAIL 0x03000000   ///< Triangulation malloc failed.
#define TRI_NOT_FOUND 0x03000001          ///< Triangulation pointer was NULL.
#define ADDING_TRI_REALLOC_FAILURE 0x03000002 ///< Realloc failed while adding triangle.
#define PSLG_INIT_MALLOC_ERROR 0x03000003     ///< PSLG struct allocation failed.
#define PSLG_VERTEX_MALLOC_ERROR 0x03000004   ///< Vertex allocation failed.
#define PSLG_EDGE_MALLOC_ERROR 0x03000005     ///< Edge allocation failed.
#define PSLG_EDGE_SPLIT_VERTEX_REALLOC_ERROR 0x03000006 ///< Realloc failed in split (vertices).
#define PSLG_EDGE_SPLIT_EDGE_REALLOC_ERROR 0x03000007   ///< Realloc failed in split (edges).
#define PSLG_TRIANGULATION_INIT_MALLOC_ERROR 0x03000008 ///< Coupled PSLG+tri malloc failed.
#define PSLG_ATTACK_TEMP_EDGES_MALLOC_ERROR 0x03000009  ///< Attack: malloc for temp edges failed.
#define PSLG_ATTACK_EDGE_REALLOCATION_ERROR 0x0300000a  ///< Attack: realloc failed on edges.
#define TRIANGULATE_POLYHEDRON_BATCH_TRIANGULATIONS_MALLOC_ERROR 0x0300000b ///< Allocating an array of triangulation pointers failed when triangulation a polyhedron.
#define TRIANGULATE_POLYHEDRON_VERTEX_MALLOC_ERROR 0x0300000c ///< When triangulating the polyhedron, allocating memory for vertices failed
#define POLYHEDRON_MALLOC_ERROR 0x0300000d ///< When allocating memory for polyhedron, malloc failed
#define POLYHEDRON_VERTEX_MALLOC_ERROR 0x0300000e ///< When allocating memory for the vertices of a polyhedron, malloc failed
#define POLYHEDRON_FACE_MALLOC_ERROR 0x0300000f ///< When allocating memory for the faces of a polyhedron, malloc failed
#define POLYHEDRON_FACE_SIZES_MALLOC_ERROR 0x03000010 ///< When allocating memory for the face sizes of the polyhedron, malloc failed
#define FILE_NO_CLEAN_LINE_OFF_ERROR 0x03000011 ///< When reading an off file, it could not find a clean line
#define OFF_HEADER_OFF_ERROR 0x03000012 ///< When reading an off file the first part of the off header was absent
#define OFF_HEADER_DATA_ERROR 0x03000013 ///< When reading an off file the second part of the off header was absent
#define OFF_VERTEX_ERROR 0x03000014 ///< Reading a vertex of an off file failed
#define OFF_FACE_ERROR 0x03000015 ///< Reading a face of an off file failed
#define DEDUP_PSLG_VERTEX_REALLOC_ERROR 0x03000016 ///< When deduplicating pslg vertices memory reallocation failed.
#define DEDUP_PSLG_EDGES_REALLOC_ERROR 0x03000017 ///< When deduplicating pslg edges memory reallocation failed.
#define STL_HEADER_WRITE_ERROR 0x03000018 ///< When writing to header of stl, writing failed
#define STL_VECTOR_WRITE_ERROR 0x03000019 ///< When writing to vector of stl, writing failed
#define RGB_BUFFER_MALLOC_ERROR 0x0300001a ///< When allocating a rgb buffer, malloc failed
#define LOAD_OPENGL_FUNCTION_ERROR 0x0300001b ///< If an OPENGL function fails we are screwed
#ifdef _WIN32
  #define POPEN  _popen
  #define PCLOSE _pclose
#else
  #define POPEN  popen
  #define PCLOSE pclose

/** 
 * @brief Print out the error 
 * @param error The error the need be printed.
 * @return I will not return anything 
*/

void print_error(int error)
{
    if (!IS_AN_ERROR(error))
    {
        return;
    }
    switch(error)
    {
        case TRI_INIT_MALLOC_FAIL:
            fprintf(stderr, "Allocating memory when initializing a triangle failed.\n");
            break;
        case TRI_NOT_FOUND:
            fprintf(stderr, "When running add_triangulation, you inputted a NULL pointer which is very mean of you.\n");
            break;
        case ADDING_TRI_REALLOC_FAILURE:
            fprintf(stderr, "When adding a triangle to a triangulation (running add_triangle) we failed at reallocating the memory.\n");
            break;
        case PSLG_INIT_MALLOC_ERROR:
            fprintf(stderr, "When mallocing the PSLG, I failed.\n");
            break;
        case PSLG_VERTEX_MALLOC_ERROR:
            fprintf(stderr, "Allocating the vertices for the PSLG failed.\n");
            break;
        case PSLG_EDGE_MALLOC_ERROR:
            fprintf(stderr, "Allocating the edges for the PSLG failed\n");
            break;
        case PSLG_EDGE_SPLIT_VERTEX_REALLOC_ERROR:
            fprintf(stderr, "When splitting a pslg at two edges, reallocating the space for the vertices failed.\n");
            break;
        case PSLG_EDGE_SPLIT_EDGE_REALLOC_ERROR:
            fprintf(stderr, "When splitting a pslg at two edges, reallocating the space for the edge failed.\n");
            break;
        case PSLG_TRIANGULATION_INIT_MALLOC_ERROR:
            fprintf(stderr, "When creating a pslg triangulation, allocating memory for the pslg triangulation failed.\n");
            break;
        case PSLG_ATTACK_TEMP_EDGES_MALLOC_ERROR:
            fprintf(stderr, "When attacking a PSLG triangulation, using malloc to allocate temporary edge list failed.\n");
            break;
        case PSLG_ATTACK_EDGE_REALLOCATION_ERROR:
            fprintf(stderr, "This shouldn't happen, but in the rare case it does, shrinking the memory during a PSLG vertex attack somehow failed.\n");
            break;
        case TRIANGULATE_POLYHEDRON_BATCH_TRIANGULATIONS_MALLOC_ERROR:
            fprintf(stderr, "When allocating a bunch an array of triangulation pointers, Memory allocating failed\n");
            break;
        case TRIANGULATE_POLYHEDRON_VERTEX_MALLOC_ERROR:
            fprintf(stderr, "When allocating vertices during polyhedron triangulation, malloc failed.\n");
            break;
        case POLYHEDRON_MALLOC_ERROR:
            fprintf(stderr,"When allocating a polyhedron, malloc failed\n");
            break;
        case POLYHEDRON_VERTEX_MALLOC_ERROR:
            fprintf(stderr, "When allocating the vertices of the polyhedron, malloc failed\n");
            break;
        case POLYHEDRON_FACE_MALLOC_ERROR:
            fprintf(stderr,"When allocating the faces of the polyhedron, malloc failed\n");
            break;
        case POLYHEDRON_FACE_SIZES_MALLOC_ERROR:
            fprintf(stderr, "When allocating the face sizes of the polyhedron, malloc failed\n");
            break;
        case FILE_NO_CLEAN_LINE_OFF_ERROR:
            fprintf(stderr, "When reading an off file and trying to find a clean line, there was non.\n");
            break;
        case OFF_HEADER_OFF_ERROR:
            fprintf(stderr, "When reading and OFF file the \"OFF\" part of the header was excluded\n");
            break;
        case OFF_HEADER_DATA_ERROR:
            fprintf(stderr, "When reading the actaul data of an off file header, (so vertex count and edge count) it was missing. \n");
            break;
        case OFF_VERTEX_ERROR:
            fprintf(stderr, "When reading a vertex from an off file something went wrong\n");
            break;
        case OFF_FACE_ERROR:
            fprintf(stderr, "When reading a face from an off file something went wrong\n");
            break;
        case DEDUP_PSLG_VERTEX_REALLOC_ERROR:
            fprintf(stderr, "When deduplicating the PSLG vertices, realloc failed\n");
            break;
        case DEDUP_PSLG_EDGES_REALLOC_ERROR:
            fprintf(stderr, "When deduplicating the PSLG edges, realloc failed\n");
            break;
        case STL_HEADER_WRITE_ERROR:
            fprintf(stderr, "When writing to the header of an stl, writing failed\n");
            break;
        case STL_VECTOR_WRITE_ERROR:
            fprintf(stderr, "When writing a vector to an stl, writing failed\n");
            break;
        case RGB_BUFFER_MALLOC_ERROR:
            fprintf(stderr, "When allocating an rgb buffer, malloc failed\n");
            break;
        case LOAD_OPENGL_FUNCTION_ERROR:
            fprintf(stderr, "Loading OpenGL function failed");
            break;
        default:
            fprintf(stderr, "SOMETHING BAD HAPPENED\n");
            break;
    } 
}

/**
 * @brief A 3 dimensional vector 
 */

typedef struct 
{
    /** 
     * @brief X Coordinate 
     * */
    float x;
    /** 
     * @brief Y Coordinate 
     * */
        float y; 
    /** 
     * @brief Z Coordinate 
     * */
    float z; 
} 
Vec3;

/**
 * @brief A polyhedron object. To serialize polyhedra.
 */

typedef struct 
{
    /**
     *  @brief The number of vertices
     *  */
    int vertex_count;
    /** 
     * @brief The number of faces
     *  */
    int face_count;
    /** 
     * @brief The vertices 
     * */
    Vec3* vertices;
    /** 
     * @brief The faces (stored as indices of the vertices) 
     * */
    int** faces;
    /** 
     * @brief The sizes of each face 
     * */
    int* face_sizes;
} 
Polyhedron;

/**
 * @brief A Point Set Linear Graph (pslg)
 */

typedef struct 
{
    /**
     *  @brief The vertices
     *  */
    Vec3* vertices;
    /**
     *  @brief The edges
     *  */
    int (*edges)[2]; // diametrically opposed foes, previously closed bros
    /**
     *  @brief The vertex count
     *  */
    int vertex_count;
    /**
     *  @brief The number of edges
     *  */
    int edge_count;
} 
PSLG;

/**
 * @brief A triangulation
 */

typedef struct
{
    /**
     *  @brief The triangles
     *  */
    Vec3 (*triangles)[3];
    /**
     *  @brief The number of triangles
     *  */
    int triangle_count;
}
Triangulation;

/**
 * @brief Literally just a PSLG and a Triangulation
 */

typedef struct
{
    /**
     *  @brief This is a PSLG
     *  */
    PSLG* pslg;
    /**
     *  @brief This is a triangulation
     *  */
    Triangulation* triangulation;
}
PSLGTriangulation;

/**
 * @brief This stores basically anything. 
 */

typedef struct
{
    /**
     * @brief This stores all of the data
     */
    void** data;
    /**
    * @brief How big is the dumpster;
    */
    int dumpster_size;
}
Dumpster;

typedef struct Animation Animation;
typedef struct GlobalBuffer GlobalBuffer;
typedef struct VideoData VideoData;  
typedef struct AnimationSection AnimationSection;  


/**
 * @brief This stores animations
 */

struct Animation
{   
    /**
     * @brief The start time of the animation (in frames)
     */
    int start_t;
    /**
     * @brief The end time of the animation (in frames)
     */
    int end_t;
    /**
     * @brief The construction function to be run on animation start
     */
    void (*construct)(struct Animation*);
    /**
     * @brief The function run before a frame is rendered.
     */
    void (*preproc)(struct Animation*, int t);
    /**
     * @brief The function run for rendering
     */
    void (*render)(struct Animation*, int t);
    /**
     * @brief The post processing function
     */
    void (*postproc)(struct Animation*, int t);
    /**
     * @brief The free function 
     * */
    void (*free)(struct Animation*);
    /**
     * @brief Put data here
     */
    Dumpster dumpster;
};

/**
 * @brief A group of Animations
 * @remark The animations individually can extend past the animation section end.
 */

struct AnimationSection
{
    /**
     * @brief The name of the animation section
     */
    char* name;
    /**
     * @brief The animations within the animation section
     */
    Animation* animations;
    /**
     * @brief The number of animations
     */
    int animation_count;
    /**
     * @brief The start of the animation
     */
    int start_t;
    /**
     * @brief The end of the animation
     */
    int end_t;
    /**
     * @brief Why is this here!?!
     * @remark Consider removing
     */
    void (*init)(struct AnimationSection*);
    /**
     * @brief A pointer an object that has a pointer to this object. 
     */
    VideoData* vd;
};

/**
 * @brief The video data
 */

struct VideoData
{
    /**
     * @brief The animation sections
     */
    AnimationSection* animation_section;
    /**
     * @brief The number of sections
     */
    int section_count;
    /**
     * @brief Backreference to Global Buffer
     */
    GlobalBuffer* gb;
};

/**
 * @brief The sound data
 */

typedef struct
{
    /**
     * @brief A list of lists of sounds.
     */
   char*** sounds;
   /**
     * @brief A list of the channel sound count.
     */
   int* sound_count;
   /**
    * @brief The list of lists of start times
    */
   float** start_t;
   /**
    * @brief The list of lists of end times.
    */
   float** end_t;
   /**
    * The number of channels
    */
   int channel_count;
   /**
    * Backreference to global buffer
    */
   GlobalBuffer* gb;
}
SoundData;

/**
 * @brief The most important class, it contains basically everything. 
 */

struct GlobalBuffer
{
    /**
     * @brief The sound data
     */
    SoundData* sounddata;
    /**
     * @brief The video data
     */
    VideoData* videodata;
};

/**
 * @brief A lighting object for OpenGL
 */
typedef struct
{
    /**
     * @brief The OpenGL light ID (GL_LIGHT0, GL_LIGHT1, etc.)
     */
    int id;

    /**
     * @brief The position of the light in world space.
     */
    Vec3 position;

    /**
     * @brief The ambient color contribution.
     */
    Vec3 ambient;

    /**
     * @brief The diffuse color contribution.
     */
    Vec3 diffuse;

    /**
     * @brief The specular color contribution.
     */
    Vec3 specular;

    /**
     * @brief Whether the light is enabled.
     */
    bool enabled;
}
Lighting;

PFNGLCREATESHADERPROC pglCreateShader;
PFNGLSHADERSOURCEPROC pglShaderSource;
PFNGLCOMPILESHADERPROC pglCompileShader;
PFNGLGETSHADERIVPROC pglGetShaderiv;
PFNGLGETSHADERINFOLOGPROC pglGetShaderInfoLog;
PFNGLCREATEPROGRAMPROC pglCreateProgram;
PFNGLATTACHSHADERPROC pglAttachShader;
PFNGLLINKPROGRAMPROC pglLinkProgram;
PFNGLGETPROGRAMIVPROC pglGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC pglGetProgramInfoLog;
PFNGLUSEPROGRAMPROC pglUseProgram;
PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation;
PFNGLUNIFORM1FPROC pglUniform1f;
PFNGLUNIFORM3FPROC pglUniform3f;
PFNGLDETACHSHADERPROC pglDetachShader;
PFNGLDELETESHADERPROC pglDeleteShader;
PFNGLDELETEPROGRAMPROC pglDeleteProgram;
PFNGLUNIFORM1IPROC pglUniform1i;
PFNGLUNIFORM2FPROC pglUniform2f;
PFNGLUNIFORM4FPROC pglUniform4f;
PFNGLUNIFORM1FVPROC pglUniform1fv;
PFNGLUNIFORM3FVPROC pglUniform3fv;
PFNGLUNIFORMMATRIX4FVPROC pglUniformMatrix4fv;
PFNGLGETATTRIBLOCATIONPROC pglGetAttribLocation;
PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray;
PFNGLDISABLEVERTEXATTRIBARRAYPROC pglDisableVertexAttribArray;
PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer;
PFNGLGENBUFFERSPROC pglGenBuffers;
PFNGLBINDBUFFERPROC pglBindBuffer;
PFNGLBUFFERDATAPROC pglBufferData;
PFNGLDELETEBUFFERSPROC pglDeleteBuffers;
PFNGLGENVERTEXARRAYSPROC pglGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC pglBindVertexArray;
PFNGLDELETEVERTEXARRAYSPROC pglDeleteVertexArrays;
PFNGLVALIDATEPROGRAMPROC pglValidateProgram;

#define LOAD_GL(type, var, name) do {var = (type)SDL_GL_GetProcAddress(name);  if (!(var)) {*result = LOAD_OPENGL_FUNCTION_ERROR; return; } } while(0)

/**
 * @brief load all opengl functions
 * @param result Did it succeed
 * @return nothing
 */

void load_gl_shader_functions(int* result)
{
    LOAD_GL(PFNGLCREATESHADERPROC, pglCreateShader, "glCreateShader");
    LOAD_GL(PFNGLSHADERSOURCEPROC, pglShaderSource, "glShaderSource");
    LOAD_GL(PFNGLCOMPILESHADERPROC, pglCompileShader, "glCompileShader");
    LOAD_GL(PFNGLGETSHADERIVPROC, pglGetShaderiv, "glGetShaderiv");
    LOAD_GL(PFNGLGETSHADERINFOLOGPROC, pglGetShaderInfoLog, "glGetShaderInfoLog");
    LOAD_GL(PFNGLCREATEPROGRAMPROC, pglCreateProgram, "glCreateProgram");
    LOAD_GL(PFNGLATTACHSHADERPROC, pglAttachShader, "glAttachShader");
    LOAD_GL(PFNGLLINKPROGRAMPROC, pglLinkProgram, "glLinkProgram");
    LOAD_GL(PFNGLGETPROGRAMIVPROC, pglGetProgramiv, "glGetProgramiv");
    LOAD_GL(PFNGLGETPROGRAMINFOLOGPROC, pglGetProgramInfoLog, "glGetProgramInfoLog");
    LOAD_GL(PFNGLUSEPROGRAMPROC, pglUseProgram, "glUseProgram");
    LOAD_GL(PFNGLGETUNIFORMLOCATIONPROC, pglGetUniformLocation, "glGetUniformLocation");
    LOAD_GL(PFNGLUNIFORM1FPROC, pglUniform1f, "glUniform1f");
    LOAD_GL(PFNGLUNIFORM3FPROC, pglUniform3f, "glUniform3f");
    LOAD_GL(PFNGLDETACHSHADERPROC, pglDetachShader, "glDetachShader");
    LOAD_GL(PFNGLDELETESHADERPROC, pglDeleteShader, "glDeleteShader");
    LOAD_GL(PFNGLDELETEPROGRAMPROC, pglDeleteProgram, "glDeleteProgram");
    LOAD_GL(PFNGLUNIFORM1IPROC, pglUniform1i, "glUniform1i");
    LOAD_GL(PFNGLUNIFORM2FPROC, pglUniform2f, "glUniform2f");
    LOAD_GL(PFNGLUNIFORM4FPROC, pglUniform4f, "glUniform4f");
    LOAD_GL(PFNGLUNIFORM1FVPROC, pglUniform1fv, "glUniform1fv");
    LOAD_GL(PFNGLUNIFORM3FVPROC, pglUniform3fv, "glUniform3fv");
    LOAD_GL(PFNGLUNIFORMMATRIX4FVPROC, pglUniformMatrix4fv, "glUniformMatrix4fv");
    LOAD_GL(PFNGLGETATTRIBLOCATIONPROC, pglGetAttribLocation, "glGetAttribLocation");
    LOAD_GL(PFNGLENABLEVERTEXATTRIBARRAYPROC, pglEnableVertexAttribArray, "glEnableVertexAttribArray");
    LOAD_GL(PFNGLDISABLEVERTEXATTRIBARRAYPROC, pglDisableVertexAttribArray, "glDisableVertexAttribArray");
    LOAD_GL(PFNGLVERTEXATTRIBPOINTERPROC, pglVertexAttribPointer, "glVertexAttribPointer");
    LOAD_GL(PFNGLGENBUFFERSPROC, pglGenBuffers, "glGenBuffers");
    LOAD_GL(PFNGLBINDBUFFERPROC, pglBindBuffer, "glBindBuffer");
    LOAD_GL(PFNGLBUFFERDATAPROC, pglBufferData, "glBufferData");
    LOAD_GL(PFNGLDELETEBUFFERSPROC, pglDeleteBuffers, "glDeleteBuffers");
    LOAD_GL(PFNGLGENVERTEXARRAYSPROC, pglGenVertexArrays, "glGenVertexArrays");
    LOAD_GL(PFNGLBINDVERTEXARRAYPROC, pglBindVertexArray, "glBindVertexArray");
    LOAD_GL(PFNGLDELETEVERTEXARRAYSPROC, pglDeleteVertexArrays, "glDeleteVertexArrays");
    LOAD_GL(PFNGLVALIDATEPROGRAMPROC, pglValidateProgram, "glValidateProgram");

    *result = SUCCESS;
}

/**
 * @brief Construct a light with default parameters.
 * @param id The Id
 * @param pos The position
 * @return Your light
 */

Lighting create_light(int id, Vec3 pos)
{
    Lighting l;
    l.id       = id;
    l.position = pos;
    l.ambient  = (Vec3){0.15f, 0.15f, 0.20f};
    l.diffuse  = (Vec3){0.90f, 0.90f, 0.90f};
    l.specular = (Vec3){0.80f, 0.80f, 0.80f};
    l.enabled  = true;
    return l;
}

/**
 * @brief apply it
 * @param l lighting
 * @return Nothing.
 */

void apply_light(Lighting* l)
{
    if (!l->enabled)
    {
        glDisable(l->id);
        return;
    }
    glEnable(GL_LIGHTING);
    glEnable(l->id);

    GLfloat ambient[4] = {l->ambient.x,  l->ambient.y,  l->ambient.z,  1.0f};
    GLfloat diffuse[4] = {l->diffuse.x,  l->diffuse.y,  l->diffuse.z,  1.0f};
    GLfloat specular[4] = {l->specular.x, l->specular.y, l->specular.z, 1.0f};
    GLfloat pos[4] = {l->position.x, l->position.y, l->position.z, 1.0f};
    glLightfv(l->id, GL_AMBIENT,  ambient);
    glLightfv(l->id, GL_DIFFUSE,  diffuse);
    glLightfv(l->id, GL_SPECULAR, specular);
    glLightfv(l->id, GL_POSITION, pos);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
}

/**
 * @brief This gets the frame buffer
 * @param[out] result The result value
 * @param w The width
 * @param h The height
 * @return the framebuffer
 */

unsigned char* get_framebuffer_rgb(int* result, int w, int h) 
{
    unsigned char* rgb = malloc(w * h * 3);
    if (!rgb) 
    {
        *result = RGB_BUFFER_MALLOC_ERROR;
        return null;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    *result = SUCCESS;
    return rgb;
}

/**
 * @brief This opens ffmpeg pipe
 * @param w Width
 * @param h Height
 * @param fps frames per second
 * @param out_mp4 The output mp4
 * @return The file
 */

FILE* open_ffmpeg_pipe(int w, int h, int fps, const char* out_mp4) 
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -f rawvideo -pixel_format rgb24 -video_size %dx%d -framerate %d -i - "
        "-vf vflip -c:v libx264 -preset veryfast -crf 18 -pix_fmt yuv420p \"%s\"",
        w, h, fps, out_mp4);
    return POPEN(cmd, "wb");
}

/**
 * @brief This closes ffmpeg pipe
 * @param pipef The pipe to close
 * @return nothing
 */

void close_ffmpeg_pipe(FILE* pipef) 
{
    if (pipef) 
    {
        PCLOSE(pipef);
    }
}


/**
 * @brief It outputs an empty triangulations
 * @param[out] result The result is set to all the goofy errors.
 * @return The triangulation
 */

Triangulation* empty_triangulation(int* result)
{
    Triangulation* tri = malloc(sizeof(Triangulation));
    if (!tri)
    {   
        *result = TRI_INIT_MALLOC_FAIL; // Soon that attitude will be your doom
        return null; 
    }
    tri->triangle_count = 0;
    tri->triangles = null;
    return tri;

} 

/**
 * @brief It adds a triangle to a triangulation
 * @param[out] result The result is set to all the goofy errors.
 * @param tri The triangulation that must have a new triangle appended to it
 * @param a The first vertex of the triangle
 * @param b The second vertex of the triangle
 * @param c The final vertex of the triangle 
 * @return Nothing
 */

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

/**
 * @brief It takes a collection of triangulations and merges them
 * @param[out] result The error code
 * @param triangulations The input triangulations
 * @param tri_count The number of triangulations
 * @param output A pointer to where the output will be stored
 * @return Nothing
 */

void merge_triangulations(int* result, Triangulation** triangulations, int tri_count, Triangulation* output)
{
    Triangulation* e = empty_triangulation(result);
    if (e == null)
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
            add_triangle(
                result,
                output, 
                triangulations[i]->triangles[j][0],
                triangulations[i]->triangles[j][1],
                triangulations[i]->triangles[j][2]
            );
            if (IS_AN_ERROR(*result))
            {
                return;
            }
        }
    }
    *result = SUCCESS;
}

/**
 * @brief This function frees a triangulation object
 * @param[out] result This is always SUCCESS because freeing will always happen 
 * @param triangulation This is the triangulation to be freed
 * @return nothing
 */

void free_triangulation( Triangulation* triangulation)
{
    free(triangulation->triangles);   
    triangulation->triangles = null;
    triangulation->triangle_count = 0;
    free(triangulation);              
}

/**
 * @brief This adds two vectors and returns the result
 * @param a The first vector to be added
 * @param b The second vector to be added
 * @return This is the output of their summation
 */

Vec3 add_vec3(Vec3 a, Vec3 b)
{
    Vec3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

/**
 * @brief This multiplies a vector by a scalar
 * @param a This is the vector
 * @param scalar This is a scalar
 * @return It returns the product of the vector and the scalar
 */

Vec3 multiply_vec3(Vec3 a, float scalar)
{
    Vec3 result;
    result.x = a.x * scalar;
    result.y = a.y * scalar;
    result.z = a.z * scalar;
    return result;
}

/**
 * @brief This subtracts two vectors and returns the result
 * @param a The vector to be subtracteed from
 * @param b The vector to be subtracted
 * @return This is the output of their subtraction so @p a - @p b
 */

Vec3 subtract_vec3(Vec3 a, Vec3 b)
{
    return add_vec3(a, multiply_vec3(b, -1.0f));
}

/**
 * @brief This interpolates between two vectors
 * @param a This is the starting vector
 * @param b This is the ending vector
 * @param t This is how much we interpolate
 * @return It returns lerp( @p a, @p b, @p t )
 */

Vec3 lerp_vec3(Vec3 a, Vec3 b, float t)
{
    return add_vec3(a, multiply_vec3(subtract_vec3(b, a), t));
}

/**
 * @brief 
 * @param v The vector to be subtracteed from
 * @return This outputs the magnitude of v
 */

float magnitude_vec3(Vec3 a)
{
    return sqrt(a.x*a.x + a.y*a.y + a.z*a.z);
}

/**
 * @brief This is the distance between two points
 * @param a the first one
 * @param b The second one
 * @return The distance
 */

float dist_vec3(Vec3 a, Vec3 b)
{
    return magnitude_vec3(subtract_vec3(a, b));
} 

/**
 * @brief This outputs whether or not two vectors are equal
 * @param a the first one
 * @param b The second one
 * @return a bool
 */

bool equal_vec3(Vec3 a, Vec3 b)
{
    return fabs(dist_vec3(a, b)) < EPSILON;
} 

/**
 * @brief This normalizes a vector
 * @param a The vector to be normalized
 * @return The normalization
 */

Vec3 normalize_vec3(Vec3 a)
{
    if (magnitude_vec3(a) < EPSILON)
    {
        Vec3 f = {0, 0, 0};
        return f;
    }
    return multiply_vec3(a, 1 / magnitude_vec3(a));
}

/**
 * @brief This takes the cross product fo two vectors
 * @param a the first one
 * @param b the second one
 * @return the normalization
 */

Vec3 cross_vec3(Vec3 a, Vec3 b)
{
    Vec3 r = 
    {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return r;
}

/**
 * @brief Generates normal vector 
 * @param a first vertex of the triangle
 * @param b The second vertex
 * @param c The third
 * @return A normal vector
 */

Vec3 normal_vec3(Vec3 a, Vec3 b, Vec3 c)
{
    Vec3 AB = subtract_vec3(b, a);
    Vec3 AC = subtract_vec3(c, a);
    Vec3 n  = cross_vec3(AB, AC);   
    return normalize_vec3(n);        
}

/**
 * @brief This checks if two segments are intersecting
 * @param a This parameter is the first vertex of the first edge
 * @param b This parameter is the second vertex of the first edge
 * @param c This parameter is the first vertex of the second edge
 * @param d This parameter is the second vertex of the second edge.
 * @param[out] out If the edges intersect, this outputs where they intersect.
 * @return This outputs 1 if they intersect but 0 if they don't
 */

int intersecting_segments(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3* out)
{    
    if (
        fabs(a.x-b.x) < EPSILON && 
        fabs(a.y-b.y) < EPSILON && 
        fabs(c.x-d.x) < EPSILON && 
        fabs(c.y-d.y) < EPSILON 
    )
    {
        return 0;
    }
    float tx; 
    float ty;
    int vertical = 0;
    float denomx;
    float denomy;
    if (
        fabs(a.x-b.x) < EPSILON && 
        fabs(a.y-b.y) < EPSILON 
    )
    {
        tx = (a.x - c.x);
        ty = (a.y - c.y);
        denomx = d.x - c.x;
        denomy = d.y - c.y;
        vertical = 1;
    }
    if (
        fabs(c.x - d.x) < EPSILON &&
        fabs(c.y - d.y) < EPSILON
    )   
    {
        tx = (c.x - a.x);
        ty = (c.y - a.y);
        denomx = b.x - a.x;
        denomy = b.x - a.y; // if you change this line of code, i will end you
        vertical = 2;
    }
    if (vertical > 0)
    {
        if (fabs(denomx) < EPSILON)
        {
            return 0;
        }
        if (fabs(denomy) < EPSILON)
        {
            return 0;
        }
        
        tx /= denomx;
        ty /= denomy;
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

/**
 * @brief This generates a PSLG from a polygon
 * @param[out] result This outputs the errors
 * @param vertices These are the vertices of the polygon
 * @param vertex_count This is the number of the vertices
 * @return This outputs the PSLG (or a pointer to it but nobody asked)
 */

PSLG* generate_pslg(int* result, Vec3* vertices, int vertex_count)
{
    PSLG* new = malloc(sizeof(PSLG));
    if(!new)
    {
        *result = PSLG_INIT_MALLOC_ERROR;
        return null;
    }
    new->vertex_count = vertex_count;
    new->edge_count = vertex_count;
    new->vertices = malloc(BIT_ALIGN(vertex_count) * sizeof(Vec3));
    if (!new->vertices) 
    {
        new->vertex_count = 0;
        new->edge_count = 0;
        new->edges = null;
        *result = PSLG_VERTEX_MALLOC_ERROR;
        return null;
    }
    for (int i = 0; i < vertex_count; i++)
    {
        new->vertices[i] = vertices[i];
    }

    new->edges = malloc(BIT_ALIGN(new->edge_count) * sizeof(int[2]));
    if (!new->edges) 
    {
        free(new->vertices);
        new->vertex_count = 0;
        new->edge_count = 0;
        *result = PSLG_EDGE_MALLOC_ERROR;
        return null;
    }

    for (int i = 0; i < vertex_count; i++)
    {
        new->edges[i][0] = i;
        new->edges[i][1] = (i + 1) % vertex_count; 
    }
    *result = SUCCESS;
    return new;
}

/**
 * @brief Deduplicates the pslg
 * @param[out] result
 * @param pslg the pslg
 * @param v1 the first vertex
 * @param v2 the second vertex
 * @return nothing
 */

void dedup_pslg_a_vertex(int* result, PSLG* pslg, int v1, int v2)
{
    if (v1 == v2)
    {
        *result = NOOP;
        return;
    }
    if (v1 > v2)
    {
        *result = NOOP;
        return;
    }
    Vec3 vec1 = pslg->vertices[v1];
    Vec3 vec2 = pslg->vertices[v2]; 
    if (!equal_vec3(vec1, vec2))
    {
        *result = NOOP;
        return;
    }    
    for (int i = v2; i < pslg->vertex_count - 1; i++)
    {
        pslg->vertices[i] = pslg->vertices[i + 1];
    }
    if (REALIGN(pslg->vertex_count, pslg->vertex_count - 1))
    {
        Vec3* temp = realloc(pslg->vertices, BIT_ALIGN(pslg->vertex_count - 1) * sizeof(Vec3));
        if (!temp)
        {
            *result = DEDUP_PSLG_VERTEX_REALLOC_ERROR;
            return;
        }
        pslg->vertices = temp;
    }
    pslg->vertex_count--;
    for (int i = 0; i < pslg->edge_count; i++)
    {
        if (pslg->edges[i][0] == v2)
        {
            pslg->edges[i][0] = v1;
        }
        if (pslg->edges[i][1] == v2)
        {
            pslg->edges[i][1] = v1;
        }
        if (pslg->edges[i][0] > v2)
        {
            pslg->edges[i][0]--;
        }
        if (pslg->edges[i][1] > v2)
        {
            pslg->edges[i][1]--;
        }
    }
    *result = SUCCESS;
}

/**
 * @brief This deduplicates a single vertex
 * @param[out] result This outputs the result
 * @param pslg This deduplicates a single vertex
 * @return nothing
 */

void dedup_pslg_one_vertex(int* result, PSLG* pslg)
{
    for (int i = 0; i < pslg->vertex_count; i++)
    {
        for (int j = 0; j < pslg->vertex_count; j++)
        {
            dedup_pslg_a_vertex(result, pslg, i, j);
            if (*result != NOOP)
            {
                return;
            }
        }
    }
    *result = NOOP;
}

/**
 * @brief This deduplicates all vertices
 * @param[out] result This outputs result
 * @param pslg The pslg
 * @return nothing
 */

void dedup_pslg_vertex(int* result, PSLG* pslg)
{
    for(;;)
    {
        dedup_pslg_one_vertex(result, pslg);
        if (*result == NOOP)
        {
            return;
        }
        if (IS_AN_ERROR(*result))
        {
            return;
        }
    }
}

/** 
 * @brief Deduplicates a single edge
 * @param[out] result This outputs result
 * @param pslg The pslg
 * @param e1 The first edge
 * @param e2 The second edge
 * @return Nothing
 */

void dedup_pslg_a_edge(int* result, PSLG* pslg, int e1, int e2)
{
    if (e1 > e2)
    {
        *result = NOOP;
        return;
    }
    if (e1 == e2)
    {
        *result = NOOP;
        return;
    }
    
    bool b = false;

    if (equal_vec3(pslg->vertices[pslg->edges[e1][0]], pslg->vertices[pslg->edges[e2][0]]))
    {
        if (equal_vec3(pslg->vertices[pslg->edges[e1][1]], pslg->vertices[pslg->edges[e2][1]]))
        {
            b = true;
        }
    }

    if (equal_vec3(pslg->vertices[pslg->edges[e1][0]], pslg->vertices[pslg->edges[e2][1]]))
    {
        if (equal_vec3(pslg->vertices[pslg->edges[e1][1]], pslg->vertices[pslg->edges[e2][0]]))
        {
            b = true;
        }
    }
    
    if(!b)
    {
        *result = NOOP;
        return;
    }

    
    for (int i = e2; i < pslg->edge_count - 1; i++)
    {
        pslg->edges[i][0] = pslg->edges[i + 1][0];
        pslg->edges[i][1] = pslg->edges[i + 1][1];
    }

    if (REALIGN(pslg->edge_count, pslg->edge_count-1))
    {
        int (*temp_ptr)[2] = realloc(pslg->edges, sizeof(int[2]) * BIT_ALIGN(pslg->edge_count - 1));
        if (!temp_ptr)
        {
            *result = DEDUP_PSLG_EDGES_REALLOC_ERROR;
            return;
        }

        pslg->edges = temp_ptr;
    }
    *result = SUCCESS;
    pslg->edge_count--;
}

/**
 * @brief Deduplicates the pslg, only one edge
 * @param[out] result The result
 * @param pslg The PSLG to be deduplicated
 * @return nothing
 */

void dedup_pslg_one_edge(int* result, PSLG* pslg)
{
    for (int i = 0; i < pslg->edge_count; i++)
    {
        for (int j = 0; j < pslg->edge_count; j++)
        {
            dedup_pslg_a_edge(result, pslg, i, j);
            if (*result != NOOP)
            {
                return;
            }
        }
    }
    *result = NOOP;
}

/**
 * @brief Deduplicates the pslg edges
 * @param[out] result The result
 * @param pslg The PSLG to be deduplicated
 * @return nothing
 */

void dedup_pslg_edge(int* result, PSLG* pslg)
{
    for(;;)
    {
        dedup_pslg_one_edge(result, pslg);
        if (*result == NOOP)
        {
            return;
        }
        if (IS_AN_ERROR(*result))
        {
            return;
        }
    }
}

/**
 * @brief Deduplicates the entire pslg
 * @param[out] result The result
 * @param pslg The pslg to be deduplicated
 * @return nothing
 */

void dedup_pslg(int *result, PSLG* pslg)
{
    dedup_pslg_vertex(result, pslg);
    if (IS_AN_ERROR(*result))
    {
        return;
    }
    dedup_pslg_edge(result, pslg);
    if (IS_AN_ERROR(*result))
    {
        return;
    }
    *result = SUCCESS;
}

/**
 * @brief This takes two edges and splits a PSLG if neccesary
 * @param[out] result This outputs the errors and status
 * @param pslg This is the input pslg
 * @param edge1 This is the first edge
 * @param edge2 This is the second edge
 * @return This outputs nothing. 
 */


void splitPSLG(int* result, PSLG* pslg, int edge1, int edge2)
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
        *result = NOOP;
        return;
    }

    Vec3 v1 = pslg->vertices[pslg->edges[edge1][0]];
    Vec3 v2 = pslg->vertices[pslg->edges[edge1][1]];
    Vec3 v3 = pslg->vertices[pslg->edges[edge2][0]];
    Vec3 v4 = pslg->vertices[pslg->edges[edge2][1]];
    int intersection = 
        intersecting_segments(
            v1,
            v2,
            v3,
            v4,
            &out
        );
    if (intersection == 0)
    {
        *result = NOOP;
        return;
    }

    if (REALIGN(pslg->vertex_count, pslg->vertex_count + 1))
    {
        Vec3* temp_ptr = realloc(pslg->vertices, sizeof(Vec3) * BIT_ALIGN(pslg->vertex_count + 1));
        if (temp_ptr == null)
        {
            *result = PSLG_EDGE_SPLIT_VERTEX_REALLOC_ERROR;
            return;
        }
        pslg->vertices = temp_ptr;  
    }
    if (REALIGN(pslg->edge_count, pslg->edge_count + 2))
    {
        int (*temp_ptr)[2] = realloc(pslg->edges, sizeof(int[2]) * BIT_ALIGN(pslg->edge_count + 2));
        if (temp_ptr == null)
        {
            *result = PSLG_EDGE_SPLIT_EDGE_REALLOC_ERROR;
            return;
        }
        pslg->edges = temp_ptr;
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
    *result = SUCCESS;
}

/**
 * @brief This removes a single edge
 * @param[out] result This is the status
 * @param pslg This is the PSLG that needs an edge to be removed 
 * @return This outputs nothing 
 */

void remove_single_edge(int* result, PSLG* pslg)
{
    for(int i = 0; i < pslg->edge_count; i++)
    {
        for(int j = 0; j < pslg->edge_count; j++)
        {
            splitPSLG(result, pslg, i, j);
            if(*result != NOOP)
            {
                return;
            }
        }
    }
    *result = NOOP;
}

/**
 * @brief This splits it entirely
 * @param[out] result The status
 * @param pslg The pslg that need be split
 * @return This outputs nothing 
 */

void split_entirely(int* result, PSLG* pslg)
{
    for(;;)
    {
        int ne = pslg->edge_count;
        int nv = pslg->vertex_count;
        remove_single_edge(result, pslg);
    
        if(*result == NOOP) 
        {
            *result = SUCCESS;
            return;
        }
        if(IS_AN_ERROR(*result))
        {
            return;
        }
        dedup_pslg(result, pslg);
        if(IS_AN_ERROR(*result))
        {
            return;
        }
        
        if (pslg->edge_count == ne)
        {
            if (pslg->vertex_count == nv)
            {
                return;
            }
        }
    }
}

/**
 * @brief This frees a pslg
 * @param pslg This is the PSLG that need be freed.
 */

void free_pslg(PSLG* pslg)
{
    // if this function fails that would funny lol, but that isn't happening
    free(pslg->edges);
    free(pslg->vertices);
    free(pslg);
}

/**
 * @brief This takes a pslg and makes an empty triangulation and merges them
 * @param[out] result This is the status
 * @param pslg This is the pslg that we use to construct the PSLGTriangulation
 * @return This is the resulting PSLGTriangulation
 */

PSLGTriangulation* create_pslg_triangulation(int* result, PSLG* pslg)
{
    PSLGTriangulation* pslgtri = malloc(sizeof(PSLGTriangulation));
    if (!pslgtri)
    {
        *result = PSLG_TRIANGULATION_INIT_MALLOC_ERROR;
        return null;
    }
    pslgtri->triangulation = empty_triangulation(result);
    if (IS_AN_ERROR(*result))
    {
        free(pslgtri);
        return null;
    }
    pslgtri->pslg = pslg;
    *result = SUCCESS;
    return pslgtri;
}

/**
 * @brief This attacks a vertex
 * @param[out] result This is the status
 * @param pslgtri The PSLGTriangulation that has the vertex to be attacked
 * @param vertex_idx The index of the vertex
 * @return Nothing
 */

void attack_vertex(int* result, PSLGTriangulation* pslgtri, int vertex_idx)
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
                *result = NOOP;
                return;
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
        *result = NOOP;
        return;
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
    add_triangle(result, tri, pslg->vertices[v1], pslg->vertices[v2], pslg->vertices[v3]);
    if (IS_AN_ERROR(*result))
    {
        return;
    }
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
    int ecount = e3_exists ? pslg->edge_count - 2 : pslg->edge_count - 1;
    int (*temp)[2] = malloc(ecount * sizeof(int[2])); // this isn't aligned because it is gonna DIE SOON.
    if (temp == null)
    {

        *result = PSLG_ATTACK_TEMP_EDGES_MALLOC_ERROR;
        return;
    }

    int EI = 0;
    for (int i = 0; i < pslg->edge_count; i++)
    {
        // sacrifice e1, and e2
        if (i == e1 || i == e2)
        {
            continue;
        }
        temp[EI][0] = pslg->edges[i][0];
        temp[EI][1] = pslg->edges[i][1];

        EI++;
    }

    // so e3_exists
    // handle it
    if (!e3_exists)
    {
        temp[EI][0] = v1;
        temp[EI][1] = v2;
    }
    if (REALIGN(pslg->edge_count, ecount))
    {
        int (*temp_ptr)[2] = realloc(pslg->edges, BIT_ALIGN(ecount) * sizeof(int[2]));
        
        if (temp_ptr == null)
        {
            printf("FAIL: %i\n", BIT_ALIGN(ecount));

            *result = PSLG_ATTACK_EDGE_REALLOCATION_ERROR;
            free(temp);
            return;
        }
        printf("WIN: %i\n", BIT_ALIGN(ecount));

        pslg->edges = temp_ptr;
    }
    pslg->edge_count = ecount;
    // time to populate the data
    for (int i = 0; i < ecount; i++)
    {
        pslg->edges[i][0] = temp[i][0];
        pslg->edges[i][1] = temp[i][1];
    }
    free(temp); // this is temporary
    *result = SUCCESS;
}

/**
 * @brief This attacks a single vertex
 * @param[out] result This is the status
 * @param pslgtri This is the PSLGTriangulation to attack
 * @return nothing
 */

void attack_single_vertex(int* result, PSLGTriangulation* pslgtri)
{
    for(int i = 0; i < pslgtri->pslg->vertex_count; i++)
    {
        attack_vertex(result, pslgtri, i);
        if (*result == SUCCESS)
        {
            return;
        }
        if(*result == NOOP)
        {
            continue;
        }
        return;
    }
    *result = NOOP;
}

/**
 * @brief This attacks a all vertices
 * @param[out] result This is the status
 * @param pslgtri This is the PSLGTriangulation to attack
 * @return nothing
 */

void attack_all_vertices(int* result, PSLGTriangulation* pslgtri)
{
    for(;;)
    {
        attack_single_vertex(result, pslgtri);
        if(*result == NOOP)
        {
            *result = SUCCESS;
            return;
        }
        if (IS_AN_ERROR(*result))
        {
            return; // the error propagates another level
        }
    }
}

/**
 * @brief This generates a triangulation of a polygon
 * @param[out] result This is the status
 * @param vertices These are the vertices
 * @param vertex_count This is the amount of vertices
 * @param tri This is where the triangulation will be stored.
 * @return output nothing
 */

void generate_triangulation(int* result, Vec3* vertices, int vertex_count, Triangulation* tri)
{
    printf("I HATE YOU\n");
    PSLG* pslg = generate_pslg(result, vertices, vertex_count);
    if (IS_AN_ERROR(*result))
    {
        goto cleanup;
        return;
    }
    split_entirely(result, pslg);
    if (IS_AN_ERROR(*result))
    {
        goto cleanup;
        return;
    }
    PSLGTriangulation* pslgtri = create_pslg_triangulation(result, pslg);
    if (IS_AN_ERROR(*result))
    {
        goto cleanup;
        return;
    }
    attack_all_vertices(result, pslgtri);
    if (IS_AN_ERROR(*result))
    {
        goto cleanup;
        return;
    }

    for(int i = 0; i < pslgtri->triangulation->triangle_count; i++)
    {
        add_triangle(result, tri, 
            pslgtri->triangulation->triangles[i][0],
            pslgtri->triangulation->triangles[i][1],
            pslgtri->triangulation->triangles[i][2]
        );
        if (IS_AN_ERROR(*result))
        {
            goto cleanup;
            return;
        }
    }
    tri->triangle_count = pslgtri->triangulation->triangle_count;

    free_pslg(pslgtri->pslg);
    free_triangulation(pslgtri->triangulation);
    free(pslgtri);
    *result = SUCCESS;
    printf("%i \n", tri->triangle_count);
    return;
    cleanup:
    {
        print_error(*result);
        printf("GRRR\n");
        if (pslg)
        {
            free_pslg(pslg);
        }
        if (pslgtri)
        {
            free_triangulation(pslgtri->triangulation);
            free(pslgtri);
        }
        if (tri)
        {
            free_triangulation(tri);
        }
    }
    return;
}

/**
 * @brief Triangulates the polyhedron
 * @param[out] result The status
 * @param poly The polyhedron to be triangulated
 * @param out Where the triangulation is stored
 * @return nothing
 */

void triangulate_polyhedron(int* result, Polyhedron* poly, Triangulation* out)
{
    Triangulation** tris = malloc(poly->face_count * sizeof(Triangulation*));
    if (!tris)
    {
        *result = TRIANGULATE_POLYHEDRON_BATCH_TRIANGULATIONS_MALLOC_ERROR;
        goto cleanup;
    }
    Triangulation* t;
    Vec3* vertices;
    for (int i = 0; i < poly->face_count; i++)
    {   
        t = empty_triangulation(result);
        if (IS_AN_ERROR(*result))
        {
            goto cleanup;
        }
        vertices = malloc(BIT_ALIGN(poly->face_sizes[i]) * sizeof(Vec3)); 
        if (!vertices)
        {
            *result = TRIANGULATE_POLYHEDRON_VERTEX_MALLOC_ERROR; // personality dialysis
            goto cleanup;

        }
        for (int j = 0; j < poly->face_sizes[i]; j++)
        {
            vertices[j] = poly->vertices[poly->faces[i][j]];
        }
        generate_triangulation(result, vertices, poly->face_sizes[i], t);
        if (IS_AN_ERROR(*result))
        {
            goto cleanup;
        }
        free(vertices);
        tris[i] = t;
    }
    merge_triangulations(result, tris, poly->face_count, out);
    if (IS_AN_ERROR(*result))
    {
        
        return;
    }
    for(int i = 0; i < poly->face_count; i++)
    {
        free_triangulation(tris[i]);
    }
    free(tris);
    *result = SUCCESS;
    return;
    cleanup:
        for (int i = 0 ; i < poly->face_count; i++)
        {
            free_triangulation(tris[i]);
        }
        free(tris);
        if (vertices)
        {
            free(vertices);
        }
        if (t)
        {
            free_triangulation(t);
        }
        if (out)
        {
            free_triangulation(out);
        }
    return;
        
}

/**
 * @brief This takes a polyhedron and frees it
 * @param poly This is the polyhedron to be freed
 * @return nothing
 */

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

/**
 * @brief This allocates memory for a polyhedron
 * @param[out] result This is the status of this function
 * @param nv The vertex count
 * @param nf The face count
 * @return A pointer to your brand new polyhedron
 */

Polyhedron* create_polyhedron(int* result, int nv, int nf) 
{
    Polyhedron* poly = malloc(sizeof(Polyhedron));
    if (!poly)
    {
        *result = POLYHEDRON_MALLOC_ERROR;
        goto clean;
    }
    poly->vertex_count = nv;
    poly->face_count = nf;
    poly->vertices = malloc(nv * sizeof(Vec3));
    if (!poly->vertices)
    {
        *result = POLYHEDRON_VERTEX_MALLOC_ERROR;
        goto clean;
    }
    poly->faces = malloc(nf * sizeof(int*));
    if (!poly->faces)
    {
        *result = POLYHEDRON_FACE_MALLOC_ERROR;
        goto clean; 
    }
    poly->face_sizes = malloc(nf * sizeof(int));
    if (!poly->face_sizes)
    {
        *result = POLYHEDRON_FACE_SIZES_MALLOC_ERROR;
        goto clean;
    }
    *result = SUCCESS;
    return poly;
    clean:
        free_polyhedron(poly);
        return (Polyhedron*)null;
}


/**
 * @brief This takes a global buffer and renders it
 * @param gb This is the global buffer to be rendered
 * @param t This is the frame index
 * @return This returns nothing
 */

void render_gb(GlobalBuffer* gb, int t)
{
    for(int i = 0; i < gb->videodata->section_count; i++)
    {
        AnimationSection* animation_section = &gb->videodata->animation_section[i];
        if (animation_section->end_t < t)
        {
            continue;
        }    
        if (animation_section->start_t > t)
        {
            continue;
        }    
        if (t == animation_section->start_t)
        {
            animation_section->init(animation_section);
        }
        if (t == animation_section->end_t)
        {
            free(animation_section->animations);
            free(animation_section);
            continue;
        }
        for (int j = 0; j < animation_section->animation_count; j++)
        {
            Animation* a = &animation_section->animations[j];
            if (a->start_t == t)
            {
                a->construct(a);
            }
            if (a->start_t <= t && a->end_t >= t)
            {
                a->preproc(a, t);
                a->render(a, t);
                a->postproc(a, t);
            }
            if (a->end_t == t)
            {
                a->free(a);
            }
        }
    }
}
/**
 * @todo Parse OFF Files
 */

/**
 * @brief Do not touch
 * @param[out] result You know
 * @param fin The file
 * @param buf The output
 * @return nothing

 */

void read_clean_line(int* result, FILE* fin, char* buf)
{
    for (;fgets(buf, LINE_LENGTH, fin);) 
    {
        buf[strcspn(buf, "\r\n")] = 0;             
        char* h = strchr(buf, '#'); 
        if (h) 
        {
            *h = 0;
        }
        char* p = buf + strspn(buf, " \t"); 
        size_t n = strlen(p);
        for (;n && (p[n-1]==' ' || p[n-1]=='\t');)
        {
            p[--n] = 0;
        }
        if (*p == 0) 
        {
            continue;            
        }         
        if (p != buf)
        {
            memmove(buf, p, n+1);  
        }
        *result = SUCCESS;
        return;
    }
    *result = FILE_NO_CLEAN_LINE_OFF_ERROR;
}

/**
 * @brief Reading off header
 * @param[out] result result
 * @param fin the file
 * @param nv The number of vertices to be written to
 * @param nf The number of faces
 * @return nothing
 */

void read_off_header(int* result, FILE* fin, int* nv, int* nf)
{
    char line[LINE_LENGTH];
    read_clean_line(result, fin, line);
    if (IS_AN_ERROR(*result))
    {
        return;
    }
    if (strcmp(line , "OFF") != 0)
    {
        *result = OFF_HEADER_OFF_ERROR;
        return;
    }
    read_clean_line(result, fin, line);
    if (IS_AN_ERROR(*result))
    {
        return;
    }
    char* t = strtok(line, " \t");
    if (!t) 
    {
        *result = OFF_HEADER_DATA_ERROR;
        return;
    }
    *nv = atoi(t);
    t = strtok(null, " \t");
    if (!t) 
    {
        *result = OFF_HEADER_DATA_ERROR;
        return;
    }
    *nf = atoi(t);
    *result = SUCCESS;
}

/**
 * @brief Reading vertex
 * @param[out] result result
 * @param fin the file
 * @param poly The polyhedron to write to
 * @param vertex_idx The index to write to
 * @return nothing
 */

void read_vertex(int* result, FILE* fin, Polyhedron* poly, int vertex_idx)
{
    char line[LINE_LENGTH];
    read_clean_line(result, fin, line);
    if (IS_AN_ERROR(*result))
    {
        return;
    }
    char* t = strtok(line, " \t");
    if (!t) 
    {
        *result = OFF_VERTEX_ERROR;
        return;
    }
    poly->vertices[vertex_idx].x = atof(t);
    t = strtok(null, " \t");
    if (!t) 
    {
        *result = OFF_VERTEX_ERROR;
        return;
    }
    poly->vertices[vertex_idx].y = atof(t);
    t = strtok(null, " \t");
    if (!t) 
    {
        *result = OFF_VERTEX_ERROR;
        return;
    }
    poly->vertices[vertex_idx].z = atof(t);
    *result = SUCCESS;
}

/**
 * @brief Reading faces
 * @param[out] result result
 * @param fin File
 * @param poly The polyhedron to write to
 * @param face_idx The index
 * @return nothing
 */

void read_face(int* result, FILE* fin, Polyhedron* poly, int face_idx)
{
    char line[LINE_LENGTH];
    read_clean_line(result, fin, line);
    if (IS_AN_ERROR(*result))
    {
        return;
    }
    char* t = strtok(line, " \t");
    if (!t) 
    {
        *result = OFF_FACE_ERROR;
        return;
    }
    poly->face_sizes[face_idx] = atoi(t);
    poly->faces[face_idx] = malloc(poly->face_sizes[face_idx] * sizeof(int));
    for (int i = 0; i < poly->face_sizes[face_idx]; i++)
    {
        t = strtok(null, " \t");
        if (!t) 
        {
            *result = OFF_FACE_ERROR;
            return;
        }
        poly->faces[face_idx][i] = atoi(t);
    }
    *result = SUCCESS;
}

/**
 * @brief Read OFF File and parse it into a polyhedron
 * @param[out] result The error code
 * @param fin
 * @return The polyhedron
 */

Polyhedron* read_off_into_polyhedron(int* result, FILE* fin)
{
    int nv = 0;
    int nf = 0;
    read_off_header(result, fin, &nv, &nf);
    if (IS_AN_ERROR(*result))
    {
        return null;
    }
    Polyhedron* poly = create_polyhedron(result, nv, nf);
    if (IS_AN_ERROR(*result))
    {
        return null;
    }
    for (int i = 0; i < nv; i++) 
    {
        read_vertex(result, fin, poly, i);
        if (IS_AN_ERROR(*result))
        {
            free_polyhedron(poly);
            return null;
        }
    }
    for (int i = 0; i < nf; i++) 
    {
        read_face(result, fin, poly, i);
        if (IS_AN_ERROR(*result))
        {
            free_polyhedron(poly);
            return null;
        }
    }
    return poly;
}
/**
 * @brief Convert a uint32 to little-endian format
 * @param v The number to convert
 * @param b The new version
 * @return nothing
 */

void le32_bytes(uint32_t v, unsigned char b[4]) 
{
    b[0] = (unsigned char)((v >> 0) & 0xFFu);
    b[1] = (unsigned char)((v >> 8) & 0xFFu);
    b[2] = (unsigned char)((v >> 16) & 0xFFu);
    b[3] = (unsigned char)((v >> 24) & 0xFFu);
}

/**
 * @brief Convert a float to little-endian format
 * @param v The number to convert
 * @param b The new version
 * @return nothing
 */

void lef32_bytes(float v, unsigned char b[4]) 
{
    le32_bytes(*(uint32_t*)&v, b);
}

/**
 * @brief Writes a triangulation into an stl
 * @param[out] result
 * @param tri The triangulation
 * @param file The file
 * @return nothing
 */

void write_to_stl(int* result, Triangulation* tri, FILE* fin)
{   
    unsigned char header[80];
    memset(header, 0, sizeof(header));
    if(fwrite(header, 1, 80, fin) != 80)
    {
        *result = STL_HEADER_WRITE_ERROR;
        return;
    }
    unsigned char h[4];
    le32_bytes((uint32_t)tri->triangle_count, h);
    if(fwrite(h, 1, 4, fin) != 4)
    {
        *result = STL_HEADER_WRITE_ERROR;
        return;
    }
    for (int i = 0; i < tri->triangle_count; i++)
    {
        Vec3 a = tri->triangles[i][0];
        Vec3 b = tri->triangles[i][1];
        Vec3 c = tri->triangles[i][2];
        Vec3 n = normal_vec3(a, b, c);
        lef32_bytes(n.x, h);
        if(fwrite(h, 1, 4, fin) != 4)
        {
            *result = STL_VECTOR_WRITE_ERROR;
            return;
        }
        lef32_bytes(n.y, h);
        if(fwrite(h, 1, 4, fin) != 4)
        {
            *result = STL_VECTOR_WRITE_ERROR;
            return;
        }
        lef32_bytes(n.z, h);
        if(fwrite(h, 1, 4, fin) != 4)
        {
            *result = STL_VECTOR_WRITE_ERROR;
            return;
        }
        lef32_bytes(a.x, h);
        if(fwrite(h, 1, 4, fin) != 4)
        {
            *result = STL_VECTOR_WRITE_ERROR;
            return;
        }
        lef32_bytes(a.y, h);
        if(fwrite(h, 1, 4, fin) != 4)
        {
            *result = STL_VECTOR_WRITE_ERROR;
            return;
        }
        lef32_bytes(a.z, h);
        if(fwrite(h, 1, 4, fin) != 4)
        {
            *result = STL_VECTOR_WRITE_ERROR;
            return;
        }
        lef32_bytes(b.x, h);
        if(fwrite(h, 1, 4, fin) != 4)
        {
            *result = STL_VECTOR_WRITE_ERROR;
            return;
        }
        lef32_bytes(b.y, h);
        if(fwrite(h, 1, 4, fin) != 4)
        {
            *result = STL_VECTOR_WRITE_ERROR;
            return;
        }
        lef32_bytes(b.z, h);
        if(fwrite(h, 1, 4, fin) != 4)
        {
            *result = STL_VECTOR_WRITE_ERROR;
            return;
        }
        lef32_bytes(c.x, h);
        if(fwrite(h, 1, 4, fin) != 4)
        {
            *result = STL_VECTOR_WRITE_ERROR;
            return;
        }
        lef32_bytes(c.y, h);
        if(fwrite(h, 1, 4, fin) != 4)
        {
            *result = STL_VECTOR_WRITE_ERROR;
            return;
        }
        lef32_bytes(c.z, h);
        if(fwrite(h, 1, 4, fin) != 4)
        {
            *result = STL_VECTOR_WRITE_ERROR;
            return;
        }
        unsigned char attr[2] = {0, 0};
        if(fwrite(attr, 1, 2, fin) != 2)
        {
            *result = STL_VECTOR_WRITE_ERROR;
            return;
        }
    }
    *result = SUCCESS;
}

/**
 * @brief Draw every triangle for die
 * @param tri triangulation
 * @return Nothing
 */

void draw_triangle(Triangulation tri)
{
    for(int i = 0; i < tri.triangle_count; i++)
    {
        Vec3 a = tri.triangles[i][0];
        Vec3 b = tri.triangles[i][1];
        Vec3 c = tri.triangles[i][2];


        Vec3 n = normal_vec3(a, b, c);
        glBegin(GL_TRIANGLES);
            glNormal3f(n.x, n.y, n.z);
            glVertex3f(a.x, a.y, a.z);
            glVertex3f(b.x, b.y, b.z);
            glVertex3f(c.x, c.y, c.z);
        glEnd();
    }
}


/**
 * @brief the main function lol
 * @param argc lol
 * @param argv lol
 * @return nothinf
 */

int main(int argc, char *argv[]) 
{
    if (argc != 2)
    {
        fprintf(stderr, "I wish for two parameters!");
        return 1;
    }

    int result = SUCCESS;
    FILE* fin = fopen(argv[1], "r");
    Polyhedron* poly = read_off_into_polyhedron(&result, fin);
    fclose(fin);
    

    if (IS_AN_ERROR(result))
    {
        print_error(result);
        return 1;
    }
    Triangulation* tri = empty_triangulation(&result);
    if (IS_AN_ERROR(result))
    {
        print_error(result);
        return 1;
    }

    triangulate_polyhedron(&result, poly, tri);
    if (IS_AN_ERROR(result))
    {
        print_error(result);
        return 1;
    }
    FILE* fin2 = fopen("e.stl", "wb");
    write_to_stl(&result, tri, fin2);
    if (IS_AN_ERROR(result))
    {
        print_error(result);
        return 1;
    }
    fclose(fin2);

    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window* win = SDL_CreateWindow("Canim",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    glEnable(0x809D);
    glEnable(GL_DEPTH_TEST);
    SDL_Event e;
    float angle = 0;
    int running = 1;
    Lighting light0 = create_light(GL_LIGHT0, (Vec3){0.0f, 0.0f, 1.0f});
    FILE* pipef = open_ffmpeg_pipe(800, 600, 60, "out.mp4");  
    if (!pipef) 
    {
        fprintf(stderr, "Failed to open ffmpeg pipe\n");
        return 1;
    }
    for (;running;) 
    {
        for (;SDL_PollEvent(&e);)
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
        angle+=0.15;
        glRotatef(angle, 1, 1, 0);   
        apply_light(&light0);
     

        draw_triangle(*tri);        

        SDL_GL_SwapWindow(win);
        int w = 800, h = 600;
        int frame_result;
        unsigned char* rgb = get_framebuffer_rgb(&frame_result, w, h);
        if (frame_result == SUCCESS && rgb) 
        {
            fwrite(rgb, 1, w*h*3, pipef);  
        }
        free(rgb);
    }
    close_ffmpeg_pipe(pipef);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
