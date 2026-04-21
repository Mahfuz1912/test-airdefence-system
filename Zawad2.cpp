// Zawad2.cpp - Second Push (Physics Engine Enhancement)
// Branch: feature/zawad-physics-engine-v3
// Contributor: Zawad
// Push #2 - Advanced Physics Calculations

#include <GL/glut.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#define PI              3.14159265358979323846f
#define WIN_W           1200
#define WIN_H           700
#define MAX_DRONES      6
#define MAX_MISSILES    10
#define MAX_EXPLOSIONS  10
#define MAX_PARTICLES   80

// [ZAWAD MODIFICATION #2]
// Advanced Physics Engine with Trajectory Calculation
struct PhysicsState {
    float gravity;
    float drag;
    float windForce;
    float accuracy;
};

struct Drone {
    float x, y;
    float speed;
    bool  active;
    bool  detected;
    int   missileAssigned;
};

struct Missile {
    float x, y;
    float speed;
    float vx, vy;  // [ZAWAD#2] Velocity components
    bool  active;
    int   targetIdx;
};

struct Explosion {
    float x, y;
    float radius;
    float maxRadius;
    int   frame;
    bool  active;
};

struct Particle {
    float x, y;
    float vx, vy;
    float life;
    float r, g, b;
    int   type;
    bool  active;
};

float gRadarAngle   = 0.0f;
const float RADAR_X = 100.0f;
const float RADAR_Y = 580.0f;
const float RADAR_R = 88.0f;
const float RADAR_DETECT_RANGE = 420.0f;

Drone      gDrones    [MAX_DRONES];
Missile    gMissiles  [MAX_MISSILES];
Explosion  gExplosions[MAX_EXPLOSIONS];
Particle   gParticles [MAX_PARTICLES];

int  gScore          = 0;
int  gSpawnTimer     = 0;
int  gSpawnInterval  = 200;
bool gPaused         = false;
int  gFrameCounter   = 0;

// [ZAWAD#2] Physics system
PhysicsState gPhysics = {0.15f, 0.02f, 0.05f, 0.92f};

float gStarX[200], gStarY[200];
float gStreetLights[20][2];

static inline float fDist(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1, dy = y2 - y1;
    return sqrtf(dx*dx + dy*dy);
}

static void col3(float r, float g, float b) { glColor3f(r, g, b); }
static void col4(float r, float g, float b, float a) { glColor4f(r, g, b, a); }

static void drawFilledCircle(float cx, float cy, float r, int seg = 60) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for(int i = 0; i <= seg; i++){
        float a = 2.0f * PI * i / seg;
        glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
    }
    glEnd();
}

static void drawCircleOutline(float cx, float cy, float r, int seg = 60) {
    glBegin(GL_LINE_LOOP);
    for(int i = 0; i < seg; i++){
        float a = 2.0f * PI * i / seg;
        glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
    }
    glEnd();
}

static void drawLine(float x1, float y1, float x2, float y2) {
    glBegin(GL_LINES);
    glVertex2f(x1, y1); glVertex2f(x2, y2);
    glEnd();
}

static void drawRect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x,   y);   glVertex2f(x+w, y);
    glVertex2f(x+w, y+h); glVertex2f(x,   y+h);
    glEnd();
}

static void drawText(float x, float y, const char* s, void* font = GLUT_BITMAP_HELVETICA_18) {
    glRasterPos2f(x, y);
    for(; *s; s++) glutBitmapCharacter(font, *s);
}

// [ZAWAD MODIFICATION #2] - Calculate trajectory with physics
static void calculateTrajectory(float& mx, float& my, float& mvx, float& mvy, 
                                float tx, float ty) {
    float dx = tx - mx;
    float dy = ty - my;
    float dist = sqrtf(dx*dx + dy*dy);
    
    if(dist < 0.1f) return;
    
    // [ZAWAD#2] Apply physics to velocity
    mvx = (dx / dist) * gPhysics.accuracy;
    mvy = (dy / dist) * gPhysics.accuracy;
    
    // [ZAWAD#2] Apply gravity and drag
    mvy -= gPhysics.gravity;
    mvx *= (1.0f - gPhysics.drag);
    mvy *= (1.0f - gPhysics.drag);
    
    // [ZAWAD#2] Apply wind force
    mvx += gPhysics.windForce * sinf(gFrameCounter * 0.05f);
}

