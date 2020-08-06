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

#define THERM_MAX_PIXELS		(THERM_MAX_ROWS * THERM_MAX_COLS)
float thermbuf[THERM_MAX_PIXELS];
seeksize_t thermSize;

int diameter = 2;

size_t widthFromPixels(size_t pixels)
{
    switch (pixels) {
    case 103 * 78:
        SCALE = SCALE_8K;
        THERM_FACE_SIZE = THERM_FACE_SIZE_8K;
        return 103;
    case 206 * 156:
        SCALE = SCALE_32K;
        THERM_FACE_SIZE = THERM_FACE_SIZE_32K;
        return 206;
    case 320 * 240:
        return 320;
    }
    return 0;
}

float findCanthus(float* thermbuf, seeksize_t frameSize, int xCenter, int yCenter)
{
    int xMax = xCenter;
    int yMax = yCenter;
    float maxTemp = thermbuf[xMax + yMax * frameSize.width];
    for (int yoff = 0; yoff < diameter; ++yoff) {
        int y = yCenter + yoff;
        if (y < frameSize.height) {
            for (int xoff = 0; xoff < diameter; ++xoff) {
                int x = xCenter + xoff;
                if (x < frameSize.width) {
                    float temp = thermbuf[x + y * frameSize.width];
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

size_t read_therm_frame(int n, float* thermbuf, seeksize_t& frame_size)
{
    char pathname[MAX_PATH];
    sprintf(pathname, "Therm%04d.bin", n);
    frame_size.width = frame_size.height = -1;
    FILE* fp = fopen(pathname, "rb");
    if (fp == NULL) {
        fprintf(stderr, "ERROR opening file %s!\n", pathname);
        return 0;
    }
    size_t nread = fread(thermbuf, sizeof(float), THERM_MAX_PIXELS, fp);
    fclose(fp);
    size_t width = widthFromPixels(nread);
    if (width > 0) {
        frame_size.height = nread / width;
        frame_size.width = width;
    }
    return width;
}

bool process_frame(imageattr_t& image)
{
    int leftX = VIS_TO_THERM_X(image.leftInner.x);
    int leftY = VIS_TO_THERM_Y(image.leftInner.y);
    int rightX = VIS_TO_THERM_X(image.rightInner.x);
    int rightY = VIS_TO_THERM_Y(image.rightInner.y);
    bool result = false;
    image.leftTemp = NAN;
    image.rightTemp = NAN;
    if (thermSize.width > 0 && thermSize.height > 0) {
        image.leftThermal.x = leftX;
        image.leftThermal.y = leftY;
        image.rightThermal.x = rightX;
        image.rightThermal.y = rightY;
        image.leftTemp = findCanthus(thermbuf, thermSize, leftX, leftY);
        image.rightTemp = findCanthus(thermbuf, thermSize, rightX - diameter, rightY);
        result = true;
    }
    return result;
}

#define MAX_FACES       5
#define MAX_BLOBS       5

#ifdef SHOW_GUI
extern "C" int find_face(image_window& win, frontal_face_detector & detector, shape_predictor & sp, const char* filename, imageattr_t imageData[MAX_FACES])
#else
extern "C" int find_face(frontal_face_detector& detector, shape_predictor& sp, const char* filename, SEEKIMAGE_T imageData[MAX_FACES])
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
    size_t therm_width = read_therm_frame(n, thermbuf, thermSize);

    // Now tell the face detector to give us a list of bounding boxes
    // around all the faces in the image.
    struct timespec start_time, end_time;
    std::vector<rectangle> dets;
    size_t nfaces;
    uint32_t faceTime;
    double adjust;
    for (adjust = 0.0; ; adjust -= 0.25) {
        CLOCK_GETTIME(&start_time);
        dets = detector(img, adjust);
        CLOCK_GETTIME(&end_time);
        faceTime = (end_time.tv_nsec + 1000000000 - start_time.tv_nsec) % 1000000000;
        nfaces = dets.size();
        if (nfaces > 0 || adjust <= -0.5)
            break;
    }

    cout << "Number of faces detected: " << nfaces;
    if (adjust != 0) {
        cout << " adjust=" << adjust << endl;
    } else {
        cout << endl;
    }

    // Now we will go ask the shape_predictor to tell us the pose of
    // each face we detected.
    std::vector<full_object_detection> shapes;
    rectangle faceBlob;
    seekrect_t refRect;
    float refMean =0.0f;
    float refMax = FLT_MIN;
    if (nfaces == 0) {
        BLOB_RECT blobs[MAX_BLOBS];
        // Detect reference black body in thermal
        int blobCount = BlobRectF(thermbuf, thermSize.width, thermSize.height, 35.0f, blobs, MAX_BLOBS);
        if (blobCount > 0) {
            int thermLeft = blobs[0].left;
            int thermTop = blobs[0].top;
            int thermRight = blobs[0].right;
            int thermBot = blobs[0].bottom;
            refRect.x = thermLeft;
            refRect.y = thermTop;
            refRect.width = thermRight - thermLeft + 1;
            refRect.height = thermBot - thermTop + 1;
            // Overwrite reference pixels with 25.0f (background)
            float sum = 0.0;
            for (int y = thermTop; y < thermBot; ++y) {
                for (int x = thermLeft; x < thermRight; ++x) {
                    int index = y * thermSize.width + x;
                    float temp = thermbuf[index];
                    if (temp > refMax) {
                        refMax = temp;
                    }
                    sum += temp;
                    thermbuf[index] = 25.0f;
                }
            }
            // Calculate average black body temperature
            refMean = sum / (refRect.width * refRect.height);
            fprintf(stdout, "Reference Black Body=%d\t%d,%d\t%d,%d max=%0.2f mean=%0.2f\n", blobCount, refRect.x, refRect.y, refRect.width, refRect.height, refMax, refMean);
        }
        // Try to locate face blob in thermal
        blobCount = BlobRectF(thermbuf, thermSize.width, thermSize.height, 30.0f, blobs, MAX_BLOBS);
        if (blobCount > 0) {
            int thermLeft = blobs[0].left;
            int thermTop = blobs[0].top;
            int thermRight = blobs[0].right;
            int thermBot = blobs[0].bottom;
            if (thermRight - thermLeft >= THERM_FACE_SIZE && thermBot - thermTop >= THERM_FACE_SIZE) {
                faceBlob.set_left(THERM_TO_VIS_X(thermLeft));
                faceBlob.set_top(THERM_TO_VIS_Y(thermTop));
                faceBlob.set_right(THERM_TO_VIS_X(thermRight));
                faceBlob.set_bottom(THERM_TO_VIS_Y(thermBot));
                dets.push_back(faceBlob);
                nfaces = 1;
                fprintf(stdout, "Visible Faces=%d\t%d,%d\t%d,%d\n", blobCount, faceBlob.left(), faceBlob.top(), faceBlob.width(), faceBlob.height());
            } else {
                fprintf(stdout, "FILTER Thermal Faces=%d\t%d,%d\t%d,%d\n", blobCount, thermLeft, thermTop, thermRight - thermLeft, thermBot - thermTop);
            }
        }
    } else if (nfaces > MAX_FACES) {
        // Limit nfaces to prevent array subscript
        nfaces = MAX_FACES;
    }

    for (unsigned long j = 0; j < nfaces; ++j)
    {
        CLOCK_GETTIME(&start_time);
        full_object_detection shape = sp(img, dets[j]);
        CLOCK_GETTIME(&end_time);
        uint32_t shapeTime = (end_time.tv_nsec + 1000000000 - start_time.tv_nsec) % 1000000000;

        // save face rect in imageData
        imageattr_t& image = imageData[j];
        image.faceTime = faceTime / 1000;
        image.shapeTime = shapeTime / 1000;
        seekrect_t& rect = image.faceRect;
        rect.x = dets[j].left();
        rect.y = dets[j].right();
        rect.width = dets[j].width();
        rect.height = dets[j].height();



        int n = shape.num_parts();
        cout << "number of parts: " << n << endl;

        if (n >= 5) {
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
        bool have_temp = process_frame(image);
    }
#ifdef SHOW_GUI
    // Now let's view our face poses on the screen.
    win.clear_overlay();
    win.set_image(img);
    // Show face rectangle in RED
    win.add_overlay(dets, rgb_pixel(255, 0, 0));
    // Show face features in GREEN
    win.add_overlay(render_face_detections(shapes));
#endif
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

            imageattr_t visData[MAX_FACES];
#ifdef SHOW_GUI
            int nfaces = find_face(win, detector, sp, filename, visData);
#else
            int nfaces = find_face(detector, sp, filename, visData);
#endif
            if (nfaces == 0) {
                ostream << "0\t" << filename << endl;

            } else {
                for (int f = 0; f < nfaces; ++f) {
                    imageattr_t& image = visData[f];
                    seekrect_t& rect = image.faceRect;
                    ostream << f << "\t" << filename << "\t" << image.faceTime << "\t" <<
                        rect.x << "," << rect.y << "\t" << rect.width << "," << rect.height << "\t" << image.shapeTime << "\t" <<
                        image.leftOuter.x << "," << image.leftOuter.y << "\t" << image.leftInner.x << "," << image.leftInner.y << "\t" <<
                        image.rightInner.x << "," << image.rightInner.y << "\t" << image.rightOuter.x << "," << image.rightOuter.y << "\t" <<
                        image.nose.x << "," << image.nose.y;
                    if (!isnan(image.leftTemp) && !isnan(image.rightTemp)) {
                        ostream << "\t" << image.leftThermal.x << "," << image.leftThermal.y << "\t" << image.rightThermal.x << "," << image.rightThermal.y;
                        ostream << setprecision(2) << fixed;
                        ostream << "\t" << image.leftTemp << "," << image.rightTemp;
                    }
                    ostream << endl;
                }
            }
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

