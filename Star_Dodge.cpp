
#include <GL/glut.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

static const float PI      = 3.14159265358979f;
static const float TWO_PI  = 6.28318530717959f;
static const float DEG2RAD = PI / 180.0f;
static const float RAD2DEG = 180.0f / PI;

static const float GROUND_Y        = 0.0f;
static const float FLY_MAX_Y       = 28.0f;
static const float WORLD_R         = 35.0f;
static const float PLAYER_SPEED    = 0.14f;
static const float PLAYER_ACCEL    = 0.022f;
static const float PLAYER_DRAG     = 0.85f;
static const float PLAYER_RAD      = 0.70f;
static const float REWARD_RAD      = 1.8f;   // LARGE pickup radius
static const float METEOR_RAD_BASE = 0.45f;
static const int   MAX_REWARDS     = 10;
static const int   SCORE_PER_LEVEL = 5;

enum State { MENU, PLAYING, PAUSED, GAMEOVER };
static State gState = MENU;
static int   gScore = 0;
static int   gLevel = 1;
static float gTime  = 0.0f;

// Screen flash effect
static float gFlashR=0, gFlashG=0, gFlashB=0, gFlashA=0;

static int gW = 1024, gH = 700;

struct Camera {
    float yaw   = -30.0f;
    float pitch =  25.0f;
    float dist  =  10.0f;
    int   lastX = -1, lastY = -1;
    bool  dragging = false;
    float ex, ey, ez;
} cam;

struct Player {
    float px = 0.0f, py = 4.0f, pz = 0.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    float spinY = 0.0f;       // UFO slow spin (visual only)
    float hoverBob = 0.0f;    // gentle hover bob
    bool fwd=false, bwd=false, strafeL=false, strafeR=false;
    bool flyUp=false, flyDown=false;
    bool alive = true;
    float flashT = 0.0f;
} ship;

struct Meteor {
    float x,y,z, vx,vy,vz;
    float rotAngle, rotSpd, ax,ay,az;
    float radius, trailPhase;
    bool active;
};
static std::vector<Meteor> gMeteors;

struct Reward {
    float x,y,z, bobPhase, spinY;
    bool active;
    float collectFlash;
};
static std::vector<Reward> gRewards;

struct Particle {
    float x,y,z, vx,vy,vz;
    float life, maxLife;
    float r,g,b, size;
};
static std::vector<Particle> gParts;

// Shockwave ring for collection effect
struct Shockwave {
    float x,y,z, radius, maxR, life, maxLife;
    float r,g,b;
};
static std::vector<Shockwave> gShockwaves;

struct Crater     { float x,z,r,depth; };
struct StarPoint  { float x,y,z,bright; };
struct NebulaBlob { float x,y,z,r,cr,cg,cb,alpha; };

static std::vector<Crater>     gCraters;
static std::vector<StarPoint>  gStarField;
static std::vector<NebulaBlob> gNebula;

static float rf(float lo,float hi){ return lo+(hi-lo)*(rand()/(float)RAND_MAX); }
static float dist3(float ax,float ay,float az,float bx,float by,float bz){
    float dx=ax-bx,dy=ay-by,dz=az-bz; return sqrtf(dx*dx+dy*dy+dz*dz);
}

static void buildStarField(){
    gStarField.clear(); srand(1337);
    for(int i=0;i<2000;i++){
        float theta=rf(0,TWO_PI), phi=acosf(rf(-1.0f,1.0f)), R=280.0f+rf(0,20.0f);
        StarPoint sp;
        sp.x=R*sinf(phi)*cosf(theta); sp.y=R*cosf(phi); sp.z=R*sinf(phi)*sinf(theta);
        sp.bright=rf(0.3f,1.0f);
        gStarField.push_back(sp);
    }
    srand((unsigned)time(nullptr));
}
static void buildNebula(){
    gNebula.clear(); srand(9999);
    struct Pal{float r,g,b;};
    Pal palettes[]={{0.4f,0.1f,0.8f},{0.1f,0.4f,0.9f},{0.8f,0.2f,0.1f},{0.1f,0.7f,0.5f}};
    for(int c=0;c<4;c++){
        float cx=rf(-200,200),cy=rf(40,180),cz=rf(-200,200);
        for(int i=0;i<20;i++){
            NebulaBlob nb;
            nb.x=cx+rf(-30,30); nb.y=cy+rf(-20,20); nb.z=cz+rf(-30,30);
            nb.r=rf(8,22); nb.cr=palettes[c].r; nb.cg=palettes[c].g; nb.cb=palettes[c].b;
            nb.alpha=rf(0.03f,0.09f);
            gNebula.push_back(nb);
        }
    }
    srand((unsigned)time(nullptr));
}
static void buildMoon(){
    gCraters.clear(); srand(42);
    for(int i=0;i<120;i++){
        Crater c; c.x=rf(-WORLD_R,WORLD_R); c.z=rf(-WORLD_R,WORLD_R);
        c.r=rf(0.3f,3.5f); c.depth=rf(0.03f,0.15f);
        gCraters.push_back(c);
    }
    srand((unsigned)time(nullptr));
}

static GLUquadric* gQSph=nullptr, *gQCyl=nullptr, *gQDisk=nullptr;