// [ZAWAD MODIFICATION #2] - Draw trajectory prediction line
static void drawTrajectoryLine(float x, float y, float vx, float vy) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    col4(0.0f, 1.0f, 1.0f, 0.3f);
    glLineWidth(1.0f);
    glBegin(GL_LINE_STRIP);
    
    for(int i = 0; i < 20; i++){
        float px = x + vx * i;
        float py = y + vy * i - gPhysics.gravity * i * i;
        glVertex2f(px, py);
    }
    glEnd();
    glLineWidth(1.0f);
    
    glDisable(GL_BLEND);
}

static void drawRadar() {
    float cx = RADAR_X, cy = RADAR_Y, r = RADAR_R;
    float sweepRad = gRadarAngle * PI / 180.0f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float bgPulse = 0.95f + 0.05f * sinf(gFrameCounter * 0.05f);
    col4(0.00f, 0.06f, 0.00f, bgPulse);
    drawFilledCircle(cx, cy, r);

    glLineWidth(1.0f);
    for(int i = 1; i <= 4; i++){
        col4(0.00f, 0.45f, 0.12f, 0.6f);
        drawCircleOutline(cx, cy, r * (float)i / 4.0f);
    }

    col4(0.00f, 0.42f, 0.12f, 0.6f);
    drawLine(cx - r, cy, cx + r, cy);
    drawLine(cx, cy - r, cx, cy + r);

    int TRAIL = 50;
    for(int i = 0; i < TRAIL; i++){
        float a = sweepRad - i * (PI / 60.0f);
        float alpha = (float)(TRAIL - i) / (float)TRAIL * 0.65f;
        col4(0.00f, 0.90f, 0.25f, alpha);
        glBegin(GL_TRIANGLES);
        glVertex2f(cx, cy);
        glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
        glVertex2f(cx + r * cosf(a - PI / 60.0f), cy + r * sinf(a - PI / 60.0f));
        glEnd();
    }

    col4(0.00f, 0.80f, 0.20f, 0.3f);
    glLineWidth(4.0f);
    drawLine(cx, cy, cx + r * cosf(sweepRad), cy + r * sinf(sweepRad));
    col4(0.20f, 1.00f, 0.40f, 1.0f);
    glLineWidth(2.0f);
    drawLine(cx, cy, cx + r * cosf(sweepRad), cy + r * sinf(sweepRad));
    glLineWidth(1.0f);

    for(int i = 0; i < MAX_DRONES; i++){
        if(gDrones[i].active && gDrones[i].detected){
            float wx = (gDrones[i].x - cx) / (RADAR_DETECT_RANGE) * r;
            float wy = (gDrones[i].y - cy) / (RADAR_DETECT_RANGE) * r;
            float bd = sqrtf(wx*wx + wy*wy);
            if(bd > r - 5){ wx = wx/bd*(r-5); wy = wy/bd*(r-5); }
            
            float blipPulse = 0.5f + 0.5f * sinf(gFrameCounter * 0.15f);
            if(blipPulse > 0.3f){
                col4(1.0f, 0.25f, 0.05f, 1.0f);
                drawFilledCircle(cx + wx, cy + wy, 5);
                col4(1.0f, 0.60f, 0.30f, 0.6f);
                drawFilledCircle(cx + wx, cy + wy, 8);
            }
        }
    }

    col4(0.10f, 0.70f, 0.25f, 1.0f);
    glLineWidth(3.0f);
    drawCircleOutline(cx, cy, r);
    glLineWidth(1.0f);

    col4(0.60f, 1.00f, 0.70f, 0.5f);
    drawFilledCircle(cx, cy, 8);
    col4(0.30f, 1.00f, 0.50f, 1.0f);
    drawFilledCircle(cx, cy, 5);

    glDisable(GL_BLEND);
}

static void initAll() {
    srand((unsigned)time(nullptr));

    for(int i = 0; i < 200; i++){
        gStarX[i] = (float)(rand() % WIN_W);
        gStarY[i] = 230 + (float)(rand() % (WIN_H - 230));
    }

    for(auto& d : gDrones) { d.active = false; d.detected = false; d.missileAssigned = -1; }
    for(auto& m : gMissiles) { 
        m.active = false;
        // [ZAWAD#2] Initialize velocity components
        m.vx = 0.0f;
        m.vy = 0.0f;
    }
    for(auto& e : gExplosions) { e.active = false; }
    for(auto& p : gParticles) { p.active = false; }
}

