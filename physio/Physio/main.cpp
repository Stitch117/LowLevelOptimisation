#include <stdlib.h>
#include <GL/glut.h>
#include <iostream>

#include <cstdlib>
#include <ctime>
#include <chrono>
#include "globals.h"
#include "Vec3.h"
#include "ColliderObject.h"
#include "Box.h"
#include "Sphere.h"
#include "optick.h"

// includes for the thread pool
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>


using namespace std::chrono;

// this is the number of falling physical items. 
#define NUMBER_OF_BOXES 50
#define NUMBER_OF_SPHERES 50

// these is where the camera is, where it is looking and the bounds of the continaing box. You shouldn't need to alter these
#define LOOKAT_X 10
#define LOOKAT_Y 10
#define LOOKAT_Z 50

#define LOOKDIR_X 10
#define LOOKDIR_Y 0
#define LOOKDIR_Z 0


//Oct tree regions
struct region
{
    Vec3 minRegionX;
    Vec3 maxRegionx;
};

bool acceptedRegionCount = false;
int regionCount = 0;

//vector of all the colliders
std::vector<ColliderObject*> colliders;

//vector of vectors containing the regions. allows dynamically setting amount of regions wanted 
std::vector<std::vector<ColliderObject*>> regionColliders;
int clickedBoxIndex = -1;

std::vector<region> regions;

//thread pool variables
std::vector<std::thread> workers; //vector of threads (the thread pool)
std::queue<std::function<void()>> tasks; //the tasks that the threads will be taking from
std::mutex queueMutex; //used to lock tasks so only one thread can access it
std::condition_variable cv; 
bool stopThreads = false;


//create the amouint of regions requested, equally divided along the x axis
void generateRegions(int _regionCount)
{
    float width = (maxX - minX) / static_cast<float>(regionCount); //width of each region

    //define a region bounding box with the minimum to the maximum
    for (int i = 0; i < regionCount; i++)
    {
        region r;
        r.minRegionX = Vec3(i * width + minX, FLOORY, minZ);
        r.maxRegionx = Vec3((i + 1) * width + minX, CIELINGY, maxZ);
        regions.push_back(r);
    }
}

//organise each object into its respetive region
void organiseVectors(std::vector<ColliderObject*> _colliders)
{
    for (int i = 0; i < _colliders.size(); i++)
    {
        ColliderObject* obj = _colliders[i];
        const float x = obj->position.x;

        //check if the x co-ordinate of the object is in a region and if so, add to it
        for (int r = 0; r < regions.size(); r++)
        {
            if (x < regions[0].minRegionX.x)
            {
                regionColliders[0].push_back(obj);
            }
            else if (x > regions[regions.size() - 1].maxRegionx.x)
            {
                regionColliders[regions.size() - 1].push_back(obj); 
            }
            else if (x >= regions[r].minRegionX.x && x < regions[r].maxRegionx.x)
            {
                regionColliders[r].push_back(obj);
                break;

                //region checks for edges of boxes to make more accurate simulation on boundries of regions
                if (x - 0.5f < regions[r].minRegionX.x)
                {
                    regionColliders[r - 1].push_back(obj);
                }
                if (x + 0.5f > regions[r].maxRegionx.x)
                {
                    regionColliders[r + 1].push_back(obj);
                }
            }
        }
    }    
}