static void spawnMeteor(){
    float spd=0.06f+gLevel*0.020f+rf(0,0.030f);
    Meteor m;
    m.x=rf(-WORLD_R,WORLD_R); m.y=rf(20.0f,90.0f); m.z=rf(-WORLD_R,WORLD_R); // staggered Y
    float dx=ship.px-m.x, dz=ship.pz-m.z;
    float dlen=sqrtf(dx*dx+dz*dz)+0.001f;
    m.vx=(dx/dlen)*rf(0,0.012f)+rf(-0.005f,0.005f);
    m.vy=-spd; m.vz=(dz/dlen)*rf(0,0.012f)+rf(-0.005f,0.005f);
    m.rotAngle=rf(0,360); m.rotSpd=rf(0.8f,3.5f);
    m.ax=rf(-1,1); m.ay=rf(-1,1); m.az=rf(-1,1);
    float al=sqrtf(m.ax*m.ax+m.ay*m.ay+m.az*m.az)+0.001f;
    m.ax/=al; m.ay/=al; m.az/=al;
    m.radius=rf(0.5f,1.1f); m.trailPhase=rf(0,TWO_PI); m.active=true;
    gMeteors.push_back(m);
}
static void spawnReward(){
    Reward r;
    do{ r.x=rf(-WORLD_R+3,WORLD_R-3); r.z=rf(-WORLD_R+3,WORLD_R-3); }
    while(dist3(r.x,0,r.z,ship.px,0,ship.pz)<5.0f);
    r.y=rf(GROUND_Y+2.5f,FLY_MAX_Y*0.75f);
    r.bobPhase=rf(0,TWO_PI); r.spinY=0; r.active=true; r.collectFlash=0;
    gRewards.push_back(r);
}
static void spawnBurst(float px,float py,float pz,
                        float cr,float cg,float cb,int n,float speed=0.08f){
    for(int i=0;i<n;i++){
        Particle p;
        p.x=px; p.y=py; p.z=pz;
        p.vx=rf(-speed,speed); p.vy=rf(-speed*0.5f,speed*1.5f); p.vz=rf(-speed,speed);
        p.life=p.maxLife=rf(0.6f,1.8f);
        p.r=cr; p.g=cg; p.b=cb; p.size=rf(3.0f,8.0f);
        gParts.push_back(p);
    }
}
static void spawnShockwave(float px,float py,float pz,float r,float g,float b){
    Shockwave sw;
    sw.x=px; sw.y=py; sw.z=pz;
    sw.radius=0.1f; sw.maxR=4.5f;
    sw.life=sw.maxLife=0.6f;
    sw.r=r; sw.g=g; sw.b=b;
    gShockwaves.push_back(sw);
}

static void resetGame(){
    gScore=0; gLevel=1; gTime=0;
    ship=Player();
    gMeteors.clear(); gRewards.clear(); gParts.clear(); gShockwaves.clear();
    gFlashA=0;
    cam.yaw=-30.0f; cam.pitch=25.0f;
    for(int i=0;i<20+gLevel*4;i++) spawnMeteor();
    for(int i=0;i<MAX_REWARDS;i++) spawnReward();
}

// ══════════════════════════════════════════════════════
//  DRAW: SPACE BACKGROUND
// ══════════════════════════════════════════════════════
static void drawSpace(){
    glDepthMask(GL_FALSE); glDisable(GL_LIGHTING);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    glPointSize(2.2f);
    glBegin(GL_POINTS);
    for(auto& s:gStarField){
        float tw=0.7f+0.3f*sinf(gTime*0.015f+s.bright*9.7f);
        float b=s.bright*tw;
        glColor4f(b*0.9f,b*0.95f,b,b);
        glVertex3f(s.x,s.y,s.z);
    }
    glEnd();
    for(auto& nb:gNebula){
        float pulse=1.0f+0.06f*sinf(gTime*0.008f+nb.r);
        glColor4f(nb.cr,nb.cg,nb.cb,nb.alpha*pulse);
        glPushMatrix(); glTranslatef(nb.x,nb.y,nb.z);
        gluSphere(gQSph,nb.r*pulse,6,5); glPopMatrix();
    }
    glDisable(GL_BLEND); glDepthMask(GL_TRUE);
}

// ══════════════════════════════════════════════════════
//  DRAW: MOON GROUND
// ══════════════════════════════════════════════════════
static void drawMoon(){
    float G=WORLD_R+5.0f, Y=GROUND_Y;
    glDisable(GL_BLEND); glDisable(GL_LIGHTING);
    int TILES=22; float step=G*2.0f/TILES;
    for(int iz=0;iz<TILES;iz++){
        for(int ix=0;ix<TILES;ix++){
            float x0=-G+ix*step,z0=-G+iz*step,x1=x0+step,z1=z0+step;
            int hx=ix*7+iz*13;
            float var=0.04f*sinf((float)hx*2.3f)+0.03f*cosf((float)hx*1.7f);
            float base=0.50f+var;
            glColor3f(base,base*0.97f,base*0.93f);
            glBegin(GL_QUADS);
              glVertex3f(x0,Y,z0); glVertex3f(x1,Y,z0);
              glVertex3f(x1,Y,z1); glVertex3f(x0,Y,z1);
            glEnd();
        }
    }
    for(auto& c:gCraters){
        int CSEG=14;
        glColor3f(0.30f,0.29f,0.27f);
        glBegin(GL_TRIANGLE_FAN); glVertex3f(c.x,Y+0.005f,c.z);
        for(int i=0;i<=CSEG;i++){ float a=(float)i/CSEG*TWO_PI; glVertex3f(c.x+c.r*0.6f*cosf(a),Y+0.005f,c.z+c.r*0.6f*sinf(a)); }
        glEnd();
        glColor3f(0.66f,0.65f,0.61f);
        glBegin(GL_TRIANGLE_STRIP);
        for(int i=0;i<=CSEG;i++){ float a=(float)i/CSEG*TWO_PI,ci=cosf(a),si=sinf(a);
            glVertex3f(c.x+c.r*0.7f*ci,Y+0.007f,c.z+c.r*0.7f*si);
            glVertex3f(c.x+c.r*1.05f*ci,Y+0.005f,c.z+c.r*1.05f*si);
        } glEnd();
    }
}

