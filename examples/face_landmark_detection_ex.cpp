// The contents of this file are in the public domain. See LICENSE_FOR_EXAMPLE_PROGRAMS.txt
/*

    This example program shows how to find frontal human faces in an image and
    estimate their pose.  The pose takes the form of 68 landmarks.  These are
    points on the face such as the corners of the mouth, along the eyebrows, on
    the eyes, and so forth.  
    


    The face detector we use is made using the classic Histogram of Oriented
    Gradients (HOG) feature combined with a linear classifier, an image pyramid,
    and sliding window detection scheme.  The pose estimator was created by
    using dlib's implementation of the paper:
       One Millisecond Face Alignment with an Ensemble of Regression Trees by
       Vahid Kazemi and Josephine Sullivan, CVPR 2014
    and was trained on the iBUG 300-W face landmark dataset (see
    https://ibug.doc.ic.ac.uk/resources/facial-point-annotations/):  
       C. Sagonas, E. Antonakos, G, Tzimiropoulos, S. Zafeiriou, M. Pantic. 
       300 faces In-the-wild challenge: Database and results. 
       Image and Vision Computing (IMAVIS), Special Issue on Facial Landmark Localisation "In-The-Wild". 2016.
    You can get the trained model file from:
    http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2.
    Note that the license for the iBUG 300-W dataset excludes commercial use.
    So you should contact Imperial College London to find out if it's OK for
    you to use this model file in a commercial product.


    Also, note that you can train your own models using dlib's machine learning
    tools.  See train_shape_predictor_ex.cpp to see an example.

    


    Finally, note that the face detector is fastest when compiled with at least
    SSE2 instructions enabled.  So if you are using a PC with an Intel or AMD
    chip then you should enable at least SSE2 instructions.  If you are using
    cmake to compile this program you can enable them by using one of the
    following commands when you create the build project:
        cmake path_to_dlib_root/examples -DUSE_SSE2_INSTRUCTIONS=ON
        cmake path_to_dlib_root/examples -DUSE_SSE4_INSTRUCTIONS=ON
        cmake path_to_dlib_root/examples -DUSE_AVX_INSTRUCTIONS=ON
    This will set the appropriate compiler options for GCC, clang, Visual
    Studio, or the Intel compiler.  If you are using another compiler then you
    need to consult your compiler's manual to determine how to enable these
    instructions.  Note that AVX is the fastest but requires a CPU from at least
    2011.  SSE4 is the next fastest and is supported by most current machines.  
*/


#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>
#include <dlib/gui_widgets.h>
#include <dlib/image_io.h>
#include <iostream>

#include "blobrectf.h"
#include "seektypes.h"

#define SHOW_GUI            // ~360KB
#define USE_LOAD_IMAGE      // ~160KB

#define BACKDOOR
//#define HALLWAY
//#define TRIPOD

#ifdef WIN32
#define CLOCK_GETTIME(t)    timespec_get(t, TIME_UTC)
#else
#define CLOCK_GETTIME(t)    clock_gettime(CLOCK_REALTIME, t)
#include <linux/limits.h>
#define MAX_PATH		PATH_MAX
#endif

using namespace dlib;
using namespace std;

// ----------------------------------------------------------------------------------------

typedef struct imageattr_t
{
    seekrect_t refRect;
    float refMax;
    float refMean;

    uint32_t faceTime;
    seekrect_t faceRect;

    uint32_t shapeTime;
    seekpoint_t leftOuter;
    seekpoint_t leftInner;
    seekpoint_t rightInner;
    seekpoint_t rightOuter;
    seekpoint_t nose;
    seekpoint_t leftThermal;
    seekpoint_t rightThermal;
    float leftTemp;
    float rightTemp;
} IMAGEATTR_T;

bool shrink_image = false;

#define SCALE_8K    		    6.55
#define SCALE_32K               (SCALE_8K / 2)

#define THERM_FACE_SIZE_8K       18
#define THERM_FACE_SIZE_32K      (THERM_FACE_SIZE_8K*2)

