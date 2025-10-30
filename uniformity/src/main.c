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

/* The standard libraries */
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* The SDL2 Libraries */
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_opengl.h>
/* Compression Algorithms */
#include <zlib.h>
/* Use OpenGL */
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
/// @def Dumpster
/// @brief This stores anything
typedef void *Dumpster;
/// @def CanimResult
/// @brief Stores the result of some event occuring
typedef uint32_t CanimResult;
/// @def max
/// @brief The maximizer
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

/// @def EPSILON
/// @brief Tolerance for floating-point comparisons.
#define EPSILON 0.000001

/// @def null
/// @brief I do not want to capitalize NULL
#define null NULL

/// @def BUFFER_SIZE
/// @brief The size of a buffer
#define BUFFER_SIZE 4096

/// @def BIT_IGNORE
/// @brief Alignment granularity in bits (round up to 2^BIT_IGNORE).
#define BIT_IGNORE 4

/// @def BIT_SIZE
/// @brief 2 ^ BIT_IGNORE - 1
#define BIT_SIZE ((1 << BIT_IGNORE) - 1)

/// @def BIT_ALIGN(x)
/// @brief Round x up to nearest aligned multiple.
#define BIT_ALIGN(x) max(((x) + BIT_SIZE) & ~BIT_SIZE, 1)

/// @def REALIGN(a,b)
/// @brief True if a and b land in different aligned capacity buckets.
#define REALIGN(a, b) BIT_ALIGN((a)) != BIT_ALIGN((b))

// status code helpers
#define STATUS_TYPE_SHIFT 24
#define STATUS_TYPE_MASK 0xFF000000
#define STATUS_INFO_MASK 0x00FFFFFF

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
#define IS_AN_ERROR(x)                                                         \
  ((STATUS_TYPE((x)) == FATAL) || (STATUS_TYPE((x)) == NONFATAL))

// Error codes
#define TRI_INIT_MALLOC_FAIL 0x03000000 ///< Triangulation malloc failed.
#define TRI_NOT_FOUND 0x03000001        ///< Triangulation pointer was NULL.
#define ADDING_TRI_REALLOC_FAILURE                                             \
  0x03000002 ///< Realloc failed while adding triangle.
#define PSLG_INIT_MALLOC_ERROR 0x03000003   ///< PSLG struct allocation failed.
#define PSLG_VERTEX_MALLOC_ERROR 0x03000004 ///< Vertex allocation failed.
#define PSLG_EDGE_MALLOC_ERROR 0x03000005   ///< Edge allocation failed.
#define PSLG_EDGE_SPLIT_VERTEX_REALLOC_ERROR                                   \
  0x03000006 ///< Realloc failed in split (vertices).
#define PSLG_EDGE_SPLIT_EDGE_REALLOC_ERROR                                     \
  0x03000007 ///< Realloc failed in split (edges).
#define PSLG_TRIANGULATION_INIT_MALLOC_ERROR                                   \
  0x03000008 ///< Coupled PSLG+tri malloc failed.
#define PSLG_ATTACK_TEMP_EDGES_MALLOC_ERROR                                    \
  0x03000009 ///< Attack: malloc for temp edges failed.
#define PSLG_ATTACK_EDGE_REALLOCATION_ERROR                                    \
  0x0300000a ///< Attack: realloc failed on edges.
#define TRIANGULATE_POLYHEDRON_BATCH_TRIANGULATIONS_MALLOC_ERROR               \
  0x0300000b ///< Allocating an array of triangulation pointers failed when
             ///< triangulation a polyhedron.
#define TRIANGULATE_POLYHEDRON_VERTEX_MALLOC_ERROR                             \
  0x0300000c ///< When triangulating the polyhedron, allocating memory for
             ///< vertices failed
#define POLYHEDRON_MALLOC_ERROR                                                \
  0x0300000d ///< When allocating memory for polyhedron, malloc failed
#define POLYHEDRON_VERTEX_MALLOC_ERROR                                         \
  0x0300000e ///< When allocating memory for the vertices of a polyhedron,
             ///< malloc failed
#define POLYHEDRON_FACE_MALLOC_ERROR                                           \
  0x0300000f ///< When allocating memory for the faces of a polyhedron, malloc
             ///< failed
#define POLYHEDRON_FACE_SIZES_MALLOC_ERROR                                     \
  0x03000010 ///< When allocating memory for the face sizes of the polyhedron,
             ///< malloc failed
#define FILE_NO_CLEAN_LINE_OFF_ERROR                                           \
  0x03000011 ///< When reading an off file, it could not find a clean line
#define OFF_HEADER_OFF_ERROR                                                   \
  0x03000012 ///< When reading an off file the first part of the off header was
             ///< absent
#define OFF_HEADER_DATA_ERROR                                                  \
  0x03000013 ///< When reading an off file the second part of the off header was
             ///< absent
#define OFF_VERTEX_ERROR 0x03000014 ///< Reading a vertex of an off file failed
#define OFF_FACE_ERROR 0x03000015   ///< Reading a face of an off file failed
#define DEDUP_PSLG_VERTEX_REALLOC_ERROR                                        \
  0x03000016 ///< When deduplicating pslg vertices memory reallocation failed.
#define DEDUP_PSLG_EDGES_REALLOC_ERROR                                         \
  0x03000017 ///< When deduplicating pslg edges memory reallocation failed.
#define STL_HEADER_WRITE_ERROR                                                 \
  0x03000018 ///< When writing to header of stl, writing failed
#define STL_VECTOR_WRITE_ERROR                                                 \
  0x03000019 ///< When writing to vector of stl, writing failed
#define RGB_BUFFER_MALLOC_ERROR                                                \
  0x0300001a ///< When allocating a rgb buffer, malloc failed
#define LOAD_OPENGL_FUNCTION_ERROR                                             \
  0x0300001b ///< If an OPENGL function fails we are screwed
#define OPENGL_SHADER_COMPILATION_ERROR                                        \
  0x0300001c ///< When compiling a shader an error occured
#define OPENGL_SHADER_PROGRAM_LINK_ERROR                                       \
  0x0300001d ///< When linking a shader program an error occured
#define DRAW_TRIANGULATION_MALLOC_ERROR                                        \
  0x0300001e ///< When drawing a trianglulation, malloc failed
#define TRI_CLONE_ERROR                                                        \
  0x0300001f ///< Malloc failed when cloning a triangulation
#define TRI_CLONE_TRI_ERROR                                                    \
  0x03000020 ///< Malloc failed when cloning the triangles in a triangulation
#define PDF_XREF_FIND_SEEK_END_ERROR                                           \
  0x03000021 ///< When trying to find the xref table in a pdf, fseek failed to
             ///< seek to the end
#define PDF_XREF_FIND_FTELL_ERROR                                              \
  0x03000022 ///< When trying to find the xref table in a pdf, ftell failed
#define PDF_XREF_FIND_SEEK_SET_ERROR                                           \
  0x03000023 ///< When trying to find the xref table in a pdf, fseek failed to
             ///< set the seek
#define PDF_XREF_STARTXREF_NOT_FOUND                                           \
  0x03000024 ///< When trying to find the xref table in a pdf, the string
             ///< startxref was not found
#define PDF_XREF_OFFSET_PARSE_ERROR                                            \
  0x03000025 ///< When trying to find the xref table in the pdf the offset could
             ///< not be properly parsed.
#define PDF_XREF_FIND_FREAD_ERROR                                              \
  0x03000026 ///< When trying to find the xref table in a pdf, fread failed
#define NEXT_STR_NOT_FOUND_ERROR                                               \
  0x03000027 ///< When using next_str, the character "\0" was not found
#define GET_XREF_FSEEK_ERROR 0x03000028 ///< When getting the xref, fseek failed
#define GET_XREF_FREAD_ERROR                                                   \
  0x03000029 ///< When getting the xref, fread failed.
#define GET_XREF_STRCHR_NEWLINE_FAIL                                           \
  0x0300002a ///< When getting xref, strchr to look at new line failed.
#define NON_XREF_XREF 0x0300002b ///< The type of the xref table is not an xref
#define GET_XREF_STREAM_SEEK_ERROR                                             \
  0x0300002c ///< When getting the xref seeking the stream failed
#define STREAM_DECOMPRESS_MALLOC_FAIL                                          \
  0x0300002d ///< Allocating memory for decompressed stream failed
#define STREAM_DECOMPRESS_INIT_FAIL                                            \
  0x0300002e ///< Initializing the stream decompression failed
#define STREAM_DECOMPRESS_FAIL 0x0300002f ///< Decompressing streams failed
#define GET_XREF_STREAM_MALLOC_ERROR                                           \
  0x03000030 ///< When getting the xref allocating memory to store the stream
             ///< failed
#define GET_XREF_STREAM_READ_ERROR                                             \
  0x03000031 ///< Reading the file to store the xref stream failed
#define GET_XREF_MALLOC_ERROR                                                  \
  0x03000032 ///< When getting the xref, allocating the PDFXref object failed
#define GET_XREF_TABLE_ENTRIES_MALLOC_ERROR                                    \
  0x03000033 ///< When getting the xref table entries, malloc failed

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#define FILENO _fileno
#define FTRUNCATE _chsize
#define WB "wb"
#else
#define POPEN popen
#define PCLOSE pclose
#define FILENO fileno
#define FTRUNCATE ftruncate
#define WB "w"
#endif

const char *triangulation_vs =
    "#version 120\n"
    "attribute vec3 position;\n"
    "attribute vec3 normal;\n"
    "attribute vec4 color;\n"
    "varying vec3 vNormal;\n"
    "varying vec3 vPos;\n"
    "varying vec4 vColor;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = gl_ModelViewProjectionMatrix * vec4(position, 1.0);\n"
    "   vPos = vec3(gl_ModelViewMatrix * vec4(position, 1.0));\n"
    "   vNormal = normalize(gl_NormalMatrix * normal);\n"
    "	vColor = color;\n"
    "}\n";

const char *triangulation_fs = "#version 120\n"
                               "varying vec3 vNormal;\n"
                               "varying vec3 vPos;\n"
                               "varying vec4 vColor;\n"
                               "void main()"
                               "{\n"
                               "   gl_FragColor = vColor;\n"
                               "}\n";

/**
 * @brief Print out the error
 * @param error The error the need be printed.
 * @return I will not return anything
 */