// ══════════════════════════════════════════════════════
//  DRAW: UFO SAUCER SHIP  (Beautiful symmetric design)
//
//  - Wide flat saucer body (main disc)
//  - Raised central dome (glass cockpit)
//  - Glowing ring bands around disc edge
//  - Bottom thruster nozzle ring
//  - Colored running lights around rim
//  - Inner dome glow (teal/green)
//  - Slowly spins on Y axis (visual only)
// ══════════════════════════════════════════════════════
static void drawUFO(){
    glEnable(GL_LIGHTING);

    // ── 1. MAIN SAUCER DISC ───────────────────────────
    // Top surface (slightly domed upward)
    int DISC_SEG = 40;
    float discR = 1.4f;

    // Top disc surface — metallic silver-blue
    glColor3f(0.55f, 0.65f, 0.80f);
    glBegin(GL_TRIANGLE_FAN);
      glNormal3f(0,1,0);
      glVertex3f(0, 0.18f, 0); // center elevated
      for(int i=0;i<=DISC_SEG;i++){
          float a=(float)i/DISC_SEG*TWO_PI;
          float r=discR, yOff=0.0f;
          // gentle dome curvature toward edge
          float t=(float)i/DISC_SEG;
          glNormal3f(sinf(a)*0.2f,1.0f,cosf(a)*0.2f);
          glVertex3f(r*sinf(a), yOff, r*cosf(a));
      }
    glEnd();

    // Bottom disc surface — darker
    glColor3f(0.30f, 0.38f, 0.52f);
    glBegin(GL_TRIANGLE_FAN);
      glNormal3f(0,-1,0);
      glVertex3f(0, -0.22f, 0);
      for(int i=DISC_SEG;i>=0;i--){
          float a=(float)i/DISC_SEG*TWO_PI;
          glNormal3f(sinf(a)*0.15f,-1.0f,cosf(a)*0.15f);
          glVertex3f(discR*sinf(a), -0.10f, discR*cosf(a));
      }
    glEnd();

    // Edge band (the rim — connecting top and bottom edge)
    glColor3f(0.42f, 0.52f, 0.70f);
    glBegin(GL_TRIANGLE_STRIP);
    for(int i=0;i<=DISC_SEG;i++){
        float a=(float)i/DISC_SEG*TWO_PI;
        float nx=sinf(a), nz=cosf(a);
        glNormal3f(nx,0.1f,nz);
        glVertex3f(discR*sinf(a),  0.02f, discR*cosf(a)); // top rim
        glVertex3f(discR*sinf(a), -0.10f, discR*cosf(a)); // bottom rim
    }
    glEnd();

    // Inner ridge ring (decorative panel, slightly inside rim)
    float ridgeR = discR * 0.32f;
    glColor3f(0.48f, 0.58f, 0.76f);
    glBegin(GL_TRIANGLE_STRIP);
    for(int i=0;i<=DISC_SEG;i++){
        float a=(float)i/DISC_SEG*TWO_PI;
        glNormal3f(0,1,0);
        glVertex3f(ridgeR*sinf(a), 0.20f, ridgeR*cosf(a));
        glVertex3f(ridgeR*sinf(a), 0.16f, ridgeR*cosf(a));
    }
    glEnd();

    // ── 2. DOME (glass cockpit) ───────────────────────
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    // Outer glass dome — translucent cyan-blue
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.25f, 0.80f, 1.0f, 0.45f);
    glPushMatrix();
      glTranslatef(0, 0.18f, 0);
      glScalef(1.0f, 0.80f, 1.0f);
      gluSphere(gQSph, 0.52f, 22, 14);
    glPopMatrix();
    // Inner glow (bright teal core)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(0.10f, 1.0f, 0.80f, 0.30f);
    glPushMatrix();
      glTranslatef(0, 0.18f, 0);
      glScalef(1.0f, 0.75f, 1.0f);
      gluSphere(gQSph, 0.38f, 14, 10);
    glPopMatrix();
    // Core bright spot
    glColor4f(0.80f, 1.0f, 1.0f, 0.55f);
    glPushMatrix();
      glTranslatef(0, 0.32f, 0);
      gluSphere(gQSph, 0.12f, 8, 6);
    glPopMatrix();

    // Dome frame ring (metallic base of dome)
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glColor3f(0.35f, 0.42f, 0.60f);
    glPushMatrix();
      glTranslatef(0, 0.18f, 0);
      glRotatef(90, 1,0,0);
      gluCylinder(gQCyl, 0.53f, 0.53f, 0.04f, 22, 1);
    glPopMatrix();

    // ── 3. GLOWING RINGS ─────────────────────────────
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    // Ring 1 — outer rim glow (rotates)
    float ringRot1 = gTime * 1.8f;
    float ringPulse = 0.7f + 0.3f*sinf(gTime*0.12f);
    glPushMatrix();
      glTranslatef(0, -0.04f, 0);
      glRotatef(ringRot1, 0,1,0);
      int RSEG=36;
      // Outer glow band
      glBegin(GL_TRIANGLE_STRIP);
      for(int i=0;i<=RSEG;i++){
          float a=(float)i/RSEG*TWO_PI;
          float r1=discR*0.88f, r2=discR*1.02f;
          glColor4f(0.20f, 0.70f, 1.0f, ringPulse*0.55f);
          glVertex3f(r2*sinf(a), 0.02f, r2*cosf(a));
          glColor4f(0.10f, 0.40f, 0.90f, 0.0f);
          glVertex3f(r1*sinf(a), 0.0f,  r1*cosf(a));
      }
      glEnd();
      // Bright ring line
      glLineWidth(2.5f);
      glColor4f(0.40f, 0.85f, 1.0f, ringPulse*0.90f);
      glBegin(GL_LINE_LOOP);
      for(int i=0;i<RSEG;i++){
          float a=(float)i/RSEG*TWO_PI;
          glVertex3f(discR*0.96f*sinf(a), 0.01f, discR*0.96f*cosf(a));
      }
      glEnd();
    glPopMatrix();

    // Ring 2 — inner counter-rotating ring
    float ringRot2 = -gTime * 2.5f;
    glPushMatrix();
      glTranslatef(0, 0.16f, 0);
      glRotatef(ringRot2, 0,1,0);
      glLineWidth(1.8f);
      glColor4f(0.60f, 1.0f, 0.50f, ringPulse*0.80f);
      glBegin(GL_LINE_LOOP);
      for(int i=0;i<32;i++){
          float a=(float)i/32*TWO_PI;
          glVertex3f(ridgeR*0.95f*sinf(a), 0.0f, ridgeR*0.95f*cosf(a));
      }
      glEnd();
    glPopMatrix();

    glLineWidth(1.0f);

    // ── 4. RIM RUNNING LIGHTS ─────────────────────────
    // 8 colored lights evenly spaced around rim
    int LIGHTS=8;
    for(int li=0;li<LIGHTS;li++){
        float a=(float)li/LIGHTS*TWO_PI + gTime*0.03f;
        float phase=(float)li/LIGHTS*TWO_PI;
        float blink=0.6f+0.4f*sinf(gTime*0.18f+phase);
        // Alternate colors: cyan, magenta, yellow
        float lr = (li%3==0)?1.0f:(li%3==1)?0.2f:0.2f;
        float lg = (li%3==0)?0.3f:(li%3==1)?0.3f:1.0f;
        float lb = (li%3==0)?0.3f:(li%3==1)?1.0f:0.3f;
        glColor4f(lr,lg,lb,blink);
        glPushMatrix();
          glTranslatef(discR*0.92f*sinf(a), -0.06f, discR*0.92f*cosf(a));
          gluSphere(gQSph, 0.055f, 6, 4);
        glPopMatrix();
    }

    // ── 5. BOTTOM THRUSTER RING ───────────────────────
    // Concentric nozzle ring under center
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LIGHTING);
    glColor3f(0.20f, 0.22f, 0.32f);
    glPushMatrix();
      glTranslatef(0, -0.22f, 0);
      glRotatef(90,1,0,0);
      gluCylinder(gQCyl, 0.28f, 0.22f, 0.14f, 20, 2);
    glPopMatrix();
    // Thruster glow ring (always slightly on for hover)
    glDisable(GL_LIGHTING);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    float hoverGlow=0.45f+0.15f*sinf(gTime*0.09f);
    glColor4f(0.30f, 0.80f, 1.0f, hoverGlow);
    glPushMatrix();
      glTranslatef(0,-0.36f,0);
      glRotatef(90,1,0,0);
      gluCylinder(gQCyl,0.22f,0.0f,0.20f,16,3);
    glPopMatrix();
    // Outer glow cone
    glColor4f(0.10f,0.50f,0.90f,hoverGlow*0.40f);
    glPushMatrix();
      glTranslatef(0,-0.22f,0);
      glRotatef(90,1,0,0);
      gluCylinder(gQCyl,0.38f,0.0f,0.30f,16,2);
    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// ══════════════════════════════════════════════════════