#if defined(BACKDOOR)
float OFFSET_X = -1.0;
float OFFSET_Y = +7.0;
#elif defined(HALLWAY)
float OFFSET_X = +3.0;
float OFFSET_Y = -10.0;
#elif defined(TRIPOD)
float OFFSET_X = +3.0;
float OFFSET_Y = -3.0;
#else
#error Must define BACKDOOR, HALLWAY or TRIPOD
#endif

float SCALE = SCALE_8K;
int THERM_FACE_SIZE = THERM_FACE_SIZE_8K;

#define VIS_TO_THERM_X(x)	((int) ((x) / SCALE + OFFSET_X + 0.5f))
#define VIS_TO_THERM_Y(y)	((int) ((y) / SCALE + OFFSET_Y + 0.5f))

#define THERM_TO_VIS_X(x)	((int) (((x) - OFFSET_X) * SCALE + 0.5f))
#define THERM_TO_VIS_Y(y)	((int) (((y) - OFFSET_Y) * SCALE + 0.5f))

#define THERM_MAX_COLS	320
#define THERM_MAX_ROWS	240

char folder[MAX_PATH];

int diameter = 2;

size_t widthFromPixels(size_t pixels)
{
    switch (pixels) {
    case 103 * 78:
        SCALE = shrink_image ? SCALE_8K / 2 : SCALE_8K;
        THERM_FACE_SIZE = THERM_FACE_SIZE_8K;
        return 103;
    case 206 * 156:
        SCALE = shrink_image ? SCALE_32K / 2 : SCALE_32K;
        THERM_FACE_SIZE = THERM_FACE_SIZE_32K;
        return 206;
    case 320 * 240:
        return 320;
    }
    return 0;
}

float findCanthus(array2d<float>& therm_image, int xCenter, int yCenter)
{
    int xMax = xCenter;
    int yMax = yCenter;
    float maxTemp = therm_image[yMax][xMax];
    for (int yoff = 0; yoff < diameter; ++yoff) {
        int y = yCenter + yoff;
        if (y < therm_image.nr()) {
            for (int xoff = 0; xoff < diameter; ++xoff) {
                int x = xCenter + xoff;
                if (x < therm_image.nc()) {
                    float temp = therm_image[yMax][xMax];
                    if (temp > maxTemp) {
                        xMax = x;
                        yMax = y;
                        maxTemp = temp;
                    }
                }
            }
        }
    }
    fprintf(stdout, "findCanthus(%d,%d)=%0.2f\n", xMax, yMax, maxTemp);
    return maxTemp;
}

size_t read_therm_image(const char* pathname, array2d<float>& image)
{
    FILE* fp = fopen(pathname, "rb");
    if (fp == NULL) {
        fprintf(stderr, "ERROR opening file %s!\n", pathname);
        return 0;
    }
    // Get size of file in bytes
    fseek(fp, 0L, SEEK_END);
    size_t nbytes = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    size_t npixels = nbytes / sizeof(float);
    int width = widthFromPixels(npixels);
    if (width > 0) {
        int height = npixels / width;
        image.set_size(height, width);
        /// Read pixels into data buffer
        size_t nread = fread((void*) &image[0][0], sizeof(float), npixels, fp);
    }
    fclose(fp);
    return width;
}

bool process_frame(imageattr_t& image, array2d<float>& therm_image)
{
    int leftX = VIS_TO_THERM_X(image.leftInner.x);
    int leftY = VIS_TO_THERM_Y(image.leftInner.y);
    int rightX = VIS_TO_THERM_X(image.rightInner.x);
    int rightY = VIS_TO_THERM_Y(image.rightInner.y);
    bool result = false;
    image.leftTemp = NAN;
    image.rightTemp = NAN;
    if (therm_image.nc() > 0 && therm_image.nr() > 0) {
        image.leftThermal.x = leftX;
        image.leftThermal.y = leftY;
        image.rightThermal.x = rightX;
        image.rightThermal.y = rightY;
        image.leftTemp = findCanthus(therm_image, leftX, leftY);
        image.rightTemp = findCanthus(therm_image, rightX - diameter, rightY);
        result = true;
    }
    return result;
}

#define MAX_FACES       5
#define MAX_BLOBS       5

#define REF_INFLATE     2