void print_error(CanimResult error) {
  if (!IS_AN_ERROR(error)) {
    return;
  }
  switch (error) {
  case TRI_INIT_MALLOC_FAIL:
    fprintf(stderr, "Allocating memory when initializing a triangle failed.\n");
    break;
  case TRI_NOT_FOUND:
    fprintf(stderr, "When running add_triangulation, you inputted a NULL "
                    "pointer which is very mean of you.\n");
    break;
  case ADDING_TRI_REALLOC_FAILURE:
    fprintf(stderr, "When adding a triangle to a triangulation (running "
                    "add_triangle) we failed at reallocating the memory.\n");
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
    fprintf(stderr, "When splitting a pslg at two edges, reallocating the "
                    "space for the vertices failed.\n");
    break;
  case PSLG_EDGE_SPLIT_EDGE_REALLOC_ERROR:
    fprintf(stderr, "When splitting a pslg at two edges, reallocating the "
                    "space for the edge failed.\n");
    break;
  case PSLG_TRIANGULATION_INIT_MALLOC_ERROR:
    fprintf(stderr, "When creating a pslg triangulation, allocating memory for "
                    "the pslg triangulation failed.\n");
    break;
  case PSLG_ATTACK_TEMP_EDGES_MALLOC_ERROR:
    fprintf(stderr, "When attacking a PSLG triangulation, using malloc to "
                    "allocate temporary edge list failed.\n");
    break;
  case PSLG_ATTACK_EDGE_REALLOCATION_ERROR:
    fprintf(stderr,
            "This shouldn't happen, but in the rare case it does, shrinking "
            "the memory during a PSLG vertex attack somehow failed.\n");
    break;
  case TRIANGULATE_POLYHEDRON_BATCH_TRIANGULATIONS_MALLOC_ERROR:
    fprintf(stderr, "When allocating a bunch an array of triangulation "
                    "pointers, Memory allocating failed\n");
    break;
  case TRIANGULATE_POLYHEDRON_VERTEX_MALLOC_ERROR:
    fprintf(stderr, "When allocating vertices during polyhedron triangulation, "
                    "malloc failed.\n");
    break;
  case POLYHEDRON_MALLOC_ERROR:
    fprintf(stderr, "When allocating a polyhedron, malloc failed\n");
    break;
  case POLYHEDRON_VERTEX_MALLOC_ERROR:
    fprintf(stderr,
            "When allocating the vertices of the polyhedron, malloc failed\n");
    break;
  case POLYHEDRON_FACE_MALLOC_ERROR:
    fprintf(stderr,
            "When allocating the faces of the polyhedron, malloc failed\n");
    break;
  case POLYHEDRON_FACE_SIZES_MALLOC_ERROR:
    fprintf(
        stderr,
        "When allocating the face sizes of the polyhedron, malloc failed\n");
    break;
  case FILE_NO_CLEAN_LINE_OFF_ERROR:
    fprintf(stderr, "When reading an off file and trying to find a clean line, "
                    "there was non.\n");
    break;
  case OFF_HEADER_OFF_ERROR:
    fprintf(stderr, "When reading and OFF file the \"OFF\" part of the header "
                    "was excluded\n");
    break;
  case OFF_HEADER_DATA_ERROR:
    fprintf(stderr, "When reading the actaul data of an off file header, (so "
                    "vertex count and edge count) it was missing. \n");
    break;
  case OFF_VERTEX_ERROR:
    fprintf(stderr,
            "When reading a vertex from an off file something went wrong\n");
    break;
  case OFF_FACE_ERROR:
    fprintf(stderr,
            "When reading a face from an off file something went wrong\n");
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
    fprintf(stderr, "Loading OpenGL function failed\n");
    break;
  case OPENGL_SHADER_COMPILATION_ERROR:
    fprintf(stderr, "When compiling a shader an error occured\n");
    break;
  case OPENGL_SHADER_PROGRAM_LINK_ERROR:
    fprintf(stderr, "When linking a shader program an error occured\n");
    break;
  case DRAW_TRIANGULATION_MALLOC_ERROR:
    fprintf(stderr, "When drawing a trianglulation, malloc failed\n");
    break;
  case TRI_CLONE_ERROR:
    fprintf(stderr,
            "When using malloc to clone a triangulation, malloc failed\n");
    break;
  case TRI_CLONE_TRI_ERROR:
    fprintf(stderr, "When using malloc to clone the triangles within a "
                    "triangulation, malloc failed\n");
    break;
  case PDF_XREF_FIND_SEEK_END_ERROR:
    fprintf(stderr, "When trying to find the xref table in a pdf, fseek failed "
                    "to seek to the end\n");
    break;
  case PDF_XREF_FIND_FTELL_ERROR:
    fprintf(stderr,
            "When trying to find the xref table in a pdf, ftell failed\n");
    break;
  case PDF_XREF_FIND_SEEK_SET_ERROR:
    fprintf(stderr, "When trying to find the xref table in a pdf, fseek failed "
                    "to set the seek\n");
    break;
  case PDF_XREF_STARTXREF_NOT_FOUND:
    fprintf(stderr, "When trying to find the xref table in a pdf, the string "
                    "startxref was not found\n");
    break;
  case PDF_XREF_OFFSET_PARSE_ERROR:
    fprintf(stderr, "When trying to find the xref table in the pdf the offset "
                    "could not be properly parsed.\n");
    break;
  case PDF_XREF_FIND_FREAD_ERROR:
    fprintf(stderr,
            "When trying to find the xref table in a pdf, fread failed\n");
    break;
  case NEXT_STR_NOT_FOUND_ERROR:
    fprintf(stderr,
            "When using next_str, the character \"\\0\" was not found \n");
    break;
  case GET_XREF_FSEEK_ERROR:
    fprintf(stderr, "When getting the xref, fseek failed\n");
    break;
  case GET_XREF_FREAD_ERROR:
    fprintf(stderr, "When getting the xref, fread failed.\n");
    break;
  case GET_XREF_STRCHR_NEWLINE_FAIL:
    fprintf(stderr, "When getting xref, strchr to look at new line failed.\n");
    break;
  case NON_XREF_XREF:
    fprintf(stderr, "The type of the xref table is not an xref\n");
    break;
  case GET_XREF_STREAM_SEEK_ERROR:
    fprintf(stderr, "When getting the xref seeking the stream failed\n");
    break;
  case STREAM_DECOMPRESS_MALLOC_FAIL:
    fprintf(stderr, "Allocating memory for decompressed stream failed\n");
    break;
  case STREAM_DECOMPRESS_INIT_FAIL:
    fprintf(stderr, "Initializing the stream decompression failed\n");
    break;
  case STREAM_DECOMPRESS_FAIL:
    fprintf(stderr, "Decompressing streams failed\n");
    break;
  case GET_XREF_STREAM_MALLOC_ERROR:
    fprintf(
        stderr,
        "When getting the xref allocating memory to store the stream failed\n");
    break;
  case GET_XREF_STREAM_READ_ERROR:
    fprintf(stderr, "Reading the file to store the xref stream failed\n");
    break;
  case GET_XREF_MALLOC_ERROR:
    fprintf(stderr,
            "When getting the xref, allocating the PDFXref object failed\n");
    break;
  case GET_XREF_TABLE_ENTRIES_MALLOC_ERROR:
    fprintf(stderr, "When getting the xref table entries, malloc failed\n");
    break;
  default:
    fprintf(stderr, "SOMETHING BAD HAPPENED, WE DON'T KNOW WHAT");
    break;
  }
}

/**
 * @brief A 3 dimensional vector
 */

typedef struct {
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
} Vec3;

/**
 * @brief The color
 */

typedef union {
  /**
   * @brief The Color BYTE Packed
   */
  uint32_t rgba;
  /**
   * @brief The color as a list, eg. v[1] is green component
   */
  uint8_t v[4];
  /**
   * @brief The color is a struct
   */
  struct {
    /**
     * @brief The red component
     */
    uint8_t r;
    /**
     * @brief The green component
     */
    uint8_t g;
    /**
     * @brief The Blue Component
     */
    uint8_t b;
    /**
     * @brief The Alpha component
     */
    uint8_t a;
  };
} Color;

/**
 * @brief The face data
 */

typedef struct {
  /**
   * @brief The color
   */
  Color color;
  /**
   * The normal of the face (normalized)
   */
  Vec3 normal;
} FaceData;

/**
 * @brief A triangle (raw, not references to index)
 */

typedef struct {
  /**
   * @brief The vertices
   */
  Vec3 vertices[3];
  /**
   * @brief The FaceData
   */
  FaceData fd;
} TriangleRaw;

/**
 * @brief A triangle but with references to indices
 */

typedef struct {
  /**
   * @brief The vertices as indices
   */
  int vertices[3];
  /**
   * @brief The FaceData
   */
  FaceData fd;
} TriangleIndexed;

/**
 * @brief A raw polygon (not indexed)
 */

typedef struct {
  /**
   * @brief The number of vertices
   */
  int vertex_count;
  /**
   * @brief The vertices
   */
  Vec3 *vertices;
  /**
   * @brief The facedata
   */
  FaceData fd;
} PolygonRaw;

/**
 * @brief An indexed polygon
 */

typedef struct {
  /**
   * @brief The number of vertices
   */
  int vertex_count;
  /**
   * @brief The vertices
   */
  int *vertices;
  /**
   * @brief The facedata
   */
  FaceData fd;
} PolygonIndexed;

/**
 * @brief A polyhedron object. To serialize polyhedra.
 */

typedef struct {
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
  Vec3 *vertices;
  /**
   * @brief The faces (stored as indices of the vertices)
   * */
  PolygonIndexed *poly;
} Polyhedron;

/**
 * @brief A Point Set Linear Graph (pslg)
 */

typedef struct {
  /**
   *  @brief The vertices
   *  */
  Vec3 *vertices;
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
  /**
   * @brief The old face
   */
  PolygonRaw poly;
} PSLG;

/**
 * @brief A triangulation
 */

typedef struct {
  /**
   *  @brief The triangles
   *  */
  TriangleRaw *triangles;
  /**
   *  @brief The number of triangles
   *  */
  int triangle_count;
} Triangulation;

/**
 * @brief A Quaternion
 */

typedef union {
  struct {
    /**
     * @brief The i component
     */
    float x;
    /**
     * @brief The j component
     */
    float y;
    /**
     * @brief The k component
     */
    float z;
    /**
     * @brief The scalar component
     */
    float w;
  };
  /**
   * @brief An array to store it
   */
  float v[4];
} Quaternion;

/**
 * @brief Literally just a PSLG and a Triangulation
 */

typedef struct {
  /**
   *  @brief This is a PSLG
   *  */
  PSLG *pslg;
  /**
   *  @brief This is a triangulation
   *  */
  Triangulation *triangulation;
} PSLGTriangulation;