//  DRAW: METEOR
// ══════════════════════════════════════════════════════
static void drawMeteor(const Meteor& m){
    glPushMatrix();
    glTranslatef(m.x,m.y,m.z);
    glRotatef(m.rotAngle,m.ax,m.ay,m.az);
    float sc=m.radius;
    glEnable(GL_LIGHTING);
    glColor3f(0.32f,0.27f,0.22f);
    gluSphere(gQSph,sc*0.75f,10,7);
    glColor3f(0.24f,0.20f,0.17f);
    float lumpA[]={0,60,120,180,240,300,30,90,150};
    for(int li=0;li<9;li++){
        float la=lumpA[li]*DEG2RAD, le=(li<6?0.0f:45.0f)*DEG2RAD;
        float lr=sc*rf(0.35f,0.65f);
        glPushMatrix();
        glTranslatef(cosf(la)*cosf(le)*sc*0.7f,sinf(le)*sc*0.7f,sinf(la)*cosf(le)*sc*0.7f);
        gluSphere(gQSph,lr,6,4); glPopMatrix();
    }
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    float pulse=0.7f+0.3f*sinf(gTime*0.1f+m.trailPhase);
    glColor4f(1.0f,0.6f,0.1f,0.55f*pulse); gluSphere(gQSph,sc*0.55f,8,6);
    glColor4f(1.0f,0.35f,0.05f,0.15f);     gluSphere(gQSph,sc*1.6f,6,5);
    glDisable(GL_BLEND);
    glPopMatrix();

    float speed=sqrtf(m.vx*m.vx+m.vy*m.vy+m.vz*m.vz);
    if(speed>0.001f){
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE);
        float trailLen=m.radius*3.5f+speed*12.0f;
        float nx=-m.vx/speed,ny=-m.vy/speed,nz=-m.vz/speed;
        for(int ti=1;ti<=8;ti++){
            float t=(float)ti/8.0f;
            float tx=m.x+nx*trailLen*t,ty=m.y+ny*trailLen*t,tz=m.z+nz*trailLen*t;
            float alpha=(1.0f-t)*0.55f, rad=m.radius*(1.0f-t*0.7f)*0.4f;
            glColor4f(1.0f,0.4f+0.5f*(1.0f-t),0.1f*(1.0f-t),alpha);
            glPushMatrix(); glTranslatef(tx,ty,tz);
            gluSphere(gQSph,rad,5,4); glPopMatrix();
        }
        glDisable(GL_BLEND);
    }
}

// ══════════════════════════════════════════════════════
//  DRAW: REWARD ORB — LARGE AND GORGEOUS
// ══════════════════════════════════════════════════════
static void drawRewardOrb(const Reward& r){
    float pulse=0.18f*sinf(gTime*0.07f+r.bobPhase);
    float rr=0.52f;
    glPushMatrix();
    float bob=0.28f*sinf(gTime*0.065f+r.bobPhase);
    glTranslatef(r.x,r.y+bob,r.z);
    glRotatef(r.spinY,0,1,0);

    glEnable(GL_LIGHTING);
    glColor3f(1.0f,0.92f,0.20f);
    gluSphere(gQSph,rr*0.45f,12,10);

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE);

    // Main glow sphere
    glColor4f(0.5f,1.0f,0.3f,0.55f+pulse*0.3f);
    gluSphere(gQSph,rr*(0.85f+pulse),10,8);

    // Outer halo (extra large — matches pickup radius visually)
    glColor4f(0.3f,1.0f,0.5f,0.12f+pulse*0.06f);
    gluSphere(gQSph,rr*2.8f,8,6);

    // Inner bright core
    glColor4f(1.0f,1.0f,0.7f,0.80f);
    gluSphere(gQSph,rr*0.28f,8,6);

    // 3 rotating rings at different angles
    for(int ri=0;ri<3;ri++){
        float ringA=gTime*(1.5f+ri*0.7f)+(float)ri*60.0f;
        float ringPulse=0.7f+0.3f*sinf(gTime*0.1f+ri*2.1f);
        glPushMatrix();
          glRotatef(ringA, (ri==0?0:ri==1?1:0.5f),(ri==0?1:ri==1?0:0.5f),(ri==2?1:0));
          glColor4f(1.0f-ri*0.2f, 0.8f+ri*0.1f, 0.2f+ri*0.3f, ringPulse*0.75f);
          glBegin(GL_LINE_LOOP);
          for(int si=0;si<28;si++){
              float a=(float)si/28*TWO_PI;
              glVertex3f(cosf(a)*rr*1.25f,0,sinf(a)*rr*1.25f);
          }
          glEnd();
        glPopMatrix();
    }

    glDisable(GL_BLEND);
    glPopMatrix();
}

// ══════════════════════════════════════════════════════
//  DRAW: PARTICLES
// ══════════════════════════════════════════════════════
static void drawParticles(){
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    glDepthMask(GL_FALSE);
    for(auto& p:gParts){
        float a=p.life/p.maxLife;
        glColor4f(p.r,p.g*a+0.5f*(1-a),p.b*a,a*0.9f);
        glPointSize(p.size*a+1.0f);
        glBegin(GL_POINTS); glVertex3f(p.x,p.y,p.z); glEnd();
    }
    glDepthMask(GL_TRUE); glDisable(GL_BLEND);
}

