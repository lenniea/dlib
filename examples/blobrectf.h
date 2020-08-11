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

#include "seektypes.h"

typedef enum blob_status
{
    BLOB_INVALID_DEPTH = -2,
    BLOB_PARAMETER_ERROR = -1,
    BLOB_NONE = 0,
} BLOB_STATUS;

int STDCALL BlobRectF(const float* image, const seekrect_t* excludeRect, int width, int height, float thresh, seekrect_t* pBlob, int count);

typedef int32_t BlobCompare(seekrect_t* a, seekrect_t* b);

void STDCALL BlobSort(seekrect_t* pRect, BlobCompare func, int count);

#ifdef DEBUG
    void DumpBlobs(seekrect_t* pBlob, int result);
#endif

#ifdef __cplusplus
}
#endif

#endif