typedef struct Animation Animation;
typedef struct GlobalBuffer GlobalBuffer;
typedef struct VideoData VideoData;
typedef struct AnimationSection AnimationSection;

/**
 * @brief This stores animations
 */

struct Animation {
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
  void (*construct)(CanimResult *, struct Animation *);
  /**
   * @brief The function run before a frame is rendered.
   */
  void (*preproc)(CanimResult *, struct Animation *, int);
  /**
   * @brief The function run for rendering
   */
  void (*render)(CanimResult *, struct Animation *, int);
  /**
   * @brief The post processing function
   */
  void (*postproc)(CanimResult *, struct Animation *, int);
  /**
   * @brief The free function
   * */
  void (*free)(CanimResult *, struct Animation *);
  /**
   * @brief Put data here
   */
  Dumpster dumpster;
};

/**
 * @brief A group of Animations
 * @remark The animations individually can extend past the animation section
 * end.
 */

struct AnimationSection {
  /**
   * @brief The name of the animation section
   */
  char *name;
  /**
   * @brief The animations within the animation section
   */
  Animation *animations;
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
   * @brief A pointer an object that has a pointer to this object.
   */
  VideoData *vd;
};

/**
 * @brief The video data
 */

struct VideoData {
  /**
   * @brief The animations
   */
  Animation *Animation;
  /**
   * @brief The number of animations
   */
  int animation_count;
  /**
   * @brief Backreference to Global Buffer
   */
  GlobalBuffer *gb;
};

/**
 * @brief The sound data
 */

typedef struct {
  /**
   * @brief A list of lists of sounds.
   */
  char ***sounds;
  /**
   * @brief A list of the channel sound count.
   */
  int *sound_count;
  /**
   * @brief The list of lists of start times
   */
  float **start_t;
  /**
   * @brief The list of lists of end times.
   */
  float **end_t;
  /**
   * The number of channels
   */
  int channel_count;
  /**
   * Backreference to global buffer
   */
  GlobalBuffer *gb;
} SoundData;

/**
 * @brief The most important class, it contains basically everything.
 */

struct GlobalBuffer {
  /**
   * @brief The sound data
   */
  SoundData *sounddata;
  /**
   * @brief The video data
   */
  VideoData *videodata;
};

/**
 * @brief One cross-reference entry.
 */

typedef struct {
  /**
   * @brief The type of the entry (0 = free, 1 = in-use, 2 = compressed)
   */
  int type;
  /**
   * @brief The file offset/object stream number
   */
  long offset;
  /**
   * @brief Generation number or index inside stream
   */
  int generation;
} PDFXrefEntry;

/**
 * @brief The full cross-reference table.
 */

typedef struct {
  /**
   * @brief The size of the Xref table
   */
  int size;
  /**
   * @brief The actual entries of the table
   */
  PDFXrefEntry *entries;
} PDFXrefTable;

/**
 * @brief The trailer info
 */

typedef struct {
  /**
   * @brief The size of the table
   */
  int size;
  /**
   * @brief Index of root object
   */
  int root_obj;
  /**
   * @brief The generation of the root object
   */
  int root_gen;
} PDFTrailer;

/**
 * @brief The PDF Xref
 */

typedef struct {
  /**
   * @brief The table
   */
  PDFXrefTable tb;
  /**
   * @brief The trailer
   */
  PDFTrailer pt;
} PDFXref;

/**
 * @brief An object stream index
 */

typedef struct {
  /**
   * @brief The object number
   */
  int obj_num;
  /**
   * @brief The offset
   */
  int offset;
} PDFObjStreamIndex;

/**
 * @brief The object stream
 */

typedef struct {
  /**
   * @brief The number of objects stored in this stream (/N).
   */
  int count;

  /**
   * @brief The byte offset in the decompressed stream
   */
  int first_offset;

  /**
   * @brief The length in bytes of the decompressed stream data.
   */
  long length;

  /**
   * @brief Whether the stream was compressed with /FlateDecode.
   */
  bool flate;

  /**
   * @brief Array of object index entries (/N items total).
   */
  PDFObjStreamIndex *index;

  /**
   * @brief The full decompressed stream data.
   */
  unsigned char *data;

  /**
   * @brief The total length of @ref data.
   */
  size_t data_len;
} PDFObjStream;

/**
 * @brief truncate an open file
 * @param f this is the file
 * @return nothing
 */

void truncate_open_file(FILE *f) {
  if (!f) {
    return;
  }
  fflush(f);
  FTRUNCATE(FILENO(f), 0);
  rewind(f);
}

/**
 * @brief This reads big endian
 * @param p The raw binary data
 * @param width the size of the big endian int to parse
 * @return The big endian integer (as a long)
 */

long read_be_int(const unsigned char *p, int width) {
  long val = 0;
  for (int i = 0; i < width; i++) {
    val = (val << 8) | p[i];
  }
  return val;
}

/**
 * @brief This decompresses a flate stream
 * @param[out] result The result
 * @param input The stream to be decompressed
 * @param input_len The size of the stream
 * @param[out] output_len The size of the decompressed
 * @return The decompressed stream
 */

unsigned char *decompress_flate(CanimResult *result, unsigned char *input,
                                size_t input_len, size_t *output_len) {
  size_t buf_size = input_len * 8;
  unsigned char *output = malloc(buf_size);
  if (!output) {
    *result = STREAM_DECOMPRESS_MALLOC_FAIL;
    return null;
  }
  z_stream strm = {0};
  strm.next_in = input;
  strm.avail_in = input_len;
  strm.next_out = output;
  strm.avail_out = buf_size;
  if (inflateInit(&strm) != Z_OK) {
    free(output);
    *result = STREAM_DECOMPRESS_INIT_FAIL;
    return null;
  }
  int ret = inflate(&strm, Z_FINISH);
  if (ret != Z_STREAM_END) {
    inflateEnd(&strm);
    free(output);
    *result = STREAM_DECOMPRESS_FAIL;
    return null;
  }
  *output_len = strm.total_out;
  inflateEnd(&strm);
  *result = SUCCESS;
  return output;
}

/**
 * @brief This generates the next string in a series of strings seperated by \0
 * @param[out] result Win/Lose
 * @param c The start of the string
 * @return A pointer to the next string
 */

unsigned char *next_str(CanimResult *result, unsigned char *c) {
  for (int32_t i = 0; *(c++) && ((i++ - BUFFER_SIZE) >> 31);) {
    // Sorry
  }
  *result = *(c - 1) ? NEXT_STR_NOT_FOUND_ERROR : SUCCESS;
  return *(c - 1) ? null : c;
}

/**
 * @brief This grabs the location of the xref table in a PDF file
 * @param[out] result The success/failing
 * @param f The PDF file
 * @return The offset
 */

long findxref(CanimResult *result, FILE *f) {
  char buf[BUFFER_SIZE + 1];
  long filesize;
  if (fseek(f, 0, SEEK_END) != 0) {
    *result = PDF_XREF_FIND_SEEK_END_ERROR;
    return -1;
  }
  filesize = ftell(f);
  if (filesize < 0) {
    *result = PDF_XREF_FIND_FTELL_ERROR;
    return -1;
  }
  if (fseek(f, max(filesize - BUFFER_SIZE, 0), SEEK_SET) != 0) {
    *result = PDF_XREF_FIND_SEEK_SET_ERROR;
    return -1;
  }
  size_t n = fread(buf, sizeof(char), BUFFER_SIZE, f);
  if (n == 0 && ferror(f)) {
    *result = PDF_XREF_FIND_FREAD_ERROR;
    return -1;
  }
  buf[n] = '\0';
  /* This keeps is because files tend to randomly have a lot of '\0', we get rid
   * of them */
  for (size_t i = 0; i < n; i++) {
    if (buf[i] == '\0') {
      buf[i] = 1;
    }
  }
  char *pos = strstr(buf, "startxref");
  if (!pos) {
    *result = PDF_XREF_STARTXREF_NOT_FOUND;
    return -1;
  }
  long offset = -1;
  if (sscanf(pos + 9, "%ld", &offset) != 1) {
    *result = PDF_XREF_OFFSET_PARSE_ERROR;
    return -1;
  }
  *result = SUCCESS;

  return offset;
}

/**
 * @brief Tests find xref
 * @param[out] result The result
 * @return nothing
 */

void test_findxref(CanimResult *result) {
  FILE *f = fopen("8.pdf", "rb");
  if (!f) {
    fprintf(stderr, "I am sorry :(\n");
    return;
  }
  long offset = findxref(result, f);
  if (IS_AN_ERROR(*result)) {
    print_error(*result);
    return;
  }
  fseek(f, offset, SEEK_SET);
  char a[2000];
  fread(a, sizeof(char), 2000, f);
  fwrite(a, sizeof(char), 2000, stdout);
  fclose(f);
}

/**
 * @brief This gets the xref
 * @param[out] result
 * @param f File
 * @return the xref
 */

