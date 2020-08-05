#ifndef __BLOBRECTF_H__
#define __BLOBRECTF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>         // for uint8_t, uint16_t
#include <stdlib.h>         // for size_t

typedef enum blob_status
{
    BLOB_INVALID_DEPTH = -2,
    BLOB_PARAMETER_ERROR = -1,
    BLOB_NONE = 0,
} BLOB_STATUS;

typedef struct blob_rect
{
    int left;
    int top;
    int right;
    int bottom;
} BLOB_RECT;

int __stdcall BlobRectF(float* image, int width, int height, float thresh, BLOB_RECT* pBlob, int count);

void __stdcall BlobSort(BLOB_RECT* pRect, int count);

#ifdef DEBUG
    void DumpBlobs(BLOB_RECT* pBlob, int result);
#endif

#ifdef __cplusplus
}
#endif

#endif
