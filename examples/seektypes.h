#ifndef __SEEKTYPES_H__
#define __SEEKTYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

typedef int32_t seekdim_t;
typedef int32_t seekcoord_t;

/**
 *  Seek Size struct
 *
 *  This struct is (cast) compatible with Win32 SIZE
 * 
 *  | offset |     type       |    field    |
 *  |:------:|:--------------:|:-----------:|
 *  |    0   | @ref seekdim_t | @ref width  |
 *  |    4   | @ref seekdim_t | @ref height |
 */
typedef struct seeksize_t {
    seekdim_t width;         /*!< Width (like cx) */
    seekdim_t height;        /*!< Height (like cy) */
} seeksize_t;

/**
 *  Seek Point struct
 *
 *  This struct is (cast) compatible with Win32 POINT
 * 
 *  | offset |      type       |    field   |
 *  |:------:|:---------------:|:----------:|
 *  |   0    | @ref seekcoord_t|   @ref x   |
 *  |   4    | @ref seekcoord_t|   @ref y   |
 */
typedef struct seekpoint_t {
    seekcoord_t x;           /*!< x-coordinate */
    seekcoord_t y;           /*!< y-coordinate */
} seekpoint_t;

/**
 *  Seek Rectangle struct
 *
 *  This struct is (cast) compatible with .NET Rect
 * 
 *  | offset |       type       |    field    |
 *  |:------:|:----------------:|:-----------:|
 *  |   0    | @ref seekcoord_t |   @ref x    |
 *  |   4    | @ref seekcoord_t |   @ref y    |
 *  |   8    | @ref seekdim_t   | @ref width  |
 *  |   12   | @ref seekdim_t   | @ref height |
 */
typedef struct seekrect_t {
    seekcoord_t x;           /*!< x-coordinate */
    seekcoord_t y;           /*!< y-coordinate */
    seekdim_t width;         /*!< Width */
    seekdim_t height;        /*!< Height  */
} seekrect_t;

/**
 *  Seek Frame struct
 * 
 *  This is a generic (user) frame type which can be used for raw 8-bit, 
 *  16-bit frame data as well as 4-byte ARGB Color and float frames.  
 *  It supports an arbitray stride (width in bytes) as well as frame
 *  "attributes" by putting seekframe_t at the start of a user defined
 *  struct.
 * 
 *  | offset |      type      |         field        |
 *  |:------:|:--------------:|:--------------------:|
 *  |   0    |     size_t     |    @ref size         |
 *  |   4    | @ref seekdim_t |    @ref width        |
 *  |   8    | @ref seekdim_t |    @ref height       |
 *  |   12   |     size_t     |    @ref stride       |
 *  |   16   |     size_t     |    @ref elsize       |
 *  |   20   |    uint32_t    |    @ref timeStamp    |
 *  |   24   |    uint16_t    | @ref seekframe_pixel |
 */
typedef struct seekframe_t {
    size_t size;            /*!< sizeof(struct) */
    seekdim_t width;       /*!< Width (like cx) */
    seekdim_t height;      /*!< Height (like cy) */
    size_t stride;          /*!< Stride of array (in bytes) */
    size_t elsize;          /*!< Element Size (in bytes) */
    uint32_t timeStamp;     /*!< Time Stamp (usec) */
} seekframe_t;

// Dyanmic object constructors
seeksize_t* seeksize_constructor(seekdim_t wdth, seekdim_t height);
seekpoint_t* seekpoint_constructor(seekcoord_t x, seekcoord_t y);
seekrect_t* seekrect_constructor(seekcoord_t x, seekcoord_t y, seekdim_t width, seekdim_t height);
seekframe_t* seekframe_constructor( size_t size, seekdim_t wdith, seekdim_t height,  size_t stride, size_t elbytes);

// Dynamic frame data access
void* seekframe_pixel(seekframe_t* frame, seekcoord_t x, seekcoord_t y);

#ifdef __cplusplus
}
#endif

#endif /* __SEEKTYPES_H__ */