// ══════════════════════════════════════════════════════
//  DRAW: SHOCKWAVES (collection effect)
// ══════════════════════════════════════════════════════
static void drawShockwaves(){
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    glDepthMask(GL_FALSE);
    for(auto& sw:gShockwaves){
        float a=sw.life/sw.maxLife;
        glColor4f(sw.r,sw.g,sw.b,a*0.8f);
        glLineWidth(3.0f*a+1.0f);
        glPushMatrix();
          glTranslatef(sw.x,sw.y,sw.z);
          // Horizontal ring
          glBegin(GL_LINE_LOOP);
          for(int i=0;i<36;i++){
              float ang=(float)i/36*TWO_PI;
              glVertex3f(cosf(ang)*sw.radius,0,sinf(ang)*sw.radius);
          }
          glEnd();
          // Vertical rings
          glBegin(GL_LINE_LOOP);
          for(int i=0;i<36;i++){
              float ang=(float)i/36*TWO_PI;
              glVertex3f(cosf(ang)*sw.radius,sinf(ang)*sw.radius,0);
          }
          glEnd();
        glPopMatrix();
        glLineWidth(1.0f);
    }
    glDepthMask(GL_TRUE); glDisable(GL_BLEND);
}

// ══════════════════════════════════════════════════════
//  LIGHTING
// ══════════════════════════════════════════════════════
static void setupLighting(){
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    GLfloat amb[]={0.14f,0.14f,0.20f,1.0f};
    GLfloat diff[]={0.85f,0.80f,0.75f,1.0f};
    GLfloat spec[]={0.65f,0.65f,0.85f,1.0f};
    GLfloat pos[]={50.0f,80.0f,30.0f,1.0f};
    glLightfv(GL_LIGHT0,GL_AMBIENT,amb);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,diff);
    glLightfv(GL_LIGHT0,GL_SPECULAR,spec);
    glLightfv(GL_LIGHT0,GL_POSITION,pos);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    GLfloat matSpec[]={0.6f,0.6f,0.6f,1.0f};
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,matSpec);
    glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,60.0f);
}

// ══════════════════════════════════════════════════════
//  HUD
// ══════════════════════════════════════════════════════
static void hud_begin(){
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0,gW,0,gH);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
}
static void hud_end(){
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glEnable(GL_DEPTH_TEST);
}
static void hud_text(float x,float y,const char* s,float r,float g,float b,
                      void* fnt=GLUT_BITMAP_HELVETICA_18){
    glColor3f(r,g,b); glRasterPos2f(x,y);
    for(const char* c=s;*c;c++) glutBitmapCharacter(fnt,*c);
}
static void hud_box(float x,float y,float w,float h,float r,float g,float b,float a){
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r,g,b,a);
    glBegin(GL_QUADS);
      glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h);
    glEnd();
    glDisable(GL_BLEND);
}
static void hud_border(float x,float y,float w,float h,float r,float g,float b){
    glColor3f(r,g,b); glLineWidth(1.8f);
    glBegin(GL_LINE_LOOP);
      glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h);
    glEnd(); glLineWidth(1.0f);
}

