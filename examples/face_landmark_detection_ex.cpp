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

#include "seektypes.h"

#define SHOW_GUI            // ~360KB
#define USE_LOAD_IMAGE      // ~160KB

#ifdef WIN32
#define CLOCK_GETTIME(t)    timespec_get(t, TIME_UTC)
#else
#define CLOCK_GETTIME(t)    clock_gettime(CLOCK_REALTIME, t)
#endif

using namespace dlib;
using namespace std;

// ----------------------------------------------------------------------------------------

typedef struct imageattr_t
{
    uint32_t duration;
    seekrect_t faceRect;
    seekpoint_t leftOuter;
    seekpoint_t leftInner;
    seekpoint_t rightInner;
    seekpoint_t rightOuter;
    seekpoint_t nose;
} IMAGEATTR_T;

#define MAX_FACES       3

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
#ifdef PYRAMID_UP
    // Make the image larger so we can detect small faces.
    pyramid_up(img);
#endif

    // Now tell the face detector to give us a list of bounding boxes
    // around all the faces in the image.
    std::vector<rectangle> dets = detector(img);
    size_t nfaces = dets.size();
    cout << "Number of faces detected: " << nfaces << endl;

    // Now we will go ask the shape_predictor to tell us the pose of
    // each face we detected.
    std::vector<full_object_detection> shapes;

    // Limit nfaces to prevent array subscript
    if (nfaces > MAX_FACES) nfaces = MAX_FACES;

    for (unsigned long j = 0; j < nfaces; ++j)
    {
        struct timespec start_time, end_time;
        CLOCK_GETTIME(&start_time);
        full_object_detection shape = sp(img, dets[j]);
        CLOCK_GETTIME(&end_time);
        uint32_t duration = (end_time.tv_nsec + 1000000000 - start_time.tv_nsec) % 1000000000;

        // output face rect to myfile
        imageattr_t& image = imageData[j];
        image.duration = duration / 1000;
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
                    ostream << f << "\t" << filename << "\t" << image.duration << "\t" <<
                        rect.x << "," << rect.y << "\t" << rect.width << "," << rect.height << "\t" <<
                        image.leftOuter.x << "," << image.leftOuter.y << "\t" << image.leftInner.x << "," << image.leftInner.y << "\t" <<
                        image.rightInner.x << "," << image.rightInner.y << "\t" << image.rightOuter.x << "," << image.rightOuter.y << endl;
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