PDFXref *get_xref(CanimResult *result, FILE *f) {
  long offset = findxref(result, f);
  if (IS_AN_ERROR(*result)) {
    return null;
  }
  char buf[BUFFER_SIZE];
  if (fseek(f, offset, SEEK_SET) != 0) {
    *result = GET_XREF_FSEEK_ERROR;
    return null;
  }
  size_t n = fread(buf, sizeof(char), BUFFER_SIZE, f);
  if (n == 0 && ferror(f)) {
    *result = GET_XREF_FREAD_ERROR;
    return null;
  }
  buf[n] = '\0';
  char *p = buf;

  p = strchr(p, '\n');
  if (!p) {
    *result = GET_XREF_STRCHR_NEWLINE_FAIL;
    return null;
  }
  p++;
  p = strchr(p, '\n');
  if (!p) {
    *result = GET_XREF_STRCHR_NEWLINE_FAIL;
    return null;
  }
  p++;
  char *q;
  int size = 0;
  int w0 = 0;
  int w1 = 0;
  int w2 = 0;
  long length = 0;
  int root_obj = 0;
  int root_gen = 0;
  if ((q = strstr(p, "/Size"))) {
    sscanf(q, "/Size %d", &size);
  }
  if ((q = strstr(p, "/Length"))) {
    sscanf(q, "/Length %ld", &length);
  }
  if ((q = strstr(p, "/W"))) {
    sscanf(q, "/W [%d %d %d]", &w0, &w1, &w2);
  }
  if ((q = strstr(p, "/Root"))) {
    sscanf(q, "/Root %d %d R", &root_obj, &root_gen);
  }
  PDFTrailer pt;
  pt.root_gen = root_gen;
  pt.root_obj = root_obj;
  pt.size = size;
  q = strstr(p, "stream");
  q += 7;
  long dict_offset_in_buf = (q - buf);
  long file_data_start = offset + dict_offset_in_buf;
  if (fseek(f, file_data_start, SEEK_SET) != 0) {
    *result = GET_XREF_STREAM_SEEK_ERROR;
    return null;
  }
  unsigned char *raw = malloc(length);
  if (!raw) {
    *result = GET_XREF_STREAM_MALLOC_ERROR;
    return null;
  }
  size_t read_bytes = fread(raw, 1, length, f);
  if (read_bytes != (size_t)length) {
    *result = GET_XREF_STREAM_READ_ERROR;
    free(raw);
    return null;
  }
  size_t decomp_len = 0;
  unsigned char *decomp = decompress_flate(result, raw, length, &decomp_len);
  free(raw);

  if (IS_AN_ERROR(*result)) {
    return null;
  }

  PDFXref *xref = malloc(sizeof(PDFXref));
  if (!xref) {
    *result = GET_XREF_MALLOC_ERROR;
    free(decomp);
    return null;
  }
  xref->pt = pt;
  xref->tb.size = size;
  xref->tb.entries = calloc(size, sizeof(PDFXrefEntry));
  if (!xref->tb.entries) {
    *result = GET_XREF_TABLE_ENTRIES_MALLOC_ERROR;
    free(decomp);
    free(xref);
    return null;
  }
  int entry_size = w0 + w1 + w2;
  for (int i = 0; i < size; i++) {
    unsigned char *rec = decomp + i * entry_size;
    xref->tb.entries[i].type = read_be_int(rec, w0);
    xref->tb.entries[i].offset = read_be_int(rec + w0, w1);
    xref->tb.entries[i].generation = read_be_int(rec + w0 + w1, w2);
  }
  free(decomp);

  *result = SUCCESS;
  return xref;
}

PFNGLUNIFORM1IPROC pglUniform1i;
PFNGLUNIFORM1FPROC pglUniform1f;
PFNGLUNIFORM3FPROC pglUniform3f;
PFNGLUNIFORM2FPROC pglUniform2f;
PFNGLUNIFORM4FPROC pglUniform4f;
PFNGLUNIFORM1FVPROC pglUniform1fv;
PFNGLUSEPROGRAMPROC pglUseProgram;
PFNGLUNIFORM3FVPROC pglUniform3fv;
PFNGLBINDBUFFERPROC pglBindBuffer;
PFNGLGENBUFFERSPROC pglGenBuffers;
PFNGLBUFFERDATAPROC pglBufferData;
PFNGLGETSHADERIVPROC pglGetShaderiv;
PFNGLLINKPROGRAMPROC pglLinkProgram;
PFNGLCREATESHADERPROC pglCreateShader;
PFNGLSHADERSOURCEPROC pglShaderSource;
PFNGLATTACHSHADERPROC pglAttachShader;
PFNGLGETPROGRAMIVPROC pglGetProgramiv;
PFNGLDELETESHADERPROC pglDeleteShader;
PFNGLDETACHSHADERPROC pglDetachShader;
PFNGLCOMPILESHADERPROC pglCompileShader;
PFNGLCREATEPROGRAMPROC pglCreateProgram;
PFNGLDELETEPROGRAMPROC pglDeleteProgram;
PFNGLDELETEBUFFERSPROC pglDeleteBuffers;
PFNGLGENVERTEXARRAYSPROC pglGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC pglBindVertexArray;
PFNGLVALIDATEPROGRAMPROC pglValidateProgram;
PFNGLUNIFORMMATRIX4FVPROC pglUniformMatrix4fv;
PFNGLGETSHADERINFOLOGPROC pglGetShaderInfoLog;
PFNGLGETPROGRAMINFOLOGPROC pglGetProgramInfoLog;
PFNGLGETATTRIBLOCATIONPROC pglGetAttribLocation;
PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation;
PFNGLDELETEVERTEXARRAYSPROC pglDeleteVertexArrays;
PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer;
PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray;
PFNGLDISABLEVERTEXATTRIBARRAYPROC pglDisableVertexAttribArray;

#define LOAD_GL(type, var, name)                                               \
  do {                                                                         \
    var = (type)(uintptr_t)SDL_GL_GetProcAddress(name);                        \
    if (!var) {                                                                \
      *result = LOAD_OPENGL_FUNCTION_ERROR;                                    \
      return;                                                                  \
    }                                                                          \
  } while (0)

/**
 * @brief load all shader functions
 * @param[out] result Did it succeed
 * @return nothing lol
 */

void load_gl_shader_functions(CanimResult *result) {
  LOAD_GL(PFNGLUNIFORM1FPROC, pglUniform1f, "glUniform1f");
  LOAD_GL(PFNGLUNIFORM3FPROC, pglUniform3f, "glUniform3f");
  LOAD_GL(PFNGLUNIFORM1IPROC, pglUniform1i, "glUniform1i");
  LOAD_GL(PFNGLUNIFORM2FPROC, pglUniform2f, "glUniform2f");
  LOAD_GL(PFNGLUNIFORM4FPROC, pglUniform4f, "glUniform4f");
  LOAD_GL(PFNGLGENBUFFERSPROC, pglGenBuffers, "glGenBuffers");
  LOAD_GL(PFNGLUSEPROGRAMPROC, pglUseProgram, "glUseProgram");
  LOAD_GL(PFNGLUNIFORM1FVPROC, pglUniform1fv, "glUniform1fv");
  LOAD_GL(PFNGLUNIFORM3FVPROC, pglUniform3fv, "glUniform3fv");
  LOAD_GL(PFNGLBINDBUFFERPROC, pglBindBuffer, "glBindBuffer");
  LOAD_GL(PFNGLBUFFERDATAPROC, pglBufferData, "glBufferData");
  LOAD_GL(PFNGLGETSHADERIVPROC, pglGetShaderiv, "glGetShaderiv");
  LOAD_GL(PFNGLLINKPROGRAMPROC, pglLinkProgram, "glLinkProgram");
  LOAD_GL(PFNGLCREATESHADERPROC, pglCreateShader, "glCreateShader");
  LOAD_GL(PFNGLSHADERSOURCEPROC, pglShaderSource, "glShaderSource");
  LOAD_GL(PFNGLATTACHSHADERPROC, pglAttachShader, "glAttachShader");
  LOAD_GL(PFNGLGETPROGRAMIVPROC, pglGetProgramiv, "glGetProgramiv");
  LOAD_GL(PFNGLDETACHSHADERPROC, pglDetachShader, "glDetachShader");
  LOAD_GL(PFNGLDELETESHADERPROC, pglDeleteShader, "glDeleteShader");
  LOAD_GL(PFNGLDELETEBUFFERSPROC, pglDeleteBuffers, "glDeleteBuffers");
  LOAD_GL(PFNGLCOMPILESHADERPROC, pglCompileShader, "glCompileShader");
  LOAD_GL(PFNGLDELETEPROGRAMPROC, pglDeleteProgram, "glDeleteProgram");
  LOAD_GL(PFNGLCREATEPROGRAMPROC, pglCreateProgram, "glCreateProgram");
  LOAD_GL(PFNGLVALIDATEPROGRAMPROC, pglValidateProgram, "glValidateProgram");
  LOAD_GL(PFNGLBINDVERTEXARRAYPROC, pglBindVertexArray, "glBindVertexArray");
  LOAD_GL(PFNGLGENVERTEXARRAYSPROC, pglGenVertexArrays, "glGenVertexArrays");
  LOAD_GL(PFNGLUNIFORMMATRIX4FVPROC, pglUniformMatrix4fv, "glUniformMatrix4fv");
  LOAD_GL(PFNGLGETSHADERINFOLOGPROC, pglGetShaderInfoLog, "glGetShaderInfoLog");
  LOAD_GL(PFNGLGETATTRIBLOCATIONPROC, pglGetAttribLocation,
          "glGetAttribLocation");
  LOAD_GL(PFNGLGETPROGRAMINFOLOGPROC, pglGetProgramInfoLog,
          "glGetProgramInfoLog");
  LOAD_GL(PFNGLGETUNIFORMLOCATIONPROC, pglGetUniformLocation,
          "glGetUniformLocation");
  LOAD_GL(PFNGLDELETEVERTEXARRAYSPROC, pglDeleteVertexArrays,
          "glDeleteVertexArrays");
  LOAD_GL(PFNGLVERTEXATTRIBPOINTERPROC, pglVertexAttribPointer,
          "glVertexAttribPointer");
  LOAD_GL(PFNGLENABLEVERTEXATTRIBARRAYPROC, pglEnableVertexAttribArray,
          "glEnableVertexAttribArray");
  LOAD_GL(PFNGLDISABLEVERTEXATTRIBARRAYPROC, pglDisableVertexAttribArray,
          "glDisableVertexAttribArray");
  *result = SUCCESS;
}

/**
 * @brief This compiles a shader
 * @param[out] result This is the result
 * @param src the source code of your shader
 * @param type The type of your shader
 * @return Your compiled shader
 *
 */

GLuint compile_shader(CanimResult *result, const char *src, GLenum type) {
  GLuint shader = pglCreateShader(type);
  pglShaderSource(shader, 1, &src, null);
  pglCompileShader(shader);
  GLint _;
  pglGetShaderiv(shader, GL_COMPILE_STATUS, &_);
  if (!_) {
    *result = OPENGL_SHADER_COMPILATION_ERROR;
    pglDeleteShader(shader);
    return 0;
  }
  *result = SUCCESS;
  return shader;
}

/**
 * @brief This creates a shader program
 * @param vs_src The vertex shader source
 * @param fs_src The fragment shader source
 * @return The shader program
 */

GLuint create_shader_program(CanimResult *result, const char *vs_src,
                             const char *fs_src) {
  GLuint vs = compile_shader(result, vs_src, GL_VERTEX_SHADER);
  if (IS_AN_ERROR(*result)) {
    return 0;
  }
  GLuint fs = compile_shader(result, fs_src, GL_FRAGMENT_SHADER);
  if (IS_AN_ERROR(*result)) {
    return 0;
  }
  GLuint prog = pglCreateProgram();
  pglAttachShader(prog, vs);
  pglAttachShader(prog, fs);
  pglLinkProgram(prog);
  GLint _;
  pglGetProgramiv(prog, GL_LINK_STATUS, &_);
  if (!_) {
    *result = OPENGL_SHADER_PROGRAM_LINK_ERROR;
    return 0;
  }
  pglDetachShader(prog, vs);
  pglDetachShader(prog, fs);
  pglDeleteShader(vs);
  pglDeleteShader(fs);
  *result = SUCCESS;
  return prog;
}