// Internal function to return [Blob] Sort Key
static int32_t BlobSortByArea(seekrect_t* a, seekrect_t* b)
{
    int32_t area_a = a->width * a->height;
    int32_t area_b = b->width * b->height;
    return area_b - area_a;
}

void inflateThermRect(seekrect_t& rect, int inflate, int width, int height) {
    // Inflate refRect by REF_INFLATE pixels (each side)
    int refLeft = rect.x;
    int refTop = rect.y;
    int refWidth = rect.width;
    int refHeight = rect.height;

    if (refLeft >= inflate) {
        refLeft -= inflate;
    }
    if (refTop >= inflate) {
        refTop -= inflate;
    }
    if (refLeft + refWidth + inflate * 2 <= width) {
        refWidth += inflate * 2;
    }
    if (refTop + refHeight + inflate * 2 <= height) {
        refHeight += inflate * 2;
    }
    rect.x = refLeft;
    rect.y = refTop;
    rect.width = refWidth;
    rect.height = refHeight;
}

#ifdef SHOW_GUI
extern "C" int find_face(image_window& win, frontal_face_detector & detector, shape_predictor & sp, const char* filename, imageattr_t& image)
#else
extern "C" int find_face(frontal_face_detector& detector, shape_predictor& sp, const char* filename, imageattr_t& image])
#endif
{
    // Output "index" and filename
    array2d<rgb_pixel> img;
#ifdef USE_LOAD_IMAGE
    load_image(img, filename);
#else
    load_png(img, filename);
#endif
    int cols = img.nc();
    int rows = img.nr();
    if (cols < 320 && rows < 240) {
        // Make the image larger so we can detect small faces.
        cout << "pyramid up " << cols << "x" << rows << " ";
        pyramid_up(img);
    } else if (shrink_image) {
        // shrink to QVGA
        cout << "resize_image " << cols << "x" << rows << " ";
        resize_image(0.5, img);
    }

    // Try to read corresponding thermal frame into thermbuf
    int n = 0;
    // Look at chars before extension
    const char* ptr = strrchr(filename, '.');
    if (ptr != NULL) {
        char ch = ptr[-1];
        while (isdigit(ch)) {
            --ptr;
            ch = ptr[-1];
        }
        n = atoi(ptr);
    }
    rectangle faceBlob;
    image.refRect.x = image.refRect.y = 0;
    image.refRect.width = image.refRect.height = 0;
    image.refMean = 0.0f;
    image.refMax = -273.15;

    array2d<float> therm_image;

    char pathname[MAX_PATH];
    sprintf(pathname, "Therm%04d.bin", n);
    size_t therm_width = read_therm_image(pathname, therm_image);
    if (therm_width > 0) {
        seekrect_t blobs[MAX_BLOBS];
        // Detect reference black body in thermal
        int blobCount = BlobRectF(&therm_image[0][0], NULL, therm_image.nc(), therm_image.nr(), 35.0f, blobs, MAX_BLOBS);
        if (blobCount > 0) {
            int thermLeft = blobs[0].x;
            int thermTop = blobs[0].y;
            int thermWidth = blobs[0].width;
            int thermHeight = blobs[0].height;
            image.refRect.x = thermLeft;
            image.refRect.y = thermTop;
            image.refRect.width = thermWidth;
            image.refRect.height = thermHeight;
            // Find max & mean of reference Black Body
            float sum = 0.0;
            for (int y = thermTop; y < thermTop + thermHeight; ++y) {
                for (int x = thermLeft; x < thermLeft + thermWidth; ++x) {
                    float temp = therm_image[y][x];
                    if (temp > image.refMax) {
                        image.refMax = temp;
                    }
                    sum += temp;
                }
            }
            // Calculate average black body temperature
            image.refMean = sum / (image.refRect.width * image.refRect.height);
        }
    }

    // Now tell the face detector to give us a list of bounding boxes
    // around all the faces in the image.
    struct timespec start_time, end_time;
    
    double adjust = -0.5;
    std::vector<std::pair<double, rectangle> > final_dets;
    CLOCK_GETTIME(&start_time);
    detector(img, final_dets, adjust);
    CLOCK_GETTIME(&end_time);
    uint32_t faceTime = (end_time.tv_nsec + 1000000000 - start_time.tv_nsec) % 1000000000;
    size_t nfaces = final_dets.size();
    size_t best_face = 0;
    double best_conf = 0.0;
    // Find face with highest confidence level
    for (size_t u = 0; u < nfaces; ++u) {
        double confidence = final_dets[u].first;
        if (confidence > best_conf) {
            best_face = u;
            best_conf = confidence;
        }
    }
#ifdef SHOW_GUI
    // Now let's view our image on the screen.
    win.clear_overlay();
    win.set_image(img);
#endif

    cout << setprecision(2) << fixed;
    cout << "Number of faces detected: " << nfaces << " best[" << best_face << "]=" << best_conf << endl;


    // Now we will go ask the shape_predictor to tell us the pose of
    // each face we detected.
    std::vector<full_object_detection> shapes;
    if (nfaces == 0) {
        seekrect_t excludeRect;
        excludeRect = image.refRect;
        inflateThermRect(excludeRect, REF_INFLATE, therm_image.nc(), therm_image.nr());

        // Try to locate face blob in thermal
        seekrect_t blobs[MAX_BLOBS];
        int blobCount = BlobRectF(&therm_image[0][0], &excludeRect, therm_image.nc(), therm_image.nr(), 30.0f, blobs, MAX_BLOBS);
        if (blobCount > 0) {
            // Sort blobs by size (area)
            BlobSort(blobs, BlobSortByArea, blobCount);
            int thermLeft = blobs[0].x;
            int thermTop = blobs[0].y;
            int thermWidth = blobs[0].width;
            int thermHeight = blobs[0].height;
            if (thermWidth >= THERM_FACE_SIZE && thermHeight >= THERM_FACE_SIZE) {
                faceBlob.set_left(THERM_TO_VIS_X(thermLeft));
                faceBlob.set_top(THERM_TO_VIS_Y(thermTop));
                faceBlob.set_right(THERM_TO_VIS_X(thermLeft + thermWidth - 1));
                faceBlob.set_bottom(THERM_TO_VIS_Y(thermTop + thermHeight - 1));
                final_dets.push_back(std::make_pair(0.0, faceBlob));
                nfaces = 1;
                fprintf(stdout, "Visible Faces=%d\t%ld,%ld\t%lu,%lu\n", blobCount, faceBlob.left(), faceBlob.top(), faceBlob.width(), faceBlob.height());
            }
            else {
                fprintf(stdout, "FILTER Thermal Faces=%d\t%d,%d\t%d,%d\n", blobCount, thermLeft, thermTop, thermWidth, thermHeight);
            }
        }
    } else if (nfaces > MAX_FACES) {
        // Limit nfaces to prevent array subscript
        nfaces = MAX_FACES;
    }

    if (nfaces > 0) {

        CLOCK_GETTIME(&start_time);
        full_object_detection shape = sp(img, final_dets[best_face].second);
        CLOCK_GETTIME(&end_time);
        uint32_t shapeTime = (end_time.tv_nsec + 1000000000 - start_time.tv_nsec) % 1000000000;

        // save best face rect in image
        image.faceTime = faceTime / 1000;
        image.shapeTime = shapeTime / 1000;
        seekrect_t& rect = image.faceRect;
        rect.x = final_dets[best_face].second.left();
        rect.y = final_dets[best_face].second.right();
        rect.width = final_dets[best_face].second.width();
        rect.height = final_dets[best_face].second.height();

        int num_parts = shape.num_parts();
        cout << "number of parts: " << num_parts << endl;

        if (num_parts >= 5) {
            image.rightOuter.x = shape.part(0).x();
            image.rightOuter.y = shape.part(0).y();
            image.rightInner.x = shape.part(1).x();
            image.rightInner.y = shape.part(1).y();
            image.leftInner.x = shape.part(2).x();
            image.leftInner.y = shape.part(2).y();
            image.leftOuter.x = shape.part(3).x();
            image.leftOuter.y = shape.part(3).y();
            image.nose.x = shape.part(4).x();
            image.nose.y = shape.part(4).y();
        }
        // You get the idea, you can get all the face part locations if
        // you want them.  Here we just store them in shapes so we can
        // put them on the screen.
        shapes.push_back(shape);

        image.leftTemp = NAN;
        image.rightTemp = NAN;
        bool have_temp = process_frame(image, therm_image);
#ifdef SHOW_GUI
        // Show face rectangle in RED
        win.add_overlay(final_dets[best_face].second, rgb_pixel(255, 0, 0));
        // Show face features in GREEN
        win.add_overlay(render_face_detections(shapes));
#endif
    }
#ifdef DEBUG
    cout << "Hit enter to process the next image..." << endl;
    cin.get();
#endif
    return nfaces;
}