// ══════════════════════════════════════════════════════
//  UPDATE — SMOOTH CAMERA-RELATIVE MOVEMENT
// ══════════════════════════════════════════════════════
static void update(){
    // Always tick gTime for menus/animations
    gTime+=1.0f;
    // Flash always decays
    gFlashA=fmaxf(0.0f,gFlashA-(gState==GAMEOVER ? 0.012f : 0.045f));

    if(gState!=PLAYING) return;

    // Camera-relative axes (yaw only — ignore pitch for movement)
    float camYawRad=cam.yaw*DEG2RAD;
    // Camera looks FROM (sin(yaw),.,.-cos(yaw)) TOWARD ship
    // So forward = direction camera is looking = -sin(yaw), 0, cos(yaw)
    float camFwdX=-sinf(camYawRad), camFwdZ= cosf(camYawRad);
    float camRgtX= cosf(camYawRad), camRgtZ= sinf(camYawRad);

    float ax=0,ay=0,az=0; bool moving=false;
    if(ship.fwd)    { ax+=camFwdX*PLAYER_ACCEL; az+=camFwdZ*PLAYER_ACCEL; moving=true; }
    if(ship.bwd)    { ax-=camFwdX*PLAYER_ACCEL; az-=camFwdZ*PLAYER_ACCEL; moving=true; }
    if(ship.strafeR){ ax+=camRgtX*PLAYER_ACCEL; az+=camRgtZ*PLAYER_ACCEL; moving=true; }
    if(ship.strafeL){ ax-=camRgtX*PLAYER_ACCEL; az-=camRgtZ*PLAYER_ACCEL; moving=true; }
    if(ship.flyUp)  { ay+=PLAYER_ACCEL; moving=true; }
    if(ship.flyDown){ ay-=PLAYER_ACCEL; moving=true; }

    ship.vx=(ship.vx+ax)*PLAYER_DRAG;
    ship.vy=(ship.vy+ay)*PLAYER_DRAG;
    ship.vz=(ship.vz+az)*PLAYER_DRAG;

    float spd=sqrtf(ship.vx*ship.vx+ship.vy*ship.vy+ship.vz*ship.vz);
    if(spd>PLAYER_SPEED){ float sc=PLAYER_SPEED/spd; ship.vx*=sc; ship.vy*=sc; ship.vz*=sc; }

    ship.px+=ship.vx; ship.py+=ship.vy; ship.pz+=ship.vz;
    ship.px=fmaxf(-WORLD_R+1,fminf(WORLD_R-1,ship.px));
    ship.pz=fmaxf(-WORLD_R+1,fminf(WORLD_R-1,ship.pz));
    ship.py=fmaxf(GROUND_Y+1.2f,fminf(FLY_MAX_Y,ship.py));

    // UFO slow spin — always rotating gently
    ship.spinY+=0.8f;
    ship.hoverBob=0.18f*sinf(gTime*0.04f);

    // Meteors
    int maxM=20+gLevel*4;
    while((int)gMeteors.size()<maxM) spawnMeteor();
    for(auto& m:gMeteors){
        if(!m.active) continue;
        m.x+=m.vx; m.y+=m.vy; m.z+=m.vz;
        m.rotAngle+=m.rotSpd;
        if(rand()%5==0) spawnBurst(m.x,m.y,m.z,1.0f,0.5f,0.1f,2,0.04f);
        if(m.y<GROUND_Y-3.0f){
            // Full respawn at top with fresh random position and speed
            float spd=0.06f+gLevel*0.020f+rf(0,0.030f);
            m.x=rf(-WORLD_R,WORLD_R); m.z=rf(-WORLD_R,WORLD_R);
            m.y=rf(58,90);  // staggered heights so rain looks continuous
            float dx=ship.px-m.x, dz=ship.pz-m.z;
            float dlen=sqrtf(dx*dx+dz*dz)+0.001f;
            m.vx=(dx/dlen)*rf(0,0.014f)+rf(-0.006f,0.006f);
            m.vy=-spd;
            m.vz=(dz/dlen)*rf(0,0.014f)+rf(-0.006f,0.006f);
            m.radius=rf(0.5f,1.1f);
        }
        if(dist3(m.x,m.y,m.z,ship.px,ship.py,ship.pz)<PLAYER_RAD+m.radius*2.2f){
            // MASSIVE death explosion — 5 burst waves
            spawnBurst(ship.px,ship.py,ship.pz, 1.0f,0.9f,0.2f, 80, 0.22f); // gold core
            spawnBurst(ship.px,ship.py,ship.pz, 1.0f,0.3f,0.0f, 70, 0.16f); // orange ring
            spawnBurst(ship.px,ship.py,ship.pz, 1.0f,1.0f,1.0f, 50, 0.28f); // white flash
            spawnBurst(ship.px,ship.py,ship.pz, 0.4f,0.6f,1.0f, 40, 0.12f); // blue outer
            spawnBurst(m.x,m.y,m.z,            1.0f,0.6f,0.1f, 60, 0.14f); // meteor debris
            // 3 expanding shockwave rings
            spawnShockwave(ship.px,ship.py,ship.pz, 1.0f,0.5f,0.1f);
            spawnShockwave(ship.px,ship.py,ship.pz, 1.0f,0.9f,0.3f);
            gShockwaves.back().maxR=9.0f;
            spawnShockwave(ship.px,ship.py,ship.pz, 0.8f,0.3f,1.0f);
            gShockwaves.back().maxR=14.0f; gShockwaves.back().maxLife=1.1f; gShockwaves.back().life=1.1f;
            // Blazing red-white screen flash
            gFlashR=1.0f; gFlashG=0.85f; gFlashB=0.4f; gFlashA=1.0f;
            gState=GAMEOVER; return;
        }
    }
    gMeteors.erase(std::remove_if(gMeteors.begin(),gMeteors.end(),
        [](const Meteor& m){return !m.active;}),gMeteors.end());

    // Rewards — LARGE collection range + BOOM effect
    while((int)gRewards.size()<MAX_REWARDS) spawnReward();
    for(auto& r:gRewards){
        if(!r.active) continue;
        r.spinY+=1.8f;
        if(dist3(r.x,r.y,r.z,ship.px,ship.py,ship.pz)<PLAYER_RAD+REWARD_RAD){
            r.active=false; gScore++;
            // Big colorful BOOM
            spawnBurst(r.x,r.y,r.z, 0.4f,1.0f,0.3f, 50, 0.14f);
            spawnBurst(r.x,r.y,r.z, 1.0f,0.9f,0.2f, 40, 0.10f);
            spawnBurst(r.x,r.y,r.z, 0.2f,0.7f,1.0f, 30, 0.18f);
            spawnShockwave(r.x,r.y,r.z, 0.3f,1.0f,0.5f);
            // 2nd shockwave (offset time)
            gShockwaves.back().maxR=6.0f;
            // Screen flash — bright green/gold
            gFlashR=0.4f; gFlashG=1.0f; gFlashB=0.3f; gFlashA=0.55f;
            if(gScore%SCORE_PER_LEVEL==0){
                gLevel++;
                for(auto& m:gMeteors) m.vy*=1.18f;
            }
        }
    }
    gRewards.erase(std::remove_if(gRewards.begin(),gRewards.end(),
        [](const Reward& r){return !r.active;}),gRewards.end());

    // Shockwaves
    for(auto& sw:gShockwaves){
        sw.radius+=sw.maxR/18.0f;
        sw.life-=sw.maxLife/18.0f;
    }
    gShockwaves.erase(std::remove_if(gShockwaves.begin(),gShockwaves.end(),
        [](const Shockwave& sw){return sw.life<=0;}),gShockwaves.end());

    // Particles
    for(auto& p:gParts){ p.x+=p.vx; p.y+=p.vy; p.z+=p.vz; p.vy-=0.002f; p.life-=0.020f; }
    gParts.erase(std::remove_if(gParts.begin(),gParts.end(),
        [](const Particle& p){return p.life<=0;}),gParts.end());

    glutPostRedisplay();
}

