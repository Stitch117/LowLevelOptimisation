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
#include "TrackerManager.h"

// includes for the thread pool
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

//canary guard value
static volatile uint64_t __myStackCheckGuard = 0xF0F0F0F0F0F0F0F0; 

using namespace std::chrono;

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

    //vector of collider objects within the region
    std::vector<ColliderObject*> regionColliders;
};

bool acceptedRegionCount = false;
int regionCount = 0;

//vector of all the colliders
std::vector<ColliderObject*> colliders;

int clickedBoxIndex = -1;

std::vector<region> regions;

//thread pool variables
std::vector<std::thread> workers; //vector of threads (the thread pool)
std::queue<std::function<void()>> tasks; //the tasks that the threads will be taking from
std::mutex queueMutex; //used to lock tasks so only one thread can access it
std::condition_variable cv; 
bool stopThreads = false;

float FPS = 0;
int numOfBoxes = 0;
int numOfSpheres = 0;

//abort function for canary
inline void myStackCheckFail() {
    std::cout << "*** stack canary check failed ***\n";
    std::abort();
}

//Canary check in each function
struct CanaryGuard {
    uint64_t localCanary;

    //initialise a local canary on fucntion start
    CanaryGuard() 
    {
        localCanary = __myStackCheckGuard;
    }

    //when local canaryGuard drops out of scope it will call this check
    ~CanaryGuard() 
    {
        if (localCanary != __myStackCheckGuard) 
        {
            myStackCheckFail(); 
        }
    }
};

//canary check
void CanaryDemo() {
    CanaryGuard cg;

    //corrupt the CanaryGuard's local value (for testing only)
    *((uint64_t*)&cg) ^= 0x1111;
}


//used in printing memory data
void printNumOfObjs()
{
    std::cout << "Num of Boxes: " << numOfBoxes << " Num of Spheres: " << numOfSpheres << std::endl;
}


//create the amouint of regions requested, equally divided along the x axis
void generateRegions(int _regionCount)
{
    CanaryGuard cg; // automatic, checked when function exits

    float width = (maxX - minX) / static_cast<float>(_regionCount); //width of each region

    //define a region bounding box with the minimum to the maximum
    for (int i = 0; i < _regionCount; i++)
    {
        region r;
        r.minRegionX = Vec3(i * width + minX, FLOORY, minZ);
        r.maxRegionx = Vec3((i + 1) * width + minX, CIELINGY, maxZ);
        regions[i] = r;
    }
}

void generateMapTracker()
{
    TrackerManager::GetTracker("Global");
    TrackerManager::GetTracker("GlobalWithHeaderAndFooter");
}

//organise each object into its respetive region
void organiseVectors(std::vector<ColliderObject*> _colliders)
{
    CanaryGuard cg; // automatic, checked when function exits

    for (int i = 0; i < _colliders.size(); i++)
    {
        ColliderObject* obj = _colliders[i];
        const float x = obj->position.x;

        //edge cases on the outside walls
        if (x < regions[0].minRegionX.x)
        {
            regions[0].regionColliders.push_back(obj);
        }
        else if (x >= regions[regions.size() - 1].maxRegionx.x)
        {
            regions[regions.size() - 1].regionColliders.push_back(obj);
        }

        //check if the x co-ordinate of the object is in a region and if so, add to it
        for (int r = 0; r < regions.size(); r++)
        {
            //internal collision checks
            if (x >= regions[r].minRegionX.x && x < regions[r].maxRegionx.x)
            {
                regions[r].regionColliders.push_back(obj);

                //region checks for edges of boxes to make more accurate simulation on boundries of regions
                if (x - HALF_OBJECT_LENGTH < regions[r].minRegionX.x && x > minX + HALF_OBJECT_LENGTH)
                {
                    regions[r - 1].regionColliders.push_back(obj);
                }
                if (x + HALF_OBJECT_LENGTH > regions[r].maxRegionx.x && x < maxX - HALF_OBJECT_LENGTH)
                {
                    regions[r + 1].regionColliders.push_back(obj);
                }

                break;
            }
        }
    }    
}