/**
 * @brief This gets the frame buffer
 * @param[out] result The result value
 * @param w The width
 * @param h The height
 * @param previous_buffer A previous buffer
 * @return the framebuffer
 */

unsigned char *get_framebuffer_rgb(CanimResult *result, int w, int h,
                                   unsigned char *previous_buffer) {
  unsigned char *rgb;
  if (!previous_buffer) {
    rgb = malloc(w * h * 3);
    if (!rgb) {
      *result = RGB_BUFFER_MALLOC_ERROR;
      return null;
    }
  } else {
    rgb = previous_buffer;
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

FILE *open_ffmpeg_pipe(int w, int h, int fps, const char *out_mp4) {
  char cmd[1024];
  snprintf(
      cmd, sizeof(cmd),
      "ffmpeg -y -f rawvideo -pixel_format rgb24 -video_size %dx%d -framerate "
      "%d -i - "
      "-vf vflip -c:v libx264 -preset veryfast -crf 18 -pix_fmt yuv420p \"%s\"",
      w, h, fps, out_mp4);

  FILE *pipef = POPEN(cmd, WB);
  if (!pipef) {
    fprintf(stderr, "popen failed: %s (errno=%d)\n", strerror(errno), errno);
  }
  return pipef;
}

/**
 * @brief This closes ffmpeg pipe
 * @param pipef The pipe to close
 * @return nothing
 */

void close_ffmpeg_pipe(FILE *pipef) {
  if (pipef) {
    PCLOSE(pipef);
  }
}

/**
 * @brief It outputs a brand new triangulation
 * @param[out] result The result is set to all the goofy errors.
 * @param src The triangulation to be cloned
 * @return The triangulation
 */

Triangulation *clone_triangulation(CanimResult *result, Triangulation *src) {
  Triangulation *tri = malloc(sizeof(Triangulation));
  if (!tri) {
    *result = TRI_CLONE_ERROR;
    return null;
  }
  tri->triangle_count = src->triangle_count;
  tri->triangles = malloc(sizeof(TriangleRaw) * BIT_ALIGN(src->triangle_count));
  if (!tri->triangles) {
    free(tri);
    *result = TRI_CLONE_TRI_ERROR;
    return null;
  }
  memcpy(tri->triangles, src->triangles,
         sizeof(TriangleRaw) * tri->triangle_count);
  *result = SUCCESS;
  return tri;
}

/**
 * @brief It outputs an empty triangulations
 * @param[out] result The result is set to all the goofy errors.
 * @return The triangulation
 */

Triangulation *empty_triangulation(CanimResult *result) {
  Triangulation *tri = malloc(sizeof(Triangulation));
  if (!tri) {
    *result = TRI_INIT_MALLOC_FAIL; // Soon that attitude will be your doom
    return null;
  }
  tri->triangle_count = 0;
  tri->triangles = null;
  *result = SUCCESS;
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

void add_triangle(CanimResult *result, Triangulation *tri,
                  TriangleRaw triangle) {
  if (!tri) {
    *result = TRI_NOT_FOUND;
    return;
  }

  if (REALIGN(tri->triangle_count, tri->triangle_count + 1)) {
    int new_capacity = BIT_ALIGN(tri->triangle_count + 1);
    TriangleRaw *temp =
        realloc(tri->triangles, new_capacity * sizeof(TriangleRaw));
    if (!temp) {
      *result = ADDING_TRI_REALLOC_FAILURE;
      return;
    }
    tri->triangles = temp;
  }
  memcpy(tri->triangles + tri->triangle_count, &triangle, sizeof(TriangleRaw));
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

void merge_triangulations(CanimResult *result, Triangulation **triangulations,
                          int tri_count, Triangulation *output) {
  Triangulation *e = empty_triangulation(result);
  if (e == null) {
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

  for (int i = 0; i < tri_count; i++) {
    for (int j = 0; j < triangulations[i]->triangle_count; j++) {
      add_triangle(result, output, triangulations[i]->triangles[j]);
      if (IS_AN_ERROR(*result)) {
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

void free_triangulation(Triangulation *triangulation) {
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

Vec3 add_vec3(Vec3 a, Vec3 b) {
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

Vec3 multiply_vec3(Vec3 a, float scalar) {
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

Vec3 subtract_vec3(Vec3 a, Vec3 b) {
  return add_vec3(a, multiply_vec3(b, -1.0f));
}

/**
 * @brief This interpolates between two vectors
 * @param a This is the starting vector
 * @param b This is the ending vector
 * @param t This is how much we interpolate
 * @return It returns lerp( @p a, @p b, @p t )
 */

Vec3 lerp_vec3(Vec3 a, Vec3 b, float t) {
  return add_vec3(a, multiply_vec3(subtract_vec3(b, a), t));
}

/**
 * @brief
 * @param v The vector to be subtracteed from
 * @return This outputs the magnitude of v
 */

float magnitude_vec3(Vec3 a) { return sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

/**
 * @brief This is the distance between two points
 * @param a the first one
 * @param b The second one
 * @return The distance
 */

float dist_vec3(Vec3 a, Vec3 b) { return magnitude_vec3(subtract_vec3(a, b)); }

/**
 * @brief This outputs whether or not two vectors are equal
 * @param a the first one
 * @param b The second one
 * @return a bool
 */

bool equal_vec3(Vec3 a, Vec3 b) { return fabs(dist_vec3(a, b)) < EPSILON; }

/**
 * @brief This normalizes a vector
 * @param a The vector to be normalized
 * @return The normalization
 */

Vec3 normalize_vec3(Vec3 a) {
  float mag = magnitude_vec3(a);
  if (mag < EPSILON) {
    Vec3 f = {0, 0, 0};
    return f;
  }
  return multiply_vec3(a, 1 / mag);
}

/**
 * @brief This takes the cross product fo two vectors
 * @param a the first one
 * @param b the second one
 * @return the normalization
 */

Vec3 cross_vec3(Vec3 a, Vec3 b) {
  Vec3 r = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
  return r;
}

/**
 * @brief Generates normal vector
 * @param a first vertex of the triangle
 * @param b The second vertex
 * @param c The third
 * @return A normal vector
 */

Vec3 normal_vec3(Vec3 a, Vec3 b, Vec3 c) {
  Vec3 AB = subtract_vec3(b, a);
  Vec3 AC = subtract_vec3(c, a);
  Vec3 n = cross_vec3(AB, AC);
  return normalize_vec3(n);
}

/**
 * @brief This multiplies two quaternions
 * @param q1 the first one
 * @param q2 the second one
 * @return q1 * q2
 */

Quaternion quat_mul(Quaternion q1, Quaternion q2) {
  return (Quaternion){
      .x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
      .y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
      .z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w,
      .w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z};
}

/**
 * @brief This conjugates a quaternion
 * @param q This is the quaternion
 * @return This returns the conjugate of the quaternion
 */

Quaternion quat_conjugate(Quaternion q) {
  return (Quaternion){.x = -q.x, .y = -q.y, .z = -q.z, .w = q.w};
}

/**
 * @brief This generates a quaternion from an angle/axis
 * @param axis This is the axis to rotate around
 * @param angle This is the angle of rotation
 * @return A quaternion
 */

Quaternion quat_from_axis_angle(Vec3 axis, float angle) {
  axis = normalize_vec3(axis);
  float half = angle * 0.5f;
  float s = sinf(half);

  return (Quaternion){
      .x = axis.x * s, .y = axis.y * s, .z = axis.z * s, .w = cosf(half)};
}

/**
 * @brief This uses a rotation quaternion to rotate a vector
 * @param q The quaternion
 * @param v The vector
 * @return The new rotated vec3.
 */

Vec3 quat_rotate_vec3(Quaternion q, Vec3 v) {
  Quaternion vq = {.x = v.x, .y = v.y, .z = v.z, .w = 0.0f};
  Quaternion qi = quat_conjugate(q);
  Quaternion r = quat_mul(quat_mul(q, vq), qi);
  return (Vec3){r.x, r.y, r.z};
}

/**
 * @brief This generates a quaternion from an angle/axis
 * @param axis This is the axis to rotate around
 * @param angle This is the angle of rotation
 * @param vec This is the vector to be rotated
 * @return A vector
 */

Vec3 rotate_vector(Vec3 axis, float angle, Vec3 vec) {
  Quaternion q = quat_from_axis_angle(axis, angle);
  return quat_rotate_vec3(q, vec);
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

int intersecting_segments(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 *out) {
  if (fabs(a.x - b.x) < EPSILON && fabs(a.y - b.y) < EPSILON &&
      fabs(c.x - d.x) < EPSILON && fabs(c.y - d.y) < EPSILON) {
    return 0;
  }
  float tx;
  float ty;
  int vertical = 0;
  float denomx;
  float denomy;
  if (fabs(a.x - b.x) < EPSILON && fabs(a.y - b.y) < EPSILON) {
    tx = (a.x - c.x);
    ty = (a.y - c.y);
    denomx = d.x - c.x;
    denomy = d.y - c.y;
    vertical = 1;
  }
  if (fabs(c.x - d.x) < EPSILON && fabs(c.y - d.y) < EPSILON) {
    tx = (c.x - a.x);
    ty = (c.y - a.y);
    denomx = b.x - a.x;
    denomy = b.x - a.y; // if you change this line of code, i will end you
    vertical = 2;
  }
  if (vertical > 0) {
    if (fabs(denomx) < EPSILON) {
      return 0;
    }
    if (fabs(denomy) < EPSILON) {
      return 0;
    }

    tx /= denomx;
    ty /= denomy;
    if (tx < 0 || tx > 1) {
      return 0;
    }
    if (ty < 0 || ty > 1) {
      return 0;
    }

    float t_avg;
    if (fabs(tx - ty) < EPSILON) {
      t_avg = (tx + ty) / 2;
    } else {
      return 0;
    }
    if (vertical == 1) {
      *out = lerp_vec3(c, d, t_avg);
    } else if (vertical == 2) {
      *out = lerp_vec3(a, b, t_avg);
    }
    return 1;
  }
  float denom = (a.x - b.x) * (c.y - d.y) - (a.y - b.y) * (c.x - d.x);

  if (fabs(denom) < EPSILON) {
    return 0;
  }
  float t = ((a.x - c.x) * (c.y - d.y) - (a.y - c.y) * (c.x - d.x)) / denom;
  float u = -((a.x - b.x) * (a.y - c.y) - (a.y - b.y) * (a.x - c.x)) / denom;
  if (t < 0 || t > 1) {
    return 0;
  }
  if (u < 0 || u > 1) {
    return 0;
  }

  Vec3 v1 = lerp_vec3(a, b, t);
  Vec3 v2 = lerp_vec3(c, d, u);
  if (fabs(v1.z - v2.z) < EPSILON) {
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

PSLG *generate_pslg(CanimResult *result, PolygonRaw pr) {
  PSLG *new = malloc(sizeof(PSLG));
  if (!new) {
    *result = PSLG_INIT_MALLOC_ERROR;
    return null;
  }
  new->vertex_count = pr.vertex_count;
  new->edge_count = pr.vertex_count;
  new->vertices = malloc(BIT_ALIGN(pr.vertex_count) * sizeof(Vec3));
  if (!new->vertices) {
    new->vertex_count = 0;
    new->edge_count = 0;
    new->edges = null;
    *result = PSLG_VERTEX_MALLOC_ERROR;
    return null;
  }
  for (int i = 0; i < pr.vertex_count; i++) {
    new->vertices[i] = pr.vertices[i];
  }

  new->edges = malloc(BIT_ALIGN(new->edge_count) * sizeof(int[2]));
  if (!new->edges) {
    free(new->vertices);
    new->vertex_count = 0;
    new->edge_count = 0;
    *result = PSLG_EDGE_MALLOC_ERROR;
    return null;
  }

  for (int i = 0; i < pr.vertex_count; i++) {
    new->edges[i][0] = i;
    new->edges[i][1] = (i + 1) % pr.vertex_count;
  }
  new->poly = pr;
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

void dedup_pslg_a_vertex(CanimResult *result, PSLG *pslg, int v1, int v2) {
  if (v1 == v2) {
    *result = NOOP;
    return;
  }
  if (v1 > v2) {
    *result = NOOP;
    return;
  }
  Vec3 vec1 = pslg->vertices[v1];
  Vec3 vec2 = pslg->vertices[v2];
  if (!equal_vec3(vec1, vec2)) {
    *result = NOOP;
    return;
  }
  for (int i = v2; i < pslg->vertex_count - 1; i++) {
    pslg->vertices[i] = pslg->vertices[i + 1];
  }
  if (REALIGN(pslg->vertex_count, pslg->vertex_count - 1)) {
    Vec3 *temp = realloc(pslg->vertices,
                         BIT_ALIGN(pslg->vertex_count - 1) * sizeof(Vec3));
    if (!temp) {
      *result = DEDUP_PSLG_VERTEX_REALLOC_ERROR;
      return;
    }
    pslg->vertices = temp;
  }
  pslg->vertex_count--;
  for (int i = 0; i < pslg->edge_count; i++) {
    if (pslg->edges[i][0] == v2) {
      pslg->edges[i][0] = v1;
    }
    if (pslg->edges[i][1] == v2) {
      pslg->edges[i][1] = v1;
    }
    if (pslg->edges[i][0] > v2) {
      pslg->edges[i][0]--;
    }
    if (pslg->edges[i][1] > v2) {
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

void dedup_pslg_one_vertex(CanimResult *result, PSLG *pslg) {
  for (int i = 0; i < pslg->vertex_count; i++) {
    for (int j = 0; j < pslg->vertex_count; j++) {
      dedup_pslg_a_vertex(result, pslg, i, j);
      if (*result != NOOP) {
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

void dedup_pslg_vertex(CanimResult *result, PSLG *pslg) {
  for (;;) {
    dedup_pslg_one_vertex(result, pslg);
    if (*result == NOOP) {
      return;
    }
    if (IS_AN_ERROR(*result)) {
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

void dedup_pslg_a_edge(CanimResult *result, PSLG *pslg, int e1, int e2) {
  if (e1 > e2) {
    *result = NOOP;
    return;
  }
  if (e1 == e2) {
    *result = NOOP;
    return;
  }

  bool b = false;

  if (equal_vec3(pslg->vertices[pslg->edges[e1][0]],
                 pslg->vertices[pslg->edges[e2][0]])) {
    if (equal_vec3(pslg->vertices[pslg->edges[e1][1]],
                   pslg->vertices[pslg->edges[e2][1]])) {
      b = true;
    }
  }

  if (equal_vec3(pslg->vertices[pslg->edges[e1][0]],
                 pslg->vertices[pslg->edges[e2][1]])) {
    if (equal_vec3(pslg->vertices[pslg->edges[e1][1]],
                   pslg->vertices[pslg->edges[e2][0]])) {
      b = true;
    }
  }

  if (!b) {
    *result = NOOP;
    return;
  }

  for (int i = e2; i < pslg->edge_count - 1; i++) {
    pslg->edges[i][0] = pslg->edges[i + 1][0];
    pslg->edges[i][1] = pslg->edges[i + 1][1];
  }

  if (REALIGN(pslg->edge_count, pslg->edge_count - 1)) {
    int (*temp_ptr)[2] =
        realloc(pslg->edges, sizeof(int[2]) * BIT_ALIGN(pslg->edge_count - 1));
    if (!temp_ptr) {
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

void dedup_pslg_one_edge(CanimResult *result, PSLG *pslg) {
  for (int i = 0; i < pslg->edge_count; i++) {
    for (int j = 0; j < pslg->edge_count; j++) {
      dedup_pslg_a_edge(result, pslg, i, j);
      if (*result != NOOP) {
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

void dedup_pslg_edge(CanimResult *result, PSLG *pslg) {
  for (;;) {
    dedup_pslg_one_edge(result, pslg);
    if (*result == NOOP) {
      return;
    }
    if (IS_AN_ERROR(*result)) {
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

void dedup_pslg(CanimResult *result, PSLG *pslg) {
  dedup_pslg_vertex(result, pslg);
  if (IS_AN_ERROR(*result)) {
    return;
  }
  dedup_pslg_edge(result, pslg);
  if (IS_AN_ERROR(*result)) {
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

void splitPSLG(CanimResult *result, PSLG *pslg, int edge1, int edge2) {
  Vec3 out;
  if (pslg->edges[edge1][0] == pslg->edges[edge2][0] ||
      pslg->edges[edge1][0] == pslg->edges[edge2][1] ||
      pslg->edges[edge1][1] == pslg->edges[edge2][0] ||
      pslg->edges[edge1][1] == pslg->edges[edge2][1]) {
    *result = NOOP;
    return;
  }

  Vec3 v1 = pslg->vertices[pslg->edges[edge1][0]];
  Vec3 v2 = pslg->vertices[pslg->edges[edge1][1]];
  Vec3 v3 = pslg->vertices[pslg->edges[edge2][0]];
  Vec3 v4 = pslg->vertices[pslg->edges[edge2][1]];
  int intersection = intersecting_segments(v1, v2, v3, v4, &out);
  if (intersection == 0) {
    *result = NOOP;
    return;
  }

  if (REALIGN(pslg->vertex_count, pslg->vertex_count + 1)) {
    Vec3 *temp_ptr = realloc(pslg->vertices,
                             sizeof(Vec3) * BIT_ALIGN(pslg->vertex_count + 1));
    if (temp_ptr == null) {
      *result = PSLG_EDGE_SPLIT_VERTEX_REALLOC_ERROR;
      return;
    }
    pslg->vertices = temp_ptr;
  }
  if (REALIGN(pslg->edge_count, pslg->edge_count + 2)) {
    int (*temp_ptr)[2] =
        realloc(pslg->edges, sizeof(int[2]) * BIT_ALIGN(pslg->edge_count + 2));
    if (temp_ptr == null) {
      *result = PSLG_EDGE_SPLIT_EDGE_REALLOC_ERROR;
      return;
    }
    pslg->edges = temp_ptr;
  }
  pslg->vertices[pslg->vertex_count] = out;
  pslg->edges[pslg->edge_count][0] = pslg->edges[edge1][1];
  pslg->edges[pslg->edge_count][1] = pslg->vertex_count;
  pslg->edges[pslg->edge_count + 1][0] = pslg->edges[edge2][1];
  pslg->edges[pslg->edge_count + 1][1] = pslg->vertex_count;
  pslg->edges[edge1][1] = pslg->vertex_count;
  pslg->edges[edge2][1] = pslg->vertex_count;
  pslg->edge_count += 2;
  pslg->vertex_count += 1;
  *result = SUCCESS;
}

/**
 * @brief This removes a single edge
 * @param[out] result This is the status
 * @param pslg This is the PSLG that needs an edge to be removed
 * @return This outputs nothing
 */

void remove_single_edge(CanimResult *result, PSLG *pslg) {
  for (int i = 0; i < pslg->edge_count; i++) {
    for (int j = 0; j < pslg->edge_count; j++) {
      splitPSLG(result, pslg, i, j);
      if (*result != NOOP) {
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

void split_entirely(CanimResult *result, PSLG *pslg) {
  for (;;) {
    int ne = pslg->edge_count;
    int nv = pslg->vertex_count;
    remove_single_edge(result, pslg);

    if (*result == NOOP) {
      *result = SUCCESS;
      return;
    }
    if (IS_AN_ERROR(*result)) {
      return;
    }
    dedup_pslg(result, pslg);
    if (IS_AN_ERROR(*result)) {
      return;
    }

    if (pslg->edge_count == ne) {
      if (pslg->vertex_count == nv) {
        return;
      }
    }
  }
}

/**
 * @brief This frees a pslg
 * @param pslg This is the PSLG that need be freed.
 */

void free_pslg(PSLG *pslg) {
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

PSLGTriangulation *create_pslg_triangulation(CanimResult *result, PSLG *pslg) {
  PSLGTriangulation *pslgtri = malloc(sizeof(PSLGTriangulation));
  if (!pslgtri) {
    *result = PSLG_TRIANGULATION_INIT_MALLOC_ERROR;
    return null;
  }
  pslgtri->triangulation = empty_triangulation(result);
  if (IS_AN_ERROR(*result)) {
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

void attack_vertex(CanimResult *result, PSLGTriangulation *pslgtri,
                   int vertex_idx) {
  PSLG *pslg = pslgtri->pslg;
  Triangulation *tri = pslgtri->triangulation;
  int d = 0;
  int e1;
  int e2;
  for (int i = 0; i < pslg->edge_count; i++) {
    if (pslg->edges[i][0] == vertex_idx || pslg->edges[i][1] == vertex_idx) {
      d++;
      if (d > 2) {
        *result = NOOP;
        return;
      }
      if (d > 1) {
        e2 = i;
      } else {
        e1 = i;
      }
    }
  }
  if (d != 2) {
    *result = NOOP;
    return;
  }
  int v1 = pslg->edges[e1][0];
  int v2 = pslg->edges[e2][0];
  int v3 = pslg->edges[e2][1];
  if (v1 == vertex_idx) {
    v1 = pslg->edges[e1][1];
  }
  if (v2 == vertex_idx) {
    v2 = pslg->edges[e2][1];
    v3 = pslg->edges[e2][0];
  }
  TriangleRaw tr;
  tr.fd = pslgtri->pslg->poly.fd;
  tr.vertices[0] = pslg->vertices[v1];
  tr.vertices[1] = pslg->vertices[v2];
  tr.vertices[2] = pslg->vertices[v3];
  add_triangle(result, tri, tr);
  if (IS_AN_ERROR(*result)) {
    return;
  }
  int e3_exists = 0;
  for (int i = 0; i < pslg->edge_count; i++) {
    if ((pslg->edges[i][0] == v1 && pslg->edges[i][1] == v2) ||
        (pslg->edges[i][0] == v2 && pslg->edges[i][1] == v1)) {
      e3_exists = 1;
      break;
    }
  }
  int ecount = e3_exists ? pslg->edge_count - 2 : pslg->edge_count - 1;
  int (*temp)[2] = malloc(
      ecount *
      sizeof(int[2])); // this isn't aligned because it is gonna DIE SOON.
  if (temp == null) {

    *result = PSLG_ATTACK_TEMP_EDGES_MALLOC_ERROR;
    return;
  }

  int EI = 0;
  for (int i = 0; i < pslg->edge_count; i++) {
    // sacrifice e1, and e2
    if (i == e1 || i == e2) {
      continue;
    }
    temp[EI][0] = pslg->edges[i][0];
    temp[EI][1] = pslg->edges[i][1];

    EI++;
  }

  // so e3_exists
  // handle it
  if (!e3_exists) {
    temp[EI][0] = v1;
    temp[EI][1] = v2;
  }
  if (REALIGN(pslg->edge_count, ecount)) {
    int (*temp_ptr)[2] =
        realloc(pslg->edges, BIT_ALIGN(ecount) * sizeof(int[2]));

    if (temp_ptr == null) {
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
  for (int i = 0; i < ecount; i++) {
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

void attack_single_vertex(CanimResult *result, PSLGTriangulation *pslgtri) {
  for (int i = 0; i < pslgtri->pslg->vertex_count; i++) {
    attack_vertex(result, pslgtri, i);
    if (*result == SUCCESS) {
      return;
    }
    if (*result == NOOP) {
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

void attack_all_vertices(CanimResult *result, PSLGTriangulation *pslgtri) {
  for (;;) {
    attack_single_vertex(result, pslgtri);
    if (*result == NOOP) {
      *result = SUCCESS;
      return;
    }
    if (IS_AN_ERROR(*result)) {
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

void generate_triangulation(CanimResult *result, PolygonRaw pr,
                            Triangulation *tri) {
  PSLG *pslg = generate_pslg(result, pr);
  if (IS_AN_ERROR(*result)) {
    return;
  }
  split_entirely(result, pslg);
  if (IS_AN_ERROR(*result)) {
    free_pslg(pslg);
    return;
  }

  PSLGTriangulation *pslgtri = create_pslg_triangulation(result, pslg);
  if (IS_AN_ERROR(*result)) {
    free_triangulation(pslgtri->triangulation);
    free_pslg(pslg);
    free(pslgtri);
    return;
  }
  attack_all_vertices(result, pslgtri);
  if (IS_AN_ERROR(*result)) {
    free_triangulation(pslgtri->triangulation);
    free_pslg(pslg);
    free(pslgtri);
    return;
  }

  for (int i = 0; i < pslgtri->triangulation->triangle_count; i++) {
    add_triangle(result, tri, pslgtri->triangulation->triangles[i]);
    if (IS_AN_ERROR(*result)) {
      free_triangulation(pslgtri->triangulation);
      free_pslg(pslg);
      free(pslgtri);
      return;
    }
  }
  tri->triangle_count = pslgtri->triangulation->triangle_count;

  free_pslg(pslg);
  free_triangulation(pslgtri->triangulation);
  free(pslgtri);
  *result = SUCCESS;
}

/**
 * @brief Triangulates the polyhedron
 * @param[out] result The status
 * @param poly The polyhedron to be triangulated
 * @param out Where the triangulation is stored
 * @return nothing
 */

void triangulate_polyhedron(CanimResult *result, Polyhedron *poly,
                            Triangulation *out) {
  Triangulation **tris = malloc(poly->face_count * sizeof(Triangulation *));
  if (!tris) {
    *result = TRIANGULATE_POLYHEDRON_BATCH_TRIANGULATIONS_MALLOC_ERROR;
    return;
  }

  for (int i = 0; i < poly->face_count; i++) {
    tris[i] = NULL;
  }

  for (int i = 0; i < poly->face_count; i++) {
    Triangulation *t = empty_triangulation(result);
    if (IS_AN_ERROR(*result)) {
      for (int j = 0; j < i; j++) {
        if (tris[j]) {
          free_triangulation(tris[j]);
        }
      }
      free(tris);
      return;
    }
    PolygonIndexed *face = &poly->poly[i];
    Vec3 *verts = malloc(BIT_ALIGN(face->vertex_count) * sizeof(Vec3));
    if (!verts) {
      *result = TRIANGULATE_POLYHEDRON_VERTEX_MALLOC_ERROR;
      free_triangulation(t);
      for (int j = 0; j < i; j++) {
        if (tris[j]) {
          free_triangulation(tris[j]);
        }
      }
      free(tris);
      return;
    }

    for (int v = 0; v < face->vertex_count; v++) {
      verts[v] = poly->vertices[face->vertices[v]];
    }

    PolygonRaw pr = {
        .vertex_count = face->vertex_count, .vertices = verts, .fd = face->fd};

    generate_triangulation(result, pr, t);
    free(verts);
    if (IS_AN_ERROR(*result)) {
      free_triangulation(t);
      for (int j = 0; j < i; j++) {
        if (tris[j]) {
          free_triangulation(tris[j]);
        }
      }
      free(tris);
      return;
    }

    tris[i] = t;
  }

  merge_triangulations(result, tris, poly->face_count, out);
  for (int i = 0; i < poly->face_count; i++) {
    if (tris[i]) {
      free_triangulation(tris[i]);
    }
  }
  free(tris);

  if (IS_AN_ERROR(*result)) {
    return;
  }

  *result = SUCCESS;
}

/**
 * @brief This takes a polyhedron and frees it
 * @param poly This is the polyhedron to be freed
 * @return nothing
 */

void free_polyhedron(Polyhedron *poly) {
  if (!poly) {
    return;
  }

  if (poly->poly) {
    for (int i = 0; i < poly->face_count; i++) {
      PolygonIndexed *face = &poly->poly[i];
      if (face->vertices) {
        free(face->vertices);
      }
    }
    free(poly->poly);
  }

  if (poly->vertices) {
    free(poly->vertices);
  }

  free(poly);
}

/**
 * @brief This allocates memory for a polyhedron
 * @param[out] result This is the status of this function
 * @param nv The vertex count
 * @param nf The face count
 * @return A pointer to your brand new polyhedron
 */

Polyhedron *create_polyhedron(CanimResult *result, int nv, int nf) {
  Polyhedron *poly = malloc(sizeof(Polyhedron));
  if (!poly) {
    *result = POLYHEDRON_MALLOC_ERROR;
    return NULL;
  }
  poly->face_count = nf;
  poly->vertex_count = nv;
  poly->vertices = malloc(sizeof(Vec3) * nv);
  if (!poly->vertices) {
    free(poly);
    *result = POLYHEDRON_VERTEX_MALLOC_ERROR;
    return NULL;
  }
  poly->poly = malloc(sizeof(PolygonIndexed) * nf);
  if (!poly->poly) {
    free(poly->vertices);
    free(poly);
    *result = POLYHEDRON_FACE_MALLOC_ERROR;
    return NULL;
  }
  return poly;
}

/**
 * @brief Do not touch
 * @param[out] result You know
 * @param fin The file
 * @param buf The output
 * @return nothing
 */

void read_clean_line(CanimResult *result, FILE *fin, char *buf) {
  for (; fgets(buf, BUFFER_SIZE, fin);) {
    buf[strcspn(buf, "\r\n")] = 0;
    char *h = strchr(buf, '#');
    if (h) {
      *h = 0;
    }
    char *p = buf + strspn(buf, " \t");
    size_t n = strlen(p);
    for (; n && (p[n - 1] == ' ' || p[n - 1] == '\t');) {
      p[--n] = 0;
    }
    if (*p == 0) {
      continue;
    }
    if (p != buf) {
      memmove(buf, p, n + 1);
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

void read_off_header(CanimResult *result, FILE *fin, int *nv, int *nf) {
  char line[BUFFER_SIZE];
  read_clean_line(result, fin, line);
  if (IS_AN_ERROR(*result)) {
    return;
  }
  if (strcmp(line, "OFF") != 0) {
    *result = OFF_HEADER_OFF_ERROR;
    return;
  }
  read_clean_line(result, fin, line);
  if (IS_AN_ERROR(*result)) {
    return;
  }
  char *t = strtok(line, " \t");
  if (!t) {
    *result = OFF_HEADER_DATA_ERROR;
    return;
  }
  *nv = atoi(t);
  t = strtok(null, " \t");
  if (!t) {
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

void read_vertex(CanimResult *result, FILE *fin, Polyhedron *poly,
                 int vertex_idx) {
  char line[BUFFER_SIZE];
  read_clean_line(result, fin, line);
  if (IS_AN_ERROR(*result)) {
    return;
  }
  char *t = strtok(line, " \t");
  if (!t) {
    *result = OFF_VERTEX_ERROR;
    return;
  }
  poly->vertices[vertex_idx].x = atof(t);
  t = strtok(null, " \t");
  if (!t) {
    *result = OFF_VERTEX_ERROR;
    return;
  }
  poly->vertices[vertex_idx].y = atof(t);
  t = strtok(null, " \t");
  if (!t) {
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

void read_face(CanimResult *result, FILE *fin, Polyhedron *poly, int face_idx) {
  char line[BUFFER_SIZE];
  read_clean_line(result, fin, line);
  if (IS_AN_ERROR(*result)) {
    return;
  }
  char *t = strtok(line, " \t");
  if (!t) {
    *result = OFF_FACE_ERROR;
    return;
  }
  poly->poly[face_idx].vertex_count = atoi(t);
  poly->poly[face_idx].vertices =
      malloc(sizeof(int) * poly->poly[face_idx].vertex_count);
  for (int i = 0; i < poly->poly[face_idx].vertex_count; i++) {
    t = strtok(null, " \t");
    if (!t) {
      *result = OFF_FACE_ERROR;
      return;
    }
    poly->poly[face_idx].vertices[i] = atoi(t);
  }
  poly->poly[face_idx].fd.normal =
      normal_vec3(poly->vertices[poly->poly[face_idx].vertices[0]],
                  poly->vertices[poly->poly[face_idx].vertices[1]],
                  poly->vertices[poly->poly[face_idx].vertices[2]]);
  poly->poly[face_idx].fd.color.rgba =
      (uint32_t)((float)face_idx * (float)0xff / ((float)poly->face_count));
  *result = SUCCESS;
}

/**
 * @brief Read OFF File and parse it into a polyhedron
 * @param[out] result The error code
 * @param fin
 * @return The polyhedron
 */

Polyhedron *read_off_into_polyhedron(CanimResult *result, FILE *fin) {
  int nv = 0;
  int nf = 0;
  read_off_header(result, fin, &nv, &nf);
  if (IS_AN_ERROR(*result)) {
    return null;
  }
  Polyhedron *poly = create_polyhedron(result, nv, nf);
  if (IS_AN_ERROR(*result)) {
    return null;
  }
  for (int i = 0; i < nv; i++) {
    read_vertex(result, fin, poly, i);
    if (IS_AN_ERROR(*result)) {
      free_polyhedron(poly);
      return null;
    }
  }
  for (int i = 0; i < nf; i++) {
    read_face(result, fin, poly, i);
    if (IS_AN_ERROR(*result)) {
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

void le32_bytes(uint32_t v, unsigned char b[4]) {
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

void lef32_bytes(float v, unsigned char b[4]) {
  le32_bytes(*(uint32_t *)&v, b);
}

/**
 * @brief Writes a triangulation into an stl
 * @param[out] result
 * @param tri The triangulation
 * @param file The file
 * @return nothing
 */

void write_to_stl(CanimResult *result, Triangulation *tri, FILE *fin) {
  unsigned char header[80];
  memset(header, 0, sizeof(header));
  if (fwrite(header, 1, 80, fin) != 80) {
    *result = STL_HEADER_WRITE_ERROR;
    return;
  }
  unsigned char h[4];
  le32_bytes((uint32_t)tri->triangle_count, h);
  if (fwrite(h, 1, 4, fin) != 4) {
    *result = STL_HEADER_WRITE_ERROR;
    return;
  }
  for (int i = 0; i < tri->triangle_count; i++) {
    Vec3 a = tri->triangles[i].vertices[0];
    Vec3 b = tri->triangles[i].vertices[1];
    Vec3 c = tri->triangles[i].vertices[2];
    Vec3 n = tri->triangles[i].fd.normal;
    lef32_bytes(n.x, h);
    if (fwrite(h, 1, 4, fin) != 4) {
      *result = STL_VECTOR_WRITE_ERROR;
      return;
    }
    lef32_bytes(n.y, h);
    if (fwrite(h, 1, 4, fin) != 4) {
      *result = STL_VECTOR_WRITE_ERROR;
      return;
    }
    lef32_bytes(n.z, h);
    if (fwrite(h, 1, 4, fin) != 4) {
      *result = STL_VECTOR_WRITE_ERROR;
      return;
    }
    lef32_bytes(a.x, h);
    if (fwrite(h, 1, 4, fin) != 4) {
      *result = STL_VECTOR_WRITE_ERROR;
      return;
    }
    lef32_bytes(a.y, h);
    if (fwrite(h, 1, 4, fin) != 4) {
      *result = STL_VECTOR_WRITE_ERROR;
      return;
    }
    lef32_bytes(a.z, h);
    if (fwrite(h, 1, 4, fin) != 4) {
      *result = STL_VECTOR_WRITE_ERROR;
      return;
    }
    lef32_bytes(b.x, h);
    if (fwrite(h, 1, 4, fin) != 4) {
      *result = STL_VECTOR_WRITE_ERROR;
      return;
    }
    lef32_bytes(b.y, h);
    if (fwrite(h, 1, 4, fin) != 4) {
      *result = STL_VECTOR_WRITE_ERROR;
      return;
    }
    lef32_bytes(b.z, h);
    if (fwrite(h, 1, 4, fin) != 4) {
      *result = STL_VECTOR_WRITE_ERROR;
      return;
    }
    lef32_bytes(c.x, h);
    if (fwrite(h, 1, 4, fin) != 4) {
      *result = STL_VECTOR_WRITE_ERROR;
      return;
    }
    lef32_bytes(c.y, h);
    if (fwrite(h, 1, 4, fin) != 4) {
      *result = STL_VECTOR_WRITE_ERROR;
      return;
    }
    lef32_bytes(c.z, h);
    if (fwrite(h, 1, 4, fin) != 4) {
      *result = STL_VECTOR_WRITE_ERROR;
      return;
    }
    unsigned char attr[2] = {0, 0};
    if (fwrite(attr, 1, 2, fin) != 2) {
      *result = STL_VECTOR_WRITE_ERROR;
      return;
    }
  }
  *result = SUCCESS;
}

/**
 * @brief Uploads and draws a triangulation using shaders.
 * @param[out] result If any fail occured.
 * @param prog The shader program (with "position" and "normal" attributes).
 * @param tri  The triangulation to draw.
 */
void draw_triangulation(CanimResult *result, GLuint prog, Triangulation *tri) {
  const size_t vertex_size = 28;
  const size_t total_bytes = tri->triangle_count * 3 * vertex_size;

  uint8_t *data = malloc(total_bytes);
  if (!data) {
    *result = DRAW_TRIANGULATION_MALLOC_ERROR;
    return;
  }

  uint8_t *p = data;
  for (int i = 0; i < tri->triangle_count; i++) {
    Vec3 a = tri->triangles[i].vertices[0];
    Vec3 b = tri->triangles[i].vertices[1];
    Vec3 c = tri->triangles[i].vertices[2];
    Vec3 n = tri->triangles[i].fd.normal;
    uint32_t col = tri->triangles[i].fd.color.rgba;

    Vec3 verts[3] = {a, b, c};
    for (int v = 0; v < 3; v++) {
      *(float *)(p + 0) = verts[v].x;
      *(float *)(p + 4) = verts[v].y;
      *(float *)(p + 8) = verts[v].z;
      *(float *)(p + 12) = n.x;
      *(float *)(p + 16) = n.y;
      *(float *)(p + 20) = n.z;
      *(uint32_t *)(p + 24) = col;
      p += vertex_size;
    }
  }

  GLuint vao, vbo;
  pglGenVertexArrays(1, &vao);
  pglBindVertexArray(vao);

  pglGenBuffers(1, &vbo);
  pglBindBuffer(GL_ARRAY_BUFFER, vbo);
  pglBufferData(GL_ARRAY_BUFFER, total_bytes, data, GL_STATIC_DRAW);
  free(data);

  GLint posLoc = pglGetAttribLocation(prog, "position");
  GLint normLoc = pglGetAttribLocation(prog, "normal");
  GLint colorLoc = pglGetAttribLocation(prog, "color");

  if (posLoc >= 0) {
    pglEnableVertexAttribArray(posLoc);
    pglVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 28, (void *)0);
  }
  if (normLoc >= 0) {
    pglEnableVertexAttribArray(normLoc);
    pglVertexAttribPointer(normLoc, 3, GL_FLOAT, GL_FALSE, 28, (void *)12);
  }
  if (colorLoc >= 0) {
    pglEnableVertexAttribArray(colorLoc);
    pglVertexAttribPointer(colorLoc, 4, GL_UNSIGNED_BYTE, GL_TRUE, 28,
                           (void *)24);
  }

  pglUseProgram(prog);
  glDrawArrays(GL_TRIANGLES, 0, tri->triangle_count * 3);

  pglBindVertexArray(0);
  pglDeleteBuffers(1, &vbo);
  pglDeleteVertexArrays(1, &vao);
}

/**
 * @brief the main function lol
 * @param argc lol
 * @param argv lol
 * @return nothinf
 */

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "I wish for two parameters!");
    return 1;
  }

  CanimResult result = SUCCESS;
  FILE *fin = fopen(argv[1], "r");
  Polyhedron *poly = read_off_into_polyhedron(&result, fin);
  fclose(fin);

  if (IS_AN_ERROR(result)) {
    print_error(result);
    return 1;
  }
  Triangulation *tri = empty_triangulation(&result);
  if (IS_AN_ERROR(result)) {
    print_error(result);
    return 1;
  }

  triangulate_polyhedron(&result, poly, tri);
  if (IS_AN_ERROR(result)) {
    print_error(result);
    return 1;
  }
  FILE *fin2 = fopen("e.stl", "wb");
  write_to_stl(&result, tri, fin2);
  if (IS_AN_ERROR(result)) {
    print_error(result);
    return 1;
  }
  fclose(fin2);

  SDL_Init(SDL_INIT_VIDEO);

  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  SDL_Window *win =
      SDL_CreateWindow("Canim", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

  SDL_GLContext ctx = SDL_GL_CreateContext(win);
  load_gl_shader_functions(&result);
  if (IS_AN_ERROR(result)) {
    print_error(result);
    return 1;
  }
  GLuint prog =
      create_shader_program(&result, triangulation_vs, triangulation_fs);
  if (IS_AN_ERROR(result)) {
    print_error(result);
    return 1;
  }

  glEnable(GL_MULTISAMPLE);
  glEnable(GL_DEPTH_TEST);
  SDL_Event e;
  float angle = 0;
  int running = 1;
  FILE *pipef = open_ffmpeg_pipe(800, 600, 60, "out.mp4");
  if (!pipef) {
    fprintf(stderr, "Failed to open ffmpeg pipe\n");
    return 1;
  }
  for (; running;) {
    for (; SDL_PollEvent(&e);) {
      if (e.type == SDL_QUIT) {
        running = 0;
      }
    }
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = 800.0f / 600.0f;
    glFrustum(-1 * aspect, 1 * aspect, -1, 1, 1, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -3);
    angle += 1;
    glRotatef(angle, 1, 1, 0);
    pglUseProgram(prog);
    draw_triangulation(&result, prog, tri);
    pglUseProgram(0);

    SDL_GL_SwapWindow(win);
    int w = 800, h = 600;
    CanimResult frame_result;
    unsigned char *rgb = get_framebuffer_rgb(&frame_result, w, h, null);
    if (frame_result == SUCCESS && rgb) {
      fwrite(rgb, 1, w * h * 3, pipef);
    }
    free(rgb);
  }
  close_ffmpeg_pipe(pipef);
  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