// ══════════════════════════════════════════════════════
//  DISPLAY
// ══════════════════════════════════════════════════════
static void display(){
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    float cy=cam.yaw*DEG2RAD, cp=cam.pitch*DEG2RAD, d=cam.dist;
    cam.ex=ship.px+d*sinf(cy)*cosf(cp);
    cam.ey=ship.py+d*sinf(cp)+0.8f;
    cam.ez=ship.pz-d*cosf(cy)*cosf(cp);
    gluLookAt(cam.ex,cam.ey,cam.ez, ship.px,ship.py+0.2f,ship.pz, 0,1,0);

    setupLighting();
    glDisable(GL_LIGHTING);
    drawSpace();
    glEnable(GL_LIGHTING);
    drawMoon();
    for(auto& m:gMeteors) if(m.active) drawMeteor(m);
    glDisable(GL_LIGHTING);
    for(auto& r:gRewards) if(r.active) drawRewardOrb(r);

    // Draw UFO — always level (no bank), gentle hover bob
    glEnable(GL_LIGHTING);
    glPushMatrix();
      glTranslatef(ship.px, ship.py+ship.hoverBob, ship.pz);
      glRotatef(ship.spinY, 0,1,0);  // slow visual spin only
      drawUFO();
    glPopMatrix();

    glDisable(GL_LIGHTING);
    drawParticles();
    drawShockwaves();

    // ── HUD ─────────────────────────────────────────────
    hud_begin();

    // Full-screen flash overlay
    if(gFlashA>0.01f){
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(gFlashR,gFlashG,gFlashB,gFlashA);
        glBegin(GL_QUADS);
          glVertex2f(0,0); glVertex2f(gW,0); glVertex2f(gW,gH); glVertex2f(0,gH);
        glEnd();
        glDisable(GL_BLEND);
    }

    if(gState==MENU){
        hud_box(0,0,(float)gW,(float)gH, 0,0,0.05f,0.78f);
        hud_box(gW/2.0f-190,gH/2.0f+100,380,56, 0.05f,0.08f,0.20f,0.88f);
        hud_border(gW/2.0f-190,gH/2.0f+100,380,56, 0.3f,0.7f,1.0f);
        hud_text(gW/2.0f-155,gH/2.0f+122,"★  S T A R   D O D G E  ★",1.0f,0.85f,0.2f,GLUT_BITMAP_TIMES_ROMAN_24);
        hud_box(gW/2.0f-240,gH/2.0f-20,480,120, 0.03f,0.04f,0.14f,0.80f);
        hud_text(gW/2.0f-215,gH/2.0f+80,"Collect glowing orbs — Dodge falling meteors!",0.5f,1.0f,0.6f);
        hud_text(gW/2.0f-215,gH/2.0f+52,"W/S — Forward/Back  |  A/D — Strafe Left/Right",0.75f,0.85f,1.0f);
        hud_text(gW/2.0f-190,gH/2.0f+26,"Q/E — Up/Down  |  Mouse Drag — Rotate Camera",0.75f,0.85f,1.0f);
        float blink=0.55f+0.45f*sinf(gTime*0.08f);
        hud_text(gW/2.0f-110,gH/2.0f-10,"Press  ENTER  to Launch",blink,blink*0.5f,0.1f,GLUT_BITMAP_TIMES_ROMAN_24);

    } else if(gState==PAUSED){
        // Score bar still visible behind
        hud_box(0,gH-52,(float)gW,52, 0.0f,0.02f,0.10f,0.70f);
        hud_border(0,gH-52,(float)gW,52, 0.2f,0.5f,0.9f);
        { char buf[64];
          snprintf(buf,sizeof(buf),"SCORE  %d",gScore);
          hud_text(20,gH-32,buf,0.3f,1.0f,0.5f,GLUT_BITMAP_TIMES_ROMAN_24);
          snprintf(buf,sizeof(buf),"LEVEL  %d",gLevel);
          hud_text(gW/2.0f-50,gH-32,buf,1.0f,0.85f,0.2f,GLUT_BITMAP_TIMES_ROMAN_24);
        }
        // Dim overlay
        hud_box(0,0,(float)gW,(float)gH, 0.0f,0.0f,0.06f,0.62f);
        // Pause panel
        hud_box(gW/2.0f-185,gH/2.0f-80,370,210, 0.03f,0.06f,0.20f,0.95f);
        hud_border(gW/2.0f-185,gH/2.0f-80,370,210, 0.3f,0.75f,1.0f);
        hud_text(gW/2.0f-72,gH/2.0f+108,"PAUSED",0.3f,0.85f,1.0f,GLUT_BITMAP_TIMES_ROMAN_24);
        hud_text(gW/2.0f-155,gH/2.0f+62,"WASD — Move   |   Q/E — Up/Down",0.75f,0.88f,1.0f);
        hud_text(gW/2.0f-155,gH/2.0f+36,"Mouse Drag — Rotate Camera",0.75f,0.88f,1.0f);
        hud_text(gW/2.0f-155,gH/2.0f+10,"Collect glowing orbs to score!",0.55f,1.0f,0.60f);
        hud_text(gW/2.0f-155,gH/2.0f-16,"Avoid the falling meteors!",1.0f,0.55f,0.30f);
        { float blink=0.6f+0.4f*sinf(gTime*0.07f);
          hud_text(gW/2.0f-148,gH/2.0f-54,"Press  P  or  ENTER  to Resume",blink,blink*0.85f,0.2f,GLUT_BITMAP_HELVETICA_18);
        }

    } else if(gState==GAMEOVER){
        hud_box(gW/2.0f-225,gH/2.0f-85,450,230, 0.02f,0.0f,0.08f,0.93f);
        hud_border(gW/2.0f-225,gH/2.0f-85,450,230, 0.9f,0.2f,0.1f);
        hud_text(gW/2.0f-105,gH/2.0f+122,"GAME  OVER",1.0f,0.15f,0.1f,GLUT_BITMAP_TIMES_ROMAN_24);
        { char buf[64];
          snprintf(buf,sizeof(buf),"Score : %d",gScore);
          hud_text(gW/2.0f-88,gH/2.0f+80,buf,1.0f,0.9f,0.2f,GLUT_BITMAP_TIMES_ROMAN_24);
          snprintf(buf,sizeof(buf),"Level Reached : %d",gLevel);
          hud_text(gW/2.0f-105,gH/2.0f+46,buf,0.6f,0.8f,1.0f);
          snprintf(buf,sizeof(buf),"Meteors Active : %d",(int)gMeteors.size());
          hud_text(gW/2.0f-115,gH/2.0f+16,buf,0.7f,0.7f,0.8f);
          float blink=0.6f+0.4f*sinf(gTime*0.09f);
          hud_text(gW/2.0f-135,gH/2.0f-28,"Press  ENTER  to Play Again",blink,blink,blink,GLUT_BITMAP_HELVETICA_18);
        }

    } else {
        hud_box(0,gH-52,(float)gW,52, 0.0f,0.02f,0.10f,0.70f);
        hud_border(0,gH-52,(float)gW,52, 0.2f,0.5f,0.9f);
        char buf[64];
        snprintf(buf,sizeof(buf),"SCORE  %d",gScore);
        hud_text(20,gH-32,buf,0.3f,1.0f,0.5f,GLUT_BITMAP_TIMES_ROMAN_24);
        snprintf(buf,sizeof(buf),"LEVEL  %d",gLevel);
        hud_text(gW/2.0f-50,gH-32,buf,1.0f,0.85f,0.2f,GLUT_BITMAP_TIMES_ROMAN_24);
        snprintf(buf,sizeof(buf),"METEORS  %d",(int)gMeteors.size());
        hud_text(gW-200.0f,gH-32,buf,1.0f,0.4f,0.3f);

        // Altitude bar
        float altPct=(ship.py-(GROUND_Y+1.2f))/(FLY_MAX_Y-GROUND_Y-1.2f);
        altPct=fmaxf(0.0f,fminf(1.0f,altPct));
        float barH=150.0f,barY=55.0f,barX=gW-30.0f;
        hud_box(barX,barY,14,barH, 0.05f,0.05f,0.15f,0.70f);
        float cr=altPct>0.75f?1.0f:altPct*1.3f;
        float cg=altPct>0.75f?(1.0f-(altPct-0.75f)*4.0f):1.0f;
        hud_box(barX,barY,14,barH*altPct, cr,cg,0.2f,0.85f);
        hud_border(barX,barY,14,barH, 0.3f,0.5f,0.9f);
        hud_text(barX-8,barY+barH+5,"ALT",0.5f,0.7f,1.0f,GLUT_BITMAP_HELVETICA_12);

        // Level progress bar
        float lp=(float)(gScore%SCORE_PER_LEVEL)/SCORE_PER_LEVEL;
        hud_box(gW/2.0f-110,gH-16,220,10, 0.05f,0.05f,0.15f,0.7f);
        hud_box(gW/2.0f-110,gH-16,220*lp,10, 0.2f,0.8f,1.0f,0.9f);
        hud_border(gW/2.0f-110,gH-16,220,10, 0.2f,0.5f,0.9f);
        hud_text(10,38,"WASD: Move  |  Q/E: Up/Down  |  Mouse: Camera",0.35f,0.45f,0.65f,GLUT_BITMAP_HELVETICA_12);

        if(ship.py>FLY_MAX_Y-4.0f){
            float w=0.5f+0.5f*sinf(gTime*0.2f);
            hud_box(0,gH-80,(float)gW,28, 0.8f,0.2f,0.0f,w*0.35f);
            hud_text(gW/2.0f-90,gH-70,"MAX ALTITUDE REACHED",1.0f,0.4f,0.1f);
        }
    }

    hud_end();
    glutSwapBuffers();
}

