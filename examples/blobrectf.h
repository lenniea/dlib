#ifndef __BLOBRECTF_H__
#define __BLOBRECTF_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#define STDCALL		__stdcall
#else
#define STDCALL
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

int STDCALL BlobRectF(float* image, int width, int height, float thresh, BLOB_RECT* pBlob, int count);

void STDCALL BlobSort(BLOB_RECT* pRect, int count);

#ifdef DEBUG
    void DumpBlobs(BLOB_RECT* pBlob, int result);
#endif

#ifdef __cplusplus
}
#endif

#endif