void initScene(int boxCount, int sphereCount) 
{
    std::cout << "\nBefore any allocation:\n";
    TrackerManager::PrintAll();
    printNumOfObjs();

    for (int i = 0; i < boxCount; ++i) 
    {
        //use overide new in crating box
        Box* box = new Box();

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
        numOfBoxes++;
    }

    //memory tracker couts
    //std::cout << "\nafter box allocation:\n";
    //TrackerManager::PrintAll(); 
    //printNumOfObjs();

    for (int i = 0; i < sphereCount; ++i) 
    {
        //use overide new in crating sphere
        Sphere* sphere = new Sphere(); //use placement new instead

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
        numOfSpheres++;
    }

    //memory tracker couts
    //std::cout << "\nafter sphere allocation:\n";
    //TrackerManager::PrintAll();
    //printNumOfObjs();
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
Vec3 screenToWorld(int x, int y) 
{
    CanaryGuard cg; // automatic, checked when function exits

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
void updatePhysics(const float deltaTime, std::vector<ColliderObject*> _colliders) 
{
    for (int i = 0; i < _colliders.size(); i++)
    {
        _colliders[i]->update(&_colliders, deltaTime);
        
    }
}

// draw the sides of the containing area
void drawQuad(const Vec3& v1, const Vec3& v2, const Vec3& v3, const Vec3& v4) {
    CanaryGuard cg; // automatic, checked when function exits

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

    //FPS check
    static auto last = steady_clock::now();
    auto old = last;

    //delta time creation
    last = steady_clock::now();
    const duration<float> frameTime = last - old;
    float deltaTime = frameTime.count();
    auto start = std::chrono::steady_clock::now();

    //empty the regions of last frame
    for (int i = 0; i < regionCount; i++)
    {
        regions[i].regionColliders.clear();
    }

    //assign each object ito the corresponsding physics region
    organiseVectors(colliders);

    //assign the task queue the physics updates
    { //{} are used to tell when to unlock the queueMutex
        std::unique_lock<std::mutex> lock(queueMutex);
        for (int i = 0; i < regions.size(); i++)
        {
            for (int j = 0; j < regions[i].regionColliders.size(); j++)
            {
                regions[i].regionColliders[j]->colour = Vec3(i / 10.0f, i / 10.0f, i / 10.0f);
            }
            tasks.push([=]() { updatePhysics(deltaTime, regions[i].regionColliders); });
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
        std::this_thread::yield(); // stops the CPU for a bit to let the threads keep working before locking the task queue again
    }

    //diagnostic data
    auto end = std::chrono::steady_clock::now();
    double difference = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
    double frameTimeFPS = 1000 * deltaTime;

    FPS = 1000 / frameTimeFPS;
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
            delete(colliders[clickedBoxIndex]);
            colliders.erase(colliders.begin() + clickedBoxIndex);
            if (colliders[clickedBoxIndex]->ColliderTypeInt == ColliderObject::ColliderType::BoxCollider)
            {
                numOfBoxes--;
            }
            else if (colliders[clickedBoxIndex]->ColliderTypeInt == ColliderObject::ColliderType::SphereCollider)
            {
                numOfSpheres--;
            }
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
    //print out current emmeory information
    else if (key == '0') 
    // 0
    { 
        std::cout << "Current memory information: \n";
        TrackerManager::PrintAll();
        printNumOfObjs();
    }
    //current FPS count print
    else if (key == '9') { // 9

        std::cout << "Current FPS of physics: " << FPS << std::endl;
    }
    else if (key == '8') { // 8

        //check canaries
        CanaryDemo(); 
    }

    //add 50 of each object if there is room in memory pool
    else if (key == '1') 
    // 1
    {
        if (numOfBoxes <= MAX_NUMBER_BOXES - NUMBER_OF_BOXES && numOfSpheres <= MAX_NUMBER_SPHERES - NUMBER_OF_SPHERES)
        {
            initScene(NUMBER_OF_BOXES, NUMBER_OF_SPHERES);
        }
    }

    //remove 50 of each object if there is that many 
    else if (key == '2')// 2
    { 
        if (colliders.size() > 0)
        {
            int tempColSize = colliders.size();

            //check if enough objs to delete
            if (tempColSize < NUMBER_OF_BOXES + NUMBER_OF_SPHERES)
            {
                //remove the last batch 
                for (int i = tempColSize - 1; i > -1; i--)
                {
                    ColliderObject* obj = colliders.back();
                    colliders.pop_back();
                    delete(obj);
                }
                numOfBoxes = 0;
                numOfSpheres = 0;
                std::cout << "\nAfter removing all remainin objects:\n";
            }
            else
            {
                //remove the last batch 
                for (int i = tempColSize - 1; i > tempColSize - NUMBER_OF_BOXES - NUMBER_OF_SPHERES - 1; i--)
                {
                    ColliderObject* obj = colliders.back();
                    colliders.pop_back();
                    delete(obj);
                }
                numOfBoxes -= NUMBER_OF_BOXES;
                numOfSpheres -= NUMBER_OF_SPHERES;
                std::cout << "\nAfter deleting " << NUMBER_OF_BOXES << " boxes and " << NUMBER_OF_SPHERES << " spheres:\n";
            }
            TrackerManager::PrintAll(); 
            printNumOfObjs();
        }
    }
}

// the main function. 
int main(int argc, char** argv) {
    CanaryGuard cg; // automatic, checked when function exits

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
    generateMapTracker();
    BoxPool::Get(MAX_NUMBER_BOXES); 
    SpherePool::Get(MAX_NUMBER_SPHERES);

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
    regions.resize(regionCount);
    for (int i = 0; i < regionCount; i++)
    {
        regions[i].regionColliders.resize(regionCount);
    }
    generateRegions(regionCount);

    //create the thread pool
    for (int i = 0; i < regionCount; i++) //One thread per region
    {
        //lamda function so it can run any task
        workers.emplace_back([]() {
            while (true)
            {
                std::function<void()> task; //holds the task of the thread
                {
                    //locks the task when started so two threads can't do same task
                    std::unique_lock<std::mutex> lock(queueMutex); //unique lock will auto unlock after task is done

                    //thread will sleep until there is either a task or the program closes
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

    //on exit, clean up the unorered map
    atexit([]() 
        { 
        TrackerManager::Cleanup(); 
        }); 

    // it will stick here until the program ends. 
    glutMainLoop();
    return 0;
}