// ══════════════════════════════════════════════════════
//  GLUT CALLBACKS
// ══════════════════════════════════════════════════════
static void reshape(int w,int h){
    gW=w; gH=(h==0?1:h);
    glViewport(0,0,gW,gH);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(62.0,(double)gW/gH,0.2,500.0);
    glMatrixMode(GL_MODELVIEW);
}
static void timerCB(int){ update(); glutTimerFunc(16,timerCB,0); glutPostRedisplay(); }

static void keyDown(unsigned char k,int,int){
    switch(k){
        case 'w': case 'W': ship.fwd    =true; break;
        case 's': case 'S': ship.bwd    =true; break;
        case 'a': case 'A': ship.strafeR=true; break;   // A = screen-left = camRgt negative = strafeR maps to -camRgt
        case 'd': case 'D': ship.strafeL=true; break;
        case 'q': case 'Q': ship.flyUp  =true; break;
        case 'e': case 'E': ship.flyDown=true; break;
        case ' ':            ship.flyUp =true; break;
        case 'p': case 'P':
            if(gState==PLAYING) gState=PAUSED;
            else if(gState==PAUSED) gState=PLAYING;
            break;
        case 13:
            if(gState==MENU||gState==GAMEOVER){ resetGame(); gState=PLAYING; }
            else if(gState==PAUSED) gState=PLAYING;
            break;
        case 27: exit(0);
    }
}
static void keyUp(unsigned char k,int,int){
    switch(k){
        case 'w': case 'W': ship.fwd    =false; break;
        case 's': case 'S': ship.bwd    =false; break;
        case 'a': case 'A': ship.strafeR=false; break;
        case 'd': case 'D': ship.strafeL=false; break;
        case 'q': case 'Q': ship.flyUp  =false; break;
        case 'e': case 'E': ship.flyDown=false; break;
        case ' ':            ship.flyUp =false; break;
    }
}
static void specialDown(int k,int,int){
    switch(k){
        case GLUT_KEY_UP:    ship.fwd    =true;  break;
        case GLUT_KEY_DOWN:  ship.bwd    =true;  break;
        case GLUT_KEY_LEFT:  ship.strafeR=true;  break;
        case GLUT_KEY_RIGHT: ship.strafeL=true;  break;
    }
}
static void specialUp(int k,int,int){
    switch(k){
        case GLUT_KEY_UP:    ship.fwd    =false; break;
        case GLUT_KEY_DOWN:  ship.bwd    =false; break;
        case GLUT_KEY_LEFT:  ship.strafeR=false; break;
        case GLUT_KEY_RIGHT: ship.strafeL=false; break;
    }
}
static void mouseBtn(int btn,int state,int x,int y){
    if(btn==GLUT_LEFT_BUTTON){ cam.dragging=(state==GLUT_DOWN); cam.lastX=x; cam.lastY=y; }
    if(btn==3) cam.dist=fmaxf(3.0f,cam.dist-0.5f);
    if(btn==4) cam.dist=fminf(22.0f,cam.dist+0.5f);
}
static void mouseMove(int x,int y){
    if(!cam.dragging) return;
    float dx=(float)(x-cam.lastX)*0.40f, dy=(float)(y-cam.lastY)*0.40f;
    cam.yaw+=dx; cam.pitch+=dy;
    cam.pitch=fmaxf(-10.0f,fminf(85.0f,cam.pitch));
    cam.lastX=x; cam.lastY=y;
    glutPostRedisplay();
}

// ══════════════════════════════════════════════════════
//  MAIN
// ══════════════════════════════════════════════════════
int main(int argc,char** argv){
    srand((unsigned)time(nullptr));
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH|GLUT_MULTISAMPLE);
    glutInitWindowSize(gW,gH);
    glutCreateWindow("Star Dodge v3.0 — UFO Edition");

    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glClearColor(0.0f,0.0f,0.012f,1.0f);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT,GL_NICEST);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POINT_SMOOTH);

    gQSph  = gluNewQuadric(); gluQuadricNormals(gQSph,  GLU_SMOOTH);
    gQCyl  = gluNewQuadric(); gluQuadricNormals(gQCyl,  GLU_SMOOTH);
    gQDisk = gluNewQuadric(); gluQuadricNormals(gQDisk, GLU_SMOOTH);

    buildStarField(); buildNebula(); buildMoon();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutMouseFunc(mouseBtn);
    glutMotionFunc(mouseMove);
    glutTimerFunc(16,timerCB,0);

    glutMainLoop();
    gluDeleteQuadric(gQSph); gluDeleteQuadric(gQCyl); gluDeleteQuadric(gQDisk);
    return 0;
}