static void spawnDrone() {
    for(auto& d : gDrones){
        if(!d.active){
            d.x = (float)(WIN_W + 60);
            d.y = 420.0f + (rand() % 180);
            d.speed = 1.4f + (rand() % 25) / 10.0f;
            d.active = true;
            d.detected = false;
            d.missileAssigned = -1;
            return;
        }
    }
}

static void launchMissile(int droneIdx) {
    for(int i = 0; i < MAX_MISSILES; i++){
        if(!gMissiles[i].active){
            gMissiles[i].x = 185.0f;
            gMissiles[i].y = 318.0f;
            gMissiles[i].speed = 5.5f;
            gMissiles[i].active = true;
            gMissiles[i].targetIdx = droneIdx;
            gDrones[droneIdx].missileAssigned = i;
            // [ZAWAD#2] Initialize velocity
            gMissiles[i].vx = 0.0f;
            gMissiles[i].vy = 0.0f;
            return;
        }
    }
}

static void update(int) {
    if(!gPaused){
        gFrameCounter++;
        if(gFrameCounter > 10000) gFrameCounter = 0;

        gRadarAngle += 2.2f;
        if(gRadarAngle >= 360.0f) gRadarAngle -= 360.0f;

        gSpawnTimer++;
        if(gSpawnTimer >= gSpawnInterval){
            spawnDrone();
            gSpawnTimer = 0;
            if(gSpawnInterval > 80) gSpawnInterval--;
        }

        for(int i = 0; i < MAX_DRONES; i++){
            auto& d = gDrones[i];
            if(!d.active) continue;
            d.x -= d.speed;
            if(d.x < -80){ d.active = false; continue; }

            if(!d.detected){
                float dist = fDist(d.x, d.y, RADAR_X, RADAR_Y);
                if(dist < RADAR_DETECT_RANGE){
                    d.detected = true;
                    launchMissile(i);
                }
            }
        }

        // [ZAWAD#2] Update missiles with physics-based movement
        for(auto& m : gMissiles){
            if(!m.active) continue;
            int ti = m.targetIdx;
            if(ti < 0 || !gDrones[ti].active){
                m.active = false;
                continue;
            }

            float tx = gDrones[ti].x, ty = gDrones[ti].y;
            
            // [ZAWAD#2] Calculate trajectory with physics
            calculateTrajectory(m.x, m.y, m.vx, m.vy, tx, ty);
            
            // [ZAWAD#2] Update position based on velocity
            m.x += m.vx;
            m.y += m.vy;
            
            float d = fDist(m.x, m.y, tx, ty);
            if(d < 18.0f){
                gDrones[ti].active = false;
                m.active = false;
                gScore += 10;
            }
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

static void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    col3(0.01f, 0.04f, 0.16f);
    drawRect(0, 0, WIN_W, WIN_H);

    drawRadar();

    // [ZAWAD#2] Draw trajectory lines for missiles
    for(int i = 0; i < MAX_MISSILES; i++){
        if(gMissiles[i].active){
            drawTrajectoryLine(gMissiles[i].x, gMissiles[i].y, 
                              gMissiles[i].vx, gMissiles[i].vy);
        }
    }

    // [ZAWAD#2] Display physics status
    col3(0.0f, 1.0f, 0.8f);
    drawText(12, 8, "[ZAWAD-2] Physics Engine v2", GLUT_BITMAP_HELVETICA_12);
    
    char buffer[128];
    sprintf(buffer, "Gravity: %.3f | Drag: %.3f | Wind: %.3f", 
            gPhysics.gravity, gPhysics.drag, gPhysics.windForce);
    drawText(12, 28, buffer, GLUT_BITMAP_HELVETICA_12);
    
    glutSwapBuffers();
}

static void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WIN_W, 0, WIN_H);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void keyboard(unsigned char key, int, int) {
    switch(key){
        case 27: exit(0); break;
        case 's': case 'S': spawnDrone(); break;
        case 'p': case 'P': gPaused = !gPaused; break;
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIN_W, WIN_H);
    glutInitWindowPosition(80, 40);
    glutCreateWindow("Air Defense v2 - [ZAWAD-2] Physics Engine");

    glClearColor(0.01f, 0.04f, 0.16f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WIN_W, 0, WIN_H);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    initAll();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, update, 0);

    glutMainLoop();
    return 0;
}