void parse_option(const char* str)
{
    char ch = *str++;
    switch (ch) {
    case 'x':
        OFFSET_X = atof(str);
        break;
    case 'y':
        OFFSET_Y = atof(str);
        break;
    case 's':
        shrink_image = true;
        break;
    default:
        fprintf(stderr, "ERROR unknown option '%c'!", ch);
    }
}

int main(int argc, char** argv)
{  
    try
    {
        // This example takes in a shape model file and then a list of images to
        // process.  We will take these filenames in as command line arguments.
        // Dlib comes with example images in the examples/faces folder so give
        // those as arguments to this program.
        if (argc == 1)
        {
            cout << "Call this program like this:" << endl;
            cout << "./face_landmark_detection_ex shape_predictor_68_face_landmarks.dat faces/*.jpg" << endl;
            cout << "\nYou can get the shape_predictor_68_face_landmarks.dat file from:\n";
            cout << "http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2" << endl;
            return 0;
        }

        // We need a face detector.  We will use this to get bounding boxes for
        // each face in an image.
        frontal_face_detector detector = get_frontal_face_detector();
        // And we also need a shape_predictor.  This is the tool that will predict face
        // landmark positions given an image and face bounding box.  Here we are just
        // loading the model from the shape_predictor_68_face_landmarks.dat file you gave
        // as a command line argument.
        shape_predictor sp;
        deserialize(argv[1]) >> sp;

#ifdef SHOW_GUI
        image_window win;
#endif
        ofstream ostream;
        ostream.open("dlib_data.txt");

        // Loop over all the images provided on the command line.
        for (int i = 2; i < argc; ++i)
        {
            const char* filename =  argv[i];

            if (filename[0] == '-') {
                parse_option(filename + 1);
                continue;
            }

            cout << "processing image " << filename << endl;

            imageattr_t image;
#ifdef SHOW_GUI
            int nfaces = find_face(win, detector, sp, filename, image);
#else
            int nfaces = find_face(detector, sp, filename, visData);
#endif
            // Always output filename, refRect, refMax
            ostream << setprecision(2) << fixed;
            ostream << "0\t" << filename << "\t" << image.refRect.x << "," << image.refRect.y << "\t" 
                    << image.refRect.width << "," << image.refRect.height << "\t" << image.refMax;
            if (nfaces > 0) {
                seekrect_t& rect = image.faceRect;
                ostream << "\t" << image.faceTime << "\t" <<
                    rect.x << "," << rect.y << "\t" << rect.width << "," << rect.height << "\t" << image.shapeTime << "\t" <<
                    image.leftOuter.x << "," << image.leftOuter.y << "\t" << image.leftInner.x << "," << image.leftInner.y << "\t" <<
                    image.rightInner.x << "," << image.rightInner.y << "\t" << image.rightOuter.x << "," << image.rightOuter.y << "\t" <<
                    image.nose.x << "," << image.nose.y;
                if (!isnan(image.leftTemp) && !isnan(image.rightTemp)) {
                    ostream << "\t" << image.leftThermal.x << "," << image.leftThermal.y << "\t" << image.rightThermal.x << "," << image.rightThermal.y;
                    ostream << "\t" << image.leftTemp << "," << image.rightTemp;
                }
            }
            ostream << endl;
        }
        ostream.close();
    }
    catch (exception& e)
    {
        cout << "\nexception thrown!" << endl;
        cout << e.what() << endl;
    }
}

// ----------------------------------------------------------------------------------------