void initScene(int boxCount, int sphereCount) {
    for (int i = 0; i < boxCount; ++i) 
    {
        //use malloc to accurately allocate a piece of memeory
        Box* boxMemory = (Box*)malloc(sizeof(Box));
        Box* box = new (boxMemory) Box(); //use placement new instead

        // Assign random x, y, and z positions within specified ranges
        box->position.x = static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 20.0f));
        box->position.y = 10.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 1.0f));
        box->position.z = static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 20.0f));

        box->size = {1.0f, 1.0f, 1.0f};

        // Assign random x-velocity between -1.0f and 1.0f
        float randomXVelocity = -1.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 2.0f));
        box->velocity = {randomXVelocity, 0.0f, 0.0f};

        // Assign a random color to the box
        box->colour.x = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        box->colour.y = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        box->colour.z = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

        colliders.push_back(box);
    }

    for (int i = 0; i < sphereCount; ++i) 
    {
        //use malloc to accurately allocate a piece of memeory
        Sphere* sphereMemory = (Sphere*)malloc(sizeof(Sphere));
        Sphere* sphere = new (sphereMemory) Sphere(); //use placement new instead

        // Assign random x, y, and z positions within specified ranges
        sphere->position.x = static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 20.0f));
        sphere->position.y = 10.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 1.0f));
        sphere->position.z = static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 20.0f));

        sphere->size = { 1.0f, 1.0f, 1.0f };

        // Assign random x-velocity between -1.0f and 1.0f
        float randomXVelocity = -1.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 2.0f));
        sphere->velocity = { randomXVelocity, 0.0f, 0.0f };

        // Assign a random color to the box
        sphere->colour.x = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        sphere->colour.y = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        sphere->colour.z = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

        colliders.push_back(sphere);
    }
}

// a ray which is used to tap (by default, remove) a box - see the 'mouse' function for how this is used.
bool rayBoxIntersection(const Vec3& rayOrigin, const Vec3& rayDirection, const ColliderObject* box) {
    float tMin = (box->position.x - box->size.x / 2.0f - rayOrigin.x) / rayDirection.x;
    float tMax = (box->position.x + box->size.x / 2.0f - rayOrigin.x) / rayDirection.x;

    if (tMin > tMax) std::swap(tMin, tMax);

    float tyMin = (box->position.y - box->size.y / 2.0f - rayOrigin.y) / rayDirection.y;
    float tyMax = (box->position.y + box->size.y / 2.0f - rayOrigin.y) / rayDirection.y;

    if (tyMin > tyMax) std::swap(tyMin, tyMax);

    if ((tMin > tyMax) || (tyMin > tMax))
        return false;

    if (tyMin > tMin)
        tMin = tyMin;

    if (tyMax < tMax)
        tMax = tyMax;

    float tzMin = (box->position.z - box->size.z / 2.0f - rayOrigin.z) / rayDirection.z;
    float tzMax = (box->position.z + box->size.z / 2.0f - rayOrigin.z) / rayDirection.z;

    if (tzMin > tzMax) std::swap(tzMin, tzMax);

    if ((tMin > tzMax) || (tzMin > tMax))
        return false;

    return true;
}

// used in the 'mouse' tap function to convert a screen point to a point in the world
Vec3 screenToWorld(int x, int y) {
    GLint viewport[4];
    GLdouble modelview[16];
    GLdouble projection[16];
    GLfloat winX, winY, winZ;
    GLdouble posX, posY, posZ;

    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    winX = (float)x;
    winY = (float)viewport[3] - (float)y;
    glReadPixels(x, int(winY), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &winZ);

    gluUnProject(winX, winY, winZ, modelview, projection, viewport, &posX, &posY, &posZ);

    return Vec3((float)posX, (float)posY, (float)posZ);
}




// update the physics: gravity, collision test, collision resolution
void updatePhysics(const float deltaTime, std::vector<ColliderObject*> _colliders) {
    OPTICK_THREAD();


    // todo for the assessment - use a thread for each sub region
    // for example, assuming we have two regions:
    // from 'colliders' create two separate lists
    // empty each list (from previous frame) and work out which collidable object is in which region, 
    //  and add the pointer to that region's list.
    // Then, run two threads with the code below (changing 'colliders' to be the region's list)

    for (int i = 0; i < _colliders.size(); i++)
    {
        _colliders[i]->update(&_colliders, deltaTime);
        
    }
}

// draw the sides of the containing area
void drawQuad(const Vec3& v1, const Vec3& v2, const Vec3& v3, const Vec3& v4) {
    
    glBegin(GL_QUADS);
    glVertex3f(v1.x, v1.y, v1.z);
    glVertex3f(v2.x, v2.y, v2.z);
    glVertex3f(v3.x, v3.y, v3.z);
    glVertex3f(v4.x, v4.y, v4.z);
    glEnd();
}



// draw the entire scene
void drawScene() {
    OPTICK_FRAME("drawScene");

    // Draw the side wall
    GLfloat diffuseMaterial[] = {0.5f, 0.5f, 0.5f, 1.0f};
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuseMaterial);

    // Draw the left side wall
    glColor3f(0.5f, 0.5f, 0.5f); // Set the wall color
    Vec3 leftSideWallV1(minX, 0.0f, maxZ);
    Vec3 leftSideWallV2(minX, 50.0f, maxZ);
    Vec3 leftSideWallV3(minX, 50.0f, minZ);
    Vec3 leftSideWallV4(minX, 0.0f, minZ);
    drawQuad(leftSideWallV1, leftSideWallV2, leftSideWallV3, leftSideWallV4);

    // Draw the right side wall
    glColor3f(0.5f, 0.5f, 0.5f); // Set the wall color
    Vec3 rightSideWallV1(maxX, 0.0f, maxZ);
    Vec3 rightSideWallV2(maxX, 50.0f, maxZ);
    Vec3 rightSideWallV3(maxX, 50.0f, minZ);
    Vec3 rightSideWallV4(maxX, 0.0f, minZ);
    drawQuad(rightSideWallV1, rightSideWallV2, rightSideWallV3, rightSideWallV4);


    // Draw the back wall
    glColor3f(0.5f, 0.5f, 0.5f); // Set the wall color
    diffuseMaterial[0] = 0.2f; diffuseMaterial[1] = 0.2f; diffuseMaterial[2] = 0.2f; //set material to darker on the back wall
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuseMaterial);

    Vec3 backWallV1(minX, 0.0f, minZ);
    Vec3 backWallV2(minX, 50.0f, minZ);
    Vec3 backWallV3(maxX, 50.0f, minZ);
    Vec3 backWallV4(maxX, 0.0f, minZ);
    drawQuad(backWallV1, backWallV2, backWallV3, backWallV4);

    for (ColliderObject* box : colliders) {
        box->draw();
    }
}

// called by GLUT - displays the scene
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    gluLookAt(LOOKAT_X, LOOKAT_Y, LOOKAT_Z, LOOKDIR_X, LOOKDIR_Y, LOOKDIR_Z, 0, 1, 0);

    drawScene();

    glutSwapBuffers();
}

// called by GLUT when the cpu is idle - has a timer function you can use for FPS, and updates the physics
// see https://www.opengl.org/resources/libraries/glut/spec3/node63.html#:~:text=glutIdleFunc
// NOTE this may be capped at 60 fps as we are using glutPostRedisplay(). If you want it to go higher than this, maybe a thread will help here. 
void idle() {
    OPTICK_FRAME("update");

    static auto last = steady_clock::now();
    auto old = last;
    last = steady_clock::now();
    const duration<float> frameTime = last - old;
    float deltaTime = frameTime.count();
    auto start = std::chrono::steady_clock::now();

    //empty the regions of last frame
    for (int i = 0; i < regionCount; i++)
    {
        regionColliders[i].clear();
    }

    //assign each object ito the corresponsding physics region
    organiseVectors(colliders);

    //assign the task queue the physics updates
    { //{} are used to tell when to unlock the queueMutex
        std::unique_lock<std::mutex> lock(queueMutex);
        for (int i = 0; i < regions.size(); i++)
        {
            tasks.push([=]() { updatePhysics(deltaTime, regionColliders[i]); });
        }
    }
    
    cv.notify_all(); //tell the thread pool there is a task

    //check and wait until all tasks are done
    bool done = false;
    while (!done)
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        done = tasks.empty(); // wait until all tasks are finished
        lock.unlock();
        std::this_thread::yield(); // stops the CPU fo ra bit to let the threads keep working before locking the task queue again
    }

    //diagnostic data
    auto end = std::chrono::steady_clock::now();
    double difference = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
    std::cout << difference << "\n";

    float FPS = 1.0f / difference;
    std::cout << FPS << "\n";

    // tell glut to draw - note this will cap this function at 60 fps
    glutPostRedisplay();
}

// called the mouse button is tapped
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // Get the camera position and direction
        Vec3 cameraPosition(LOOKAT_X, LOOKAT_Y, LOOKAT_Z); // Replace with your actual camera position
        Vec3 cameraDirection(LOOKDIR_X, LOOKDIR_Y, LOOKDIR_Z); // Replace with your actual camera direction

        // Get the world coordinates of the clicked point
        Vec3 clickedWorldPos = screenToWorld(x, y);

        // Calculate the ray direction from the camera position to the clicked point
        Vec3 rayDirection = clickedWorldPos - cameraPosition;
        rayDirection.normalise();

        // Perform a ray-box intersection test and remove the clicked box
        bool clickedBoxOK = false;
        float minIntersectionDistance = std::numeric_limits<float>::max();

        for (int i = 0; i < colliders.size(); i++)
        {
            if (rayBoxIntersection(cameraPosition, rayDirection, colliders[i])) {
                // Calculate the distance between the camera and the intersected box
                Vec3 diff = colliders[i]->position - cameraPosition;
                float distance = diff.length();

                // Update the clicked box index if this box is closer to the camera
                if (distance < minIntersectionDistance) {
                    clickedBoxOK = true;
                    clickedBoxIndex = i;
                    minIntersectionDistance = distance;
                }
            }
        }

        // Remove the clicked box if any
        if (clickedBoxOK != false) 
        {
            free(colliders[clickedBoxIndex]);
            colliders[clickedBoxIndex] = nullptr;
            colliders.erase(colliders.begin() + clickedBoxIndex);
        }
    }
}

// called when the keyboard is used
void keyboard(unsigned char key, int x, int y) {
    const float impulseMagnitude = 20.0f; // Upward impulse magnitude

    if (key == ' ') { // Spacebar key
        for (ColliderObject* box : colliders) {
            box->velocity.y += impulseMagnitude;
        }
    }
    else if (key == '0') { // 1

        std::cout << "Memory used" << std::endl;
    }
    else if (key == '1') { // 1

        initScene(NUMBER_OF_BOXES, NUMBER_OF_SPHERES);
    }
}

// the main function. 
int main(int argc, char** argv) {

    srand(static_cast<unsigned>(time(0))); // Seed random number generator
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1920, 1080);
    glutCreateWindow("Simple Physics Simulation");

    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 800.0 / 600.0, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);

    //ask for anmount of regions
    while (acceptedRegionCount != true)
    {
        std::cout << "Enter number of regions to a power of 2 (2, 4, 8, 16...) \n";
        std::cin >> regionCount;
        if (regionCount < 1)
        {
            std::cout << "Can't be smaller than one so defaulting to 1\n";
            regionCount = 1;
            acceptedRegionCount;
            break;
        }

        //if a number is a power of 2 then log2(n) should return a whole number, so cieling and floor should return the same number
        float tempLogAmount = log2(regionCount);
        if (ceil(tempLogAmount) == floor(tempLogAmount))
        {
            std::cout << "accepted amount of: " << regionCount << "\n";
            acceptedRegionCount = true;
        }
        else
        {
            std::cout << "That is not a power of two. Please try again\n";
        }
    }

    //create the regions
    regionColliders.resize(regionCount);
    generateRegions(regionCount);

    //create the thread pool
    for (int i = 0; i < regionCount; ++i) //One thread per region
    {
        //lamda function so it can run any task
        workers.emplace_back([]() {
            while (true)
            {
                std::function<void()> task; //holds the task of the thread
                {
                    //locks the task when started so two threads can't do same task
                    std::unique_lock<std::mutex> lock(queueMutex); //unique lock will auto unlock after task is done

                    //thread will sleep until there is eithe ra task or the program closes
                    cv.wait(lock, [] { return stopThreads || !tasks.empty(); });

                    //thread can exit 
                    if (stopThreads && tasks.empty())
                        return;

                    task = std::move(tasks.front()); //gets the first job in the task vector
                    tasks.pop(); 
                }
                task();
            }
        });
    }


    initScene(NUMBER_OF_BOXES, NUMBER_OF_SPHERES);
    glutDisplayFunc(display);
    glutIdleFunc(idle);

    // it will stick here until the program ends. 
    glutMainLoop();
    return 0;
}
