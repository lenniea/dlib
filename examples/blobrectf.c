#include "blobrectf.h"

#define BLOB_MIN        4

/*
 *  Internal function to check if Blob already exists in 
 *  array of Blob seekrect_t
 * 
 *  @param  pBlob       --> array of Blob seekrect_t
 *  @param  count       count of Blob seekrect_t
 *  @param  row         Row of blob (y)
 *  @param  left        Left column of Blob (x1)
 *  @param  right       Right column of Blob (x2)
 */
static int CheckBlob(seekrect_t* pBlob, size_t count, int row, int left, int right)
{
    size_t i;
    if (right - left < BLOB_MIN) {
        return 1;
    }
    for (i = 0; i < count; ++i) {
        int blobLeft = pBlob[i].x;
        int blobRight = blobLeft + pBlob[i].width;
        int blobTop = pBlob[i].y;
        int blobBottom = blobTop + pBlob[i].height;
        if (row >= blobTop && row < blobBottom && left >= blobLeft && right < blobRight) {
            return 1;
        }
    }
    return 0;
}

// Internal 8-bit function to find start of Blob on a row
static int StartBlobF(float* pRow, int col, int width, float thresh)
{
    // Detect an edge in the row
    while (col < width) {
        if (pRow[col] >= thresh) {
            return col;
        }
        ++col;
    }
    return -1;
}

// Internal 8-bit function to find end of Blob on a row
static int EndBlobF(float* pRow, int col, int width, float thresh)
{
    // Detect an edge in the row
    while (col < width) {
        if (pRow[col] < thresh) {
            return col;
        }
        ++col;
    }
    return width;
}

// Internal float function to scan row for for next blob starting at col
static int ScanBlobF(float* image, int width, float thresh, int row, int col, int* startBlob)
{
    // Scan for start of blob
    float* pRow = image + row * width;
    int start = StartBlobF(pRow, col, width, thresh);
    if (start >= 0) {
        *startBlob = start;
        // Scan for end of blob
        int endBlob = EndBlobF(pRow, *startBlob, width, thresh);
        return endBlob;
    }
    return -1;
}

/**
 *  @brief float Blob Detection
 * 
 *  This function performs float Blob Detection on a 2D Image buffer.
 * 
 *  @param image        --> float pixel buffer
 *  @param width        Width of the image (pixels)
 *  @param height       Height of the image (pixels)
 *  @param thresh       threshold to detect blobs
 *  @param pBlob        --> array of seek_rect_t's to detect Blobs
 *  @param count        (Maximum) number of seekrect_t's to detect
 * 
 *  @return             Returns the number of Blobs detected or negative error code
 */
int STDCALL BlobRectF(float* image, int width, int height, float thresh, seekrect_t* pBlob, int count)
{
    int row;
    int blobCount = 0;

    for (row = 0; row < height; ++row) {
        float* pRow = image + row * width;

        int startBlob, startBlobNext;
        int endBlob = 0;
        // Scan for 1st blob on next 2 rows
        for (;;) {
            endBlob = ScanBlobF(image, width, thresh, row, endBlob, &startBlob);
            if (endBlob < 0) {
                break;
            }
            // Make sure this is a "new" blob start
            if (CheckBlob(pBlob, blobCount, row, startBlob, endBlob)){
                continue;
            }
            int endBlobNext = 0;
            int height = 1;
            int next = row + 1;
            for (;;) {
                endBlobNext = ScanBlobF(image, width, thresh, next, endBlobNext, &startBlobNext);
                if (endBlobNext < 0) {
                    int blobWidth = endBlob - startBlob;
                    if (blobCount < count && height > BLOB_MIN && blobWidth >= BLOB_MIN) {
                        pBlob[blobCount].x = startBlob;
                        pBlob[blobCount].y = row;
                        pBlob[blobCount].width = endBlob - startBlob + 1;
                        pBlob[blobCount].height = height;
                        ++blobCount;
                    }
                    break;
                }
                // Check for overlap between 1st row and this row
                if (endBlobNext< startBlob || startBlobNext > endBlob) {
                    continue;
                }
                if (startBlobNext < startBlob) {
                    startBlob = startBlobNext;
                }
                if (endBlobNext > endBlob) {
                    endBlob = endBlobNext;
                }
                // Extend Blob seekrect_t's
                ++height; ++next;
                endBlobNext = 0;
            }
        }
    }
    return blobCount;
}

// Internal function to return [Blob] Sort Key
static uint32_t SortKey(seekrect_t* pRect)
{
    int x = (pRect->x + pRect->width);           // drop bottom 3-bits for sorting
    int y = (pRect->y + pRect->height) >> 4;      // drop bottom 3-bits for sorting
    return x + (y << 16);
}

/**
 *  @brief  Sort Blobs
 *
 *  Sort Blob RECTangles in a 2D Image by row (Y) then column (X).
 *
 *  @param pRect    --> Blob seekrect_t's to sort
 *  @param count    # of seekrect_t's
 */
#define COPY_SEEK_RECT(a,b)  { (a)->x = (b)->x; (a)->y = (b)->y; (a)->width = (b)->width; (a)->height = (b)->height; }

void STDCALL BlobSort(seekrect_t* pRect, int count)
{
    // Dumb bubble sort for now...
    for (int i = 0; i < count - 1; ++i)
    {
        seekrect_t* pLeft = pRect + i;
        for (int j = i + 1; j < count; ++j)
        {
            seekrect_t* pRight = pRect + j;
            uint32_t left = SortKey(pLeft);
            uint32_t right = SortKey(pRight);
            if (left > right) {
                seekrect_t temp;
                COPY_SEEK_RECT(&temp, pLeft);
                COPY_SEEK_RECT(pLeft, pRight);
                COPY_SEEK_RECT(pLeft, &temp);
            }
        }
    }
}

#ifdef DEBUG

#include <stdio.h>

/**
 *  @brief  Dump Blobs
 * 
 *  Dump Blobs to stderr for debugging purposes
 * 
 *  @param pRect    --> Blob seekrect_t's to sort
 *  @param count    # of seekrect_t's
 */
void __stdcall DumpBlobs(seekrect_t* pBlobs, int result)
{
    fprintf(stderr, "DumpBlobs result=%d\n", result);
    if (result > 0) {
        printf("#\tLeft\tTop\tWidth\tHeight\n");
        for (int i = 0; i < result; ++i) {
            fprintf(stderr, "%d\t%d\t%d\t%d\t%d\n", i, pBlobs[i].x, pBlobs[i].y, pBlobs[i].width, pBlobs[i].height);
        }
    }
}

#endif
