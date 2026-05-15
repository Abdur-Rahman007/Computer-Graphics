#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>

const float PI = 3.1415926535f;

const int SCENE_WIDTH = 1000;
const int SCENE_HEIGHT = 650;

int windowWidth = SCENE_WIDTH;
int windowHeight = SCENE_HEIGHT;

float cloudOffset = -320.0f;
float windmillAngle = 0.0f;
float sunScale = 1.0f;
// Vertical boat: t in [0,1.35], 0=bottom, 1=at bridge, 1.35=past bridge (hidden)
float boatT         = 0.0f;
float boatSpeed     = 0.0015f;  // progress units per frame
bool  boatGoingUp   = true;     // true = heading toward bridge
float waterHighlightOffset = 0.0f;
float bridgeCarX = 608.0f;
int bridgeCarDelayFrames = 0;
bool animationPaused = false;
float eveningBlend = 0.0f;
float hotWeatherBlend = 0.0f;
float rainOffset = 0.0f;
int rainFramesRemaining = 0;
int fireFramesRemaining = 0;
bool eveningRequested = false;
bool hotWeatherRequested = false;

// Night transition
bool nightRequested = false;
bool isNight = false;
float nightBlend = 0.0f;
float sunSetProgress = 0.0f;
float moonRiseProgress = 0.0f;
const float NIGHT_TRANSITION_SPEED = 0.004f;
const float MOON_TARGET_X = 870.0f;
const float MOON_TARGET_Y = 555.0f;

// Badminton (played on football field at night)
// Shuttlecock arcs between two players using a parabolic path
// Field: x 35-360, y 100-235. Net at x=197, y=118-175.
const float BADMINTON_P1_X = 85.0f;   // left player x
const float BADMINTON_P2_X = 310.0f;  // right player x
const float BADMINTON_Y    = 118.0f;  // feet y
float shuttleT       = 0.0f;   // 0=at p1, 1=at p2
float shuttleDir     = 1.0f;   // 1=going right, -1=going left
float shuttleSpeed   = 0.008f;
// arm swing phase for each player
float badmintonArm1  = 0.0f;
float badmintonArm2  = 0.0f;

// ── Automation sequencer ─────────────────────────────────────────────────────
// All timings in frames (≈16ms each, so 60fps)
int autoFrame = 0;         // master frame counter
bool autoJump1Done   = false;
bool autoJump2Done   = false;
bool autoRainDone    = false;
bool autoFireDone    = false;
bool autoNightDone   = false;
int  autoFireCooldown = 0;  // frames until next auto-fire
int  autoFireShots    = 0;  // how many shots fired so far
const int AUTO_FOOTBALL_PASS_INTERVAL = 90;  // pass every ~1.5s
const int AUTO_JUMP1_FRAME   = 300;   // ~5s
const int AUTO_JUMP2_FRAME   = 450;   // ~7.5s (2.5s after jump1)
const int AUTO_RAIN_FRAME    = 700;   // ~11.5s
const int AUTO_FIRE_SHOTS    = 6;     // number of shots before night
const int AUTO_FIRE_INTERVAL = 90;    // ~1.5s between shots
const int AUTO_NIGHT_DELAY   = 120;   // ~2s after last shot before night

const int RAIN_DURATION_FRAMES = 625;
const float EVENING_TRANSITION_SPEED = 0.01f;
const float HOT_WEATHER_TRANSITION_SPEED = 0.018f;

const float BRIDGE_LEFT_X = 392.0f;
const float BRIDGE_RIGHT_X = 608.0f;
const float BRIDGE_ANCHOR_Y = 302.0f;
const float BRIDGE_MIDDLE_Y = 282.0f;
const float RIVER_SURFACE_Y = 95.0f;

enum JumperState
{
    JUMPER_STANDING,
    JUMPER_JUMPING,
    JUMPER_SWIMMING,
    JUMPER_GONE
};

struct BridgeJumper
{
    float x;
    float y;
    float vx;
    float vy;
    float swimPhase;
    int state;
    int shirtColor;
};

BridgeJumper bridgeJumpers[] = {
    {470.0f, 0.0f, 0.0f, 0.0f, 0.0f, JUMPER_STANDING, 0},
    {500.0f, 0.0f, 0.0f, 0.0f, 0.0f, JUMPER_STANDING, 1},
};

const int BRIDGE_JUMPER_COUNT = sizeof(bridgeJumpers) / sizeof(bridgeJumpers[0]);
int nextBridgeJumperIndex = 0;

struct Player
{
    float x;
    float y;
    int team;
};

const Player footballPlayers[] = {
    {70.0f, 145.0f, 0},
    {150.0f, 205.0f, 1},
    {225.0f, 145.0f, 0},
    {120.0f, 140.0f, 1},
    {290.0f, 205.0f, 0},
    {335.0f, 130.0f, 1},
};

const int FOOTBALL_PLAYER_COUNT = sizeof(footballPlayers) / sizeof(footballPlayers[0]);

int ballHolderIndex = 0;
int ballTargetIndex = 0;
float footballX = footballPlayers[0].x + 10.0f;
float footballY = footballPlayers[0].y - 22.0f;
bool footballMoving = false;

void startFootballPass()
{
    if (!footballMoving)
    {
        ballTargetIndex = (ballHolderIndex + 1) % FOOTBALL_PLAYER_COUNT;
        footballMoving = true;
    }
}

// Forward declarations
static float riverBankL(float fy);
static float riverBankR(float fy);

void setColor(float r, float g, float b)
{
    glColor3f(r / 255.0f, g / 255.0f, b / 255.0f);
}

void setColorAlpha(float r, float g, float b, float a)
{
    glColor4f(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

void drawFilledCircle(float cx, float cy, float radius, int segments)
{
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);

    for (int i = 0; i <= segments; ++i)
    {
        const float angle = 2.0f * PI * i / segments;
        glVertex2f(cx + std::cos(angle) * radius, cy + std::sin(angle) * radius);
    }

    glEnd();
}

void drawRectangle(float x1, float y1, float x2, float y2)
{
    glBegin(GL_QUADS);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
}

void drawGradientRectangle(float x1, float y1, float x2, float y2,
                           float bottomR, float bottomG, float bottomB,
                           float topR, float topG, float topB)
{
    glBegin(GL_QUADS);
    setColor(bottomR, bottomG, bottomB);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    setColor(topR, topG, topB);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
}

void drawGradientRectangleAlpha(float x1, float y1, float x2, float y2,
                                float bottomR, float bottomG, float bottomB, float bottomA,
                                float topR, float topG, float topB, float topA)
{
    glBegin(GL_QUADS);
    setColorAlpha(bottomR, bottomG, bottomB, bottomA);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    setColorAlpha(topR, topG, topB, topA);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
}

void drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
    glBegin(GL_TRIANGLES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glVertex2f(x3, y3);
    glEnd();
}

void drawQuad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4)
{
    glBegin(GL_QUADS);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glVertex2f(x3, y3);
    glVertex2f(x4, y4);
    glEnd();
}

void drawLine(float x1, float y1, float x2, float y2, float width)
{
    glLineWidth(width);
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
    glLineWidth(1.0f);
}

float getBridgeDeckY(float x)
{
    float t = (x - BRIDGE_LEFT_X) / (BRIDGE_RIGHT_X - BRIDGE_LEFT_X);
    if (t < 0.0f)
    {
        t = 0.0f;
    }
    if (t > 1.0f)
    {
        t = 1.0f;
    }

    return BRIDGE_ANCHOR_Y + (BRIDGE_MIDDLE_Y - BRIDGE_ANCHOR_Y) * (1.0f - std::fabs(2.0f * t - 1.0f));
}

void resetBridgeJumpers()
{
    bridgeJumpers[0].x = 470.0f;
    bridgeJumpers[1].x = 500.0f;

    for (int i = 0; i < BRIDGE_JUMPER_COUNT; ++i)
    {
        bridgeJumpers[i].y = getBridgeDeckY(bridgeJumpers[i].x) + 6.0f;
        bridgeJumpers[i].vx = 0.0f;
        bridgeJumpers[i].vy = 0.0f;
        bridgeJumpers[i].swimPhase = 0.0f;
        bridgeJumpers[i].state = JUMPER_STANDING;
    }

    nextBridgeJumperIndex = 0;
}

void startNextBridgeJump()
{
    if (nextBridgeJumperIndex >= BRIDGE_JUMPER_COUNT)
    {
        return;
    }

    BridgeJumper &jumper = bridgeJumpers[nextBridgeJumperIndex];
    if (jumper.state == JUMPER_STANDING)
    {
        jumper.vx = 0.35f + nextBridgeJumperIndex * 0.35f;
        jumper.vy = 1.6f;
        jumper.state = JUMPER_JUMPING;
        ++nextBridgeJumperIndex;
    }
}

void updateBridgeJumpers()
{
    for (int i = 0; i < BRIDGE_JUMPER_COUNT; ++i)
    {
        BridgeJumper &jumper = bridgeJumpers[i];

        if (jumper.state == JUMPER_JUMPING)
        {
            jumper.x += jumper.vx;
            jumper.y += jumper.vy;
            jumper.vy -= 0.34f;

            if (jumper.y <= RIVER_SURFACE_Y + 5.0f)
            {
                jumper.y = RIVER_SURFACE_Y + 5.0f;
                jumper.vx = 0.55f + i * 0.15f;
                jumper.vy = 0.0f;
                jumper.state = JUMPER_SWIMMING;
            }
        }
        else if (jumper.state == JUMPER_SWIMMING)
        {
            jumper.swimPhase += 0.18f;

            // Right bank x at current y (piecewise, matching riverBankR logic)
            float targetX;
            const float MID_Y = 95.0f, SRC_Y = 260.0f;
            if (jumper.y >= MID_Y) {
                float f = (jumper.y - MID_Y) / (SRC_Y - MID_Y);
                targetX = 608.0f - f * (608.0f - 540.0f);
            } else {
                float f = jumper.y / MID_Y;
                targetX = 820.0f - f * (820.0f - 608.0f);
            }
            // Subtract a margin so swimmer stays inside the river
            targetX -= 28.0f;

            // Swim diagonally: rightward toward the right bank, downward toward y=0
            const float swimDX = (targetX - jumper.x) * 0.03f;
            const float swimDY = -0.35f;   // always drift downward

            jumper.x += swimDX;
            jumper.y += swimDY;

            // Clamp inside river
            if (jumper.x > targetX) jumper.x = targetX;

            // Disappear when they reach the bottom edge
            if (jumper.y < 4.0f)    jumper.state = JUMPER_GONE;
        }
    }
}

void drawText(float x, float y, const char *text)
{
    glRasterPos2f(x, y);
    for (const char *c = text; *c != '\0'; ++c)
    {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *c);
    }
}

void drawCloud(float x, float y)
{
    setColorAlpha(110, 150, 178, 0.16f);
    drawFilledCircle(x + 10, y - 9, 24, 40);
    drawFilledCircle(x + 40, y - 7, 33, 40);
    drawFilledCircle(x + 70, y - 10, 24, 40);

    setColorAlpha(226, 238, 246, 0.82f);
    drawFilledCircle(x, y - 2, 24, 40);
    drawFilledCircle(x + 28, y + 8, 32, 40);
    drawFilledCircle(x + 62, y - 2, 25, 40);
    drawFilledCircle(x + 32, y - 11, 24, 40);

    setColorAlpha(255, 255, 255, 0.92f);
    drawFilledCircle(x - 5, y + 5, 18, 32);
    drawFilledCircle(x + 24, y + 17, 23, 36);
    drawFilledCircle(x + 55, y + 7, 18, 32);

    setColorAlpha(184, 208, 224, 0.28f);
    drawLine(x - 8, y - 18, x + 64, y - 20, 3.0f);
}

void drawStars()
{
    if (nightBlend <= 0.0f) return;

    // Deterministic star field using prime-based pseudo-random positions
    const int STAR_COUNT = 180;
    // Stars only in the sky region (above land, y in [260, SCENE_HEIGHT])
    for (int i = 0; i < STAR_COUNT; ++i)
    {
        const float x = std::fmod(i * 173.0f + 47.0f, (float)SCENE_WIDTH);
        const float y = 310.0f + std::fmod(i * 97.0f + 31.0f, (float)(SCENE_HEIGHT - 310));
        // Twinkle: vary alpha slightly per star using a different phase
        const float twinkle = 0.7f + 0.3f * std::sin(i * 2.39f + nightBlend * 8.0f);
        const float alpha = nightBlend * twinkle;

        // Larger stars every ~15
        if (i % 15 == 0)
        {
            setColorAlpha(255, 252, 220, alpha);
            drawFilledCircle(x, y, 2.2f, 8);
        }
        else if (i % 5 == 0)
        {
            setColorAlpha(240, 245, 255, alpha);
            drawFilledCircle(x, y, 1.4f, 6);
        }
        else
        {
            setColorAlpha(255, 255, 255, alpha * 0.85f);
            drawFilledCircle(x, y, 0.9f, 5);
        }
    }
}

void drawMoon()
{
    if (moonRiseProgress <= 0.0f) return;

    // Arc: start at left horizon (-80, 260), end at sun position (870, 555)
    // Arc goes up through a high mid-point
    const float startX = -80.0f;
    const float startY = 260.0f;
    const float endX   = MOON_TARGET_X;
    const float endY   = MOON_TARGET_Y;
    const float midX   = (startX + endX) * 0.5f;      // ~395
    const float midY   = 700.0f;                       // high arc peak above scene

    // Quadratic bezier interpolation
    const float t  = moonRiseProgress;
    const float it = 1.0f - t;
    const float mx = it * it * startX + 2.0f * it * t * midX + t * t * endX;
    const float my = it * it * startY + 2.0f * it * t * midY + t * t * endY;

    glPushMatrix();
    glTranslatef(mx, my, 0.0f);

    // Moon body
    setColorAlpha(235, 240, 255, nightBlend);
    drawFilledCircle(0, 0, 40, 64);

    // Crescent shadow to give a crescent look
    setColorAlpha(60, 80, 130, nightBlend);
    drawFilledCircle(14, 6, 34, 64);

    // Subtle surface craters
    setColorAlpha(190, 200, 230, 0.35f * nightBlend);
    drawFilledCircle(-10, 10, 7, 20);
    drawFilledCircle(8, -12, 4, 16);
    drawFilledCircle(-18, -6, 3, 14);

    glPopMatrix();
}

void drawRealisticSky()
{
    drawGradientRectangle(0, 260, SCENE_WIDTH, 395, 188, 224, 238, 143, 206, 235);
    drawGradientRectangle(0, 395, SCENE_WIDTH, 535, 143, 206, 235, 102, 171, 218);
    drawGradientRectangle(0, 535, SCENE_WIDTH, SCENE_HEIGHT, 102, 171, 218, 56, 130, 190);

    drawGradientRectangleAlpha(0, 260, SCENE_WIDTH, 360,
                               255, 218, 159, 0.35f,
                               255, 226, 178, 0.03f);
    drawGradientRectangleAlpha(0, 260, SCENE_WIDTH, SCENE_HEIGHT,
                               255, 255, 255, 0.10f,
                               255, 255, 255, 0.00f);

    if (nightBlend < 1.0f)
    {
        setColorAlpha(255, 235, 189, 0.15f * (1.0f - nightBlend));
        drawFilledCircle(870, 555, 142, 72);
    }

    setColorAlpha(255, 255, 255, 0.10f);
    drawLine(40, 510, 255, 540, 2.0f);
    drawLine(170, 468, 360, 492, 1.5f);
    drawLine(640, 500, 875, 528, 2.0f);

    // Night sky overlay
    if (nightBlend > 0.0f)
    {
        drawGradientRectangleAlpha(0, 260, SCENE_WIDTH, SCENE_HEIGHT,
                                   8, 12, 40, 0.92f * nightBlend,
                                   4, 8, 28, 0.97f * nightBlend);
        // Also darken land
        drawGradientRectangleAlpha(0, 0, SCENE_WIDTH, 260,
                                   5, 10, 30, 0.85f * nightBlend,
                                   8, 14, 38, 0.80f * nightBlend);
    }
}

void drawGrassStroke(float x, float y, float scale, float lean)
{
    drawLine(x, y, x + lean * scale, y + 8.0f * scale, 1.0f);
    drawLine(x + 3.0f * scale, y, x + (3.0f + lean) * scale, y + 6.0f * scale, 1.0f);
    drawLine(x - 3.0f * scale, y, x + (-3.0f + lean * 0.4f) * scale, y + 5.5f * scale, 1.0f);
}

void drawRealisticLand()
{
    drawGradientRectangle(0, 0, SCENE_WIDTH, 260, 58, 132, 70, 116, 179, 87);

    setColorAlpha(180, 214, 113, 0.22f);
    drawQuad(0, 238, 330, 252, 545, 208, 0, 190);
    drawQuad(680, 244, SCENE_WIDTH, 254, SCENE_WIDTH, 184, 610, 205);

    setColorAlpha(43, 111, 62, 0.26f);
    drawQuad(0, 0, 255, 0, 425, 95, 0, 112);
    drawQuad(642, 0, SCENE_WIDTH, 0, SCENE_WIDTH, 136, 548, 95);

    setColorAlpha(119, 167, 77, 0.38f);
    drawQuad(0, 104, 420, 94, 430, 142, 0, 160);
    drawQuad(548, 92, SCENE_WIDTH, 104, SCENE_WIDTH, 158, 560, 140);

    setColorAlpha(82, 140, 73, 0.30f);
    drawLine(0, 258, SCENE_WIDTH, 248, 3.0f);
    drawLine(30, 215, 365, 238, 1.6f);
    drawLine(650, 228, 980, 210, 1.6f);
    drawLine(62, 78, 330, 112, 1.4f);
    drawLine(706, 118, 984, 78, 1.4f);

    setColorAlpha(36, 92, 54, 0.28f);
    for (int i = 0; i < 24; ++i)
    {
        const float x = 18.0f + i * 42.0f;
        const float y = 112.0f + std::fmod(i * 37.0f, 132.0f);
        const float scale = 0.55f + std::fmod(i * 13.0f, 9.0f) * 0.05f;
        const float lean = std::sin(i * 1.7f) * 2.2f;
        drawGrassStroke(x, y, scale, lean);
    }

    setColorAlpha(188, 222, 118, 0.26f);
    for (int i = 0; i < 18; ++i)
    {
        const float x = 36.0f + i * 55.0f;
        const float y = 38.0f + std::fmod(i * 29.0f, 190.0f);
        const float scale = 0.45f + std::fmod(i * 11.0f, 7.0f) * 0.05f;
        const float lean = std::cos(i * 1.1f) * 1.8f;
        drawGrassStroke(x, y, scale, lean);
    }

    setColorAlpha(30, 80, 50, 0.16f);
    drawQuad(35, 100, 360, 100, 370, 236, 28, 238);
    drawQuad(625, 96, 720, 0, 250, 0, 425, 96);
}

void drawEveningLighting()
{
    if (eveningBlend <= 0.0f)
    {
        return;
    }

    drawGradientRectangleAlpha(0, 260, SCENE_WIDTH, SCENE_HEIGHT,
                               255, 148, 87, 0.18f * eveningBlend,
                               24, 50, 105, 0.48f * eveningBlend);

    drawGradientRectangleAlpha(0, 0, SCENE_WIDTH, 260,
                               24, 46, 72, 0.30f * eveningBlend,
                               255, 137, 88, 0.10f * eveningBlend);

    setColorAlpha(255, 187, 101, 0.22f * eveningBlend * (1.0f - nightBlend));
    drawFilledCircle(870, 555, 118, 72);

    setColorAlpha(20, 35, 58, 0.18f * eveningBlend);
    drawRectangle(0, 0, SCENE_WIDTH, SCENE_HEIGHT);
}

void drawHotWeatherLighting()
{
}

void drawRain()
{
    if (rainFramesRemaining <= 0)
    {
        return;
    }

    setColorAlpha(218, 238, 248, 0.58f);
    for (int i = 0; i < 190; ++i)
    {
        const float x = std::fmod(i * 41.0f + rainOffset * 0.18f, SCENE_WIDTH + 32.0f) - 16.0f;
        const float y = std::fmod(i * 29.0f - rainOffset, SCENE_HEIGHT + 28.0f);
        drawLine(x, y, x, y - 12.0f, 1.0f);
    }

    setColorAlpha(238, 249, 255, 0.36f);
    for (int i = 0; i < 90; ++i)
    {
        const float x = std::fmod(i * 67.0f + rainOffset * 0.12f, SCENE_WIDTH + 24.0f) - 12.0f;
        const float y = std::fmod(i * 53.0f - rainOffset * 1.12f, SCENE_HEIGHT + 24.0f);
        drawLine(x, y, x, y - 9.0f, 1.0f);
    }
}

void drawFieldLight()
{
    const float la = nightBlend;

    const float poleX[2] = { 38.0f,  357.0f };
    const float poleBaseY = 100.0f;
    const float poleTopY  = 185.0f;

    for (int p = 0; p < 2; ++p)
    {
        const float px     = poleX[p];
        const float armDir = (p == 0) ? 1.0f : -1.0f;

        // pole — always visible
        setColor(180, 180, 180);
        drawRectangle(px - 2, poleBaseY, px + 2, poleTopY);

        // arm — always visible
        drawRectangle(px, poleTopY - 2, px + armDir * 18, poleTopY + 2);

        // lamp housing — always visible
        setColor(200, 200, 160);
        drawRectangle(px + armDir * 12, poleTopY, px + armDir * 22, poleTopY + 8);

        // lamp glow core — night only
        if (la > 0.0f)
        {
            setColorAlpha(255, 255, 220, la);
            drawFilledCircle(px + armDir * 17, poleTopY + 4, 5, 12);

            const float coneTopX   = px + armDir * 17;
            const float coneTopY   = poleTopY + 4;
            const float coneSpread = 80.0f;

            setColorAlpha(255, 255, 200, 0.07f * la);
            drawTriangle(coneTopX, coneTopY,
                         coneTopX - coneSpread, poleBaseY,
                         coneTopX + coneSpread, poleBaseY);

            setColorAlpha(255, 255, 210, 0.04f * la);
            drawTriangle(coneTopX, coneTopY,
                         coneTopX - coneSpread * 1.5f, poleBaseY,
                         coneTopX + coneSpread * 1.5f, poleBaseY);
        }
    }

    // ground glow and centre strip — night only
    if (la > 0.0f)
    {
        setColorAlpha(255, 255, 200, 0.09f * la);
        drawRectangle(40, 100, 355, 235);

        setColorAlpha(255, 255, 220, 0.06f * la);
        drawRectangle(100, 100, 295, 235);
    }
}

void drawBadmintonScene()
{
    if (nightBlend <= 0.5f) return;

    const float alpha = (nightBlend - 0.5f) * 2.0f; // fade in as night deepens

    // Field bounds: x 35-360, y 100-235. Net at x=197.
    const float netX  = 197.0f;
    const float floorY = BADMINTON_Y;

    // ── net ─────────────────────────────────────────────────────────────────
    setColorAlpha(220, 220, 220, 0.85f * alpha);
    drawLine(netX, floorY, netX, floorY + 55.0f, 2.0f);
    // net mesh lines
    for (int i = 0; i <= 4; ++i)
    {
        const float ny = floorY + i * 11.0f;
        drawLine(netX - 2, ny, netX + 2, ny, 1.0f);
    }
    // net posts
    setColorAlpha(160, 120, 60, alpha);
    drawRectangle(netX - 2, floorY, netX, floorY + 58);
    drawRectangle(netX,     floorY, netX + 2, floorY + 58);

    // ── compute shuttlecock position (parabolic arc) ─────────────────────────
    // t goes 0->1, shuttle goes from server to receiver
    // shuttleT always 0->1. shuttleDir tells which way.
    float sX, sY;
    if (shuttleDir > 0.0f)
    {
        // p1 -> p2
        sX = BADMINTON_P1_X + shuttleT * (BADMINTON_P2_X - BADMINTON_P1_X);
    }
    else
    {
        // p2 -> p1
        sX = BADMINTON_P2_X + shuttleT * (BADMINTON_P1_X - BADMINTON_P2_X);
    }
    // arc peak at midpoint
    const float arcHeight = 38.0f;
    sY = floorY + 30.0f + arcHeight * 4.0f * shuttleT * (1.0f - shuttleT);

    // ── player 1 (left, faces right) ────────────────────────────────────────
    {
        const float px = BADMINTON_P1_X;
        const float py = floorY;

        // legs
        setColorAlpha(50, 50, 140, alpha);
        drawRectangle(px - 5, py,      px - 1, py + 16);
        drawRectangle(px + 1, py,      px + 5, py + 16);

        // body
        setColorAlpha(220, 60, 60, alpha);
        drawRectangle(px - 6, py + 16, px + 6, py + 30);

        // racket arm — swings up when badmintonArm1 is high
        const float arm1Angle = -20.0f + badmintonArm1 * 80.0f; // degrees from body
        const float armRad1   = arm1Angle * PI / 180.0f;
        const float elbowX    = px + 6 + std::cos(armRad1) * 10.0f;
        const float elbowY    = py + 26 + std::sin(armRad1) * 10.0f;
        setColorAlpha(220, 60, 60, alpha);
        drawLine(px + 6, py + 26, elbowX, elbowY, 2.5f);

        // racket (oval head + handle)
        setColorAlpha(180, 140, 40, alpha);
        drawLine(elbowX, elbowY,
                 elbowX + std::cos(armRad1) * 14.0f,
                 elbowY + std::sin(armRad1) * 14.0f, 2.0f);
        setColorAlpha(230, 230, 230, alpha);
        drawFilledCircle(elbowX + std::cos(armRad1) * 19.0f,
                         elbowY + std::sin(armRad1) * 19.0f, 6, 14);

        // left arm (idle)
        setColorAlpha(220, 60, 60, alpha);
        drawLine(px - 6, py + 26, px - 14, py + 22, 2.0f);

        // head
        setColorAlpha(220, 170, 110, alpha);
        drawFilledCircle(px, py + 37, 8, 18);

        // hair
        setColorAlpha(40, 25, 10, alpha);
        drawRectangle(px - 8, py + 40, px + 8, py + 45);
    }

    // ── player 2 (right, faces left) ────────────────────────────────────────
    {
        const float px = BADMINTON_P2_X;
        const float py = floorY;

        // legs
        setColorAlpha(50, 50, 140, alpha);
        drawRectangle(px - 5, py,      px - 1, py + 16);
        drawRectangle(px + 1, py,      px + 5, py + 16);

        // body
        setColorAlpha(60, 180, 80, alpha);
        drawRectangle(px - 6, py + 16, px + 6, py + 30);

        // racket arm — swings up when badmintonArm2 is high, arm points LEFT
        const float arm2Angle = 200.0f - badmintonArm2 * 80.0f;
        const float armRad2   = arm2Angle * PI / 180.0f;
        const float elbowX    = px - 6 + std::cos(armRad2) * 10.0f;
        const float elbowY    = py + 26 + std::sin(armRad2) * 10.0f;
        setColorAlpha(60, 180, 80, alpha);
        drawLine(px - 6, py + 26, elbowX, elbowY, 2.5f);

        // racket
        setColorAlpha(180, 140, 40, alpha);
        drawLine(elbowX, elbowY,
                 elbowX + std::cos(armRad2) * 14.0f,
                 elbowY + std::sin(armRad2) * 14.0f, 2.0f);
        setColorAlpha(230, 230, 230, alpha);
        drawFilledCircle(elbowX + std::cos(armRad2) * 19.0f,
                         elbowY + std::sin(armRad2) * 19.0f, 6, 14);

        // right arm (idle)
        setColorAlpha(60, 180, 80, alpha);
        drawLine(px + 6, py + 26, px + 14, py + 22, 2.0f);

        // head
        setColorAlpha(220, 170, 110, alpha);
        drawFilledCircle(px, py + 37, 8, 18);

        // hair
        setColorAlpha(40, 25, 10, alpha);
        drawRectangle(px - 8, py + 40, px + 8, py + 45);
    }

    // ── shuttlecock ─────────────────────────────────────────────────────────
    // cork base
    setColorAlpha(240, 200, 120, alpha);
    drawFilledCircle(sX, sY, 4, 10);
    // feathers (white fan above cork)
    setColorAlpha(255, 255, 255, 0.9f * alpha);
    drawTriangle(sX - 5, sY + 4, sX + 5, sY + 4, sX,     sY + 14);
    setColorAlpha(230, 230, 255, 0.7f * alpha);
    drawTriangle(sX - 7, sY + 2, sX,     sY + 4, sX - 2, sY + 13);
    drawTriangle(sX + 7, sY + 2, sX,     sY + 4, sX + 2, sY + 13);
}

void drawTargetPracticeScene()
{
    if (hotWeatherBlend <= 0.0f) return;
    if (nightBlend > 0.5f) return;

    // Field: x 35-360, y 100-235. Center x=197, y=167.
    // Board centered at (295, 160), shooter centered at (120, 160).

    // --- Target board (right side of field) ---
    // Two vertical posts
    setColorAlpha(75, 46, 26, hotWeatherBlend);
    drawRectangle(278, 138, 284, 192);
    drawRectangle(308, 138, 314, 192);

    // Board backing
    setColorAlpha(126, 79, 42, hotWeatherBlend);
    drawRectangle(272, 182, 320, 208);

    // Board face
    setColorAlpha(238, 228, 194, hotWeatherBlend);
    drawRectangle(276, 185, 316, 205);

    // Target rings
    setColorAlpha(190, 48, 44, hotWeatherBlend);
    drawFilledCircle(296, 195, 13, 36);
    setColorAlpha(238, 228, 194, hotWeatherBlend);
    drawFilledCircle(296, 195, 9, 32);
    setColorAlpha(190, 48, 44, hotWeatherBlend);
    drawFilledCircle(296, 195, 5, 24);

    // --- Shooter (left side of field) ---
    // Legs
    setColorAlpha(55, 37, 27, hotWeatherBlend);
    drawLine(118, 138, 113, 118, 3.0f);
    drawLine(130, 138, 138, 118, 3.0f);
    // Left arm raised
    setColorAlpha(200, 40, 40, hotWeatherBlend);
    drawLine(116, 158, 102, 148, 3.0f);

    // Body
    setColorAlpha(200, 40, 40, hotWeatherBlend);
    drawRectangle(112, 138, 136, 163);

    // Head
    setColorAlpha(236, 188, 139, hotWeatherBlend);
    drawFilledCircle(124, 172, 9, 24);

    // Hat
    setColorAlpha(42, 31, 24, hotWeatherBlend);
    drawFilledCircle(124, 178, 7, 18);

    // Gun (pointing right toward board)
    setColorAlpha(51, 38, 29, hotWeatherBlend);
    drawLine(134, 155, 162, 158, 4.0f);
    drawRectangle(160, 153, 185, 159);
    drawRectangle(138, 146, 154, 155);
    drawLine(146, 146, 152, 134, 2.0f);

    // Muzzle flash on fire
    if (fireFramesRemaining > 0)
    {
        setColorAlpha(255, 236, 128, hotWeatherBlend);
        drawTriangle(185, 156, 198, 162, 198, 150);

        setColorAlpha(255, 247, 210, hotWeatherBlend);
        drawLine(197, 156, 270, 186, 2.0f);
        drawLine(296, 195, 304, 199, 2.0f);
        drawLine(296, 195, 305, 192, 2.0f);
    }
}

void drawMountain(float left, float base, float right, float peak, float r, float g, float b)
{
    const float center = (left + right) / 2.0f;
    const float width  = right - left;
    const float height = peak - base;
    const int   STEPS  = 40; // segments per slope

    // Build curved silhouette as a filled polygon.
    // Left slope: left->peak, right slope: peak->right.
    // Each slope gets subtle sinusoidal bumps to break the straight edge.

    // --- main body (dark face) ---
    setColor(r * 0.75f, g * 0.88f, b * 0.82f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(center, base); // fan origin at base centre
    // left slope
    for (int i = 0; i <= STEPS; ++i)
    {
        const float t   = (float)i / STEPS;
        const float x   = left + t * (center - left);
        const float y   = base + t * height;
        // bumps: 2 ridges on left slope
        const float bump = std::sin(t * PI) * width * 0.028f
                         * (1.0f + 0.4f * std::sin(t * PI * 3.5f));
        glVertex2f(x - bump, y);
    }
    // right slope
    for (int i = 0; i <= STEPS; ++i)
    {
        const float t   = (float)i / STEPS;
        const float x   = center + t * (right - center);
        const float y   = peak - t * height;
        const float bump = std::sin((1.0f - t) * PI) * width * 0.022f
                         * (1.0f + 0.35f * std::sin((1.0f - t) * PI * 4.0f));
        glVertex2f(x + bump, y);
    }
    glEnd();

    // --- lighter left face highlight ---
    setColor(r * 1.10f, g * 1.12f, b * 1.02f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(center, base);
    for (int i = 0; i <= STEPS; ++i)
    {
        const float t    = (float)i / STEPS;
        const float x    = left + t * (center - left);
        const float y    = base + t * height;
        const float bump = std::sin(t * PI) * width * 0.028f
                         * (1.0f + 0.4f * std::sin(t * PI * 3.5f));
        glVertex2f(x - bump, y);
    }
    // close back to base along inner strip
    glVertex2f(center - width * 0.04f, base);
    glEnd();

    // --- darker right face shadow ---
    setColor(r * 0.55f, g * 0.72f, b * 0.68f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(center + width * 0.10f, base);
    glVertex2f(center, peak);
    for (int i = 0; i <= STEPS; ++i)
    {
        const float t    = (float)i / STEPS;
        const float x    = center + t * (right - center);
        const float y    = peak - t * height;
        const float bump = std::sin((1.0f - t) * PI) * width * 0.022f
                         * (1.0f + 0.35f * std::sin((1.0f - t) * PI * 4.0f));
        glVertex2f(x + bump, y);
    }
    glEnd();

    // --- snow cap ---
    setColorAlpha(239, 247, 225, 0.36f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(center, peak);
    for (int i = 0; i <= 16; ++i)
    {
        const float t  = (float)i / 16;
        const float x  = (center - width * 0.10f) + t * (width * 0.18f);
        const float y  = peak - height * 0.25f + std::sin(t * PI) * height * 0.04f;
        glVertex2f(x, y);
    }
    glEnd();

    // --- subtle green base band ---
    setColorAlpha(48, 78, 69, 0.22f);
    drawQuad(left  + width * 0.12f, base,
             center - width * 0.04f, base + height * 0.24f,
             center + width * 0.15f, base + height * 0.15f,
             right  - width * 0.08f, base);

    // --- ridge / crack lines ---
    setColorAlpha(32, 58, 50, 0.34f);
    drawLine(center, peak - 2.0f, center + width * 0.11f, base + height * 0.46f, 2.0f);
    drawLine(center - width * 0.03f, peak - height * 0.18f,
             center - width * 0.19f, base + height * 0.39f, 1.5f);
    drawLine(center + width * 0.07f, peak - height * 0.32f,
             center + width * 0.29f, base + height * 0.21f, 1.5f);

    // --- horizontal strata lines ---
    setColorAlpha(231, 239, 209, 0.28f);
    for (int i = 0; i < 5; ++i)
    {
        const float t         = 0.18f + i * 0.13f;
        const float y         = base + height * t;
        const float halfWidth = width * 0.5f * (1.0f - t);
        const float sway      = std::sin(t * 17.0f) * width * 0.025f;
        drawLine(center - halfWidth * 0.82f, y + sway,
                 center - halfWidth * 0.20f, y + height * 0.03f + sway, 1.0f);
        drawLine(center + halfWidth * 0.12f, y + height * 0.02f - sway,
                 center + halfWidth * 0.72f, y - sway, 1.0f);
    }

    setColorAlpha(44, 92, 75, 0.32f);
    for (int i = 0; i < 4; ++i)
    {
        const float t         = 0.14f + i * 0.15f;
        const float y         = base + height * t;
        const float halfWidth = width * 0.5f * (1.0f - t);
        drawLine(center - halfWidth * 0.55f, y - height * 0.03f,
                 center - halfWidth * 0.22f, y + height * 0.02f, 1.0f);
        drawLine(center + halfWidth * 0.30f, y,
                 center + halfWidth * 0.58f, y + height * 0.03f, 1.0f);
    }
}

void drawFerrisWheel(float x, float y)
{
    const int   CABINS   = 8;
    const float RADIUS   = 52.0f;   // wheel radius
    const float CABIN_W  = 10.0f;
    const float CABIN_H  = 10.0f;

    glPushMatrix();
    glTranslatef(x, y, 0.0f);

    // ── two support legs ──────────────────────────────────────────────────
    setColor(80, 60, 40);
    drawLine(-RADIUS * 0.55f, -RADIUS, -RADIUS * 0.10f, 0.0f, 4.0f);
    drawLine( RADIUS * 0.55f, -RADIUS,  RADIUS * 0.10f, 0.0f, 4.0f);

    // cross brace
    drawLine(-RADIUS * 0.40f, -RADIUS * 0.55f,
              RADIUS * 0.40f, -RADIUS * 0.55f, 2.5f);

    // ground platform
    setColor(100, 75, 50);
    drawRectangle(-RADIUS * 0.70f, -RADIUS - 6.0f,
                   RADIUS * 0.70f, -RADIUS);

    // ── spokes (rotate with wheel) ────────────────────────────────────────
    glPushMatrix();
    glRotatef(windmillAngle, 0.0f, 0.0f, 1.0f);

    setColor(90, 90, 100);
    for (int i = 0; i < CABINS; ++i)
    {
        const float angle = 2.0f * PI * i / CABINS;
        drawLine(0.0f, 0.0f,
                 std::cos(angle) * RADIUS,
                 std::sin(angle) * RADIUS, 1.8f);
    }

    // ── outer ring ────────────────────────────────────────────────────────
    setColor(70, 70, 85);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i <= 64; ++i)
    {
        const float a = 2.0f * PI * i / 64.0f;
        glVertex2f(std::cos(a) * RADIUS, std::sin(a) * RADIUS);
    }
    glEnd();

    // inner decorative ring
    setColor(100, 100, 120);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i <= 48; ++i)
    {
        const float a = 2.0f * PI * i / 48.0f;
        glVertex2f(std::cos(a) * RADIUS * 0.45f,
                   std::sin(a) * RADIUS * 0.45f);
    }
    glEnd();

    // ── cabins (hang vertically regardless of wheel rotation) ────────────
    float cabinColors[8][3] = {
        {220, 60,  60},  {240, 160, 40},  {50,  180, 80},  {60,  120, 220},
        {180, 50,  180}, {240, 220, 40},  {40,  200, 200},  {220, 100, 50}
    };
    for (int i = 0; i < CABINS; ++i)
    {
        const float spokePhi = 2.0f * PI * i / CABINS;
        const float sx = std::cos(spokePhi) * RADIUS;
        const float sy = std::sin(spokePhi) * RADIUS;

        // cabin hangs from spoke tip — counter-rotate to stay upright
        glPushMatrix();
        glTranslatef(sx, sy, 0.0f);
        glRotatef(-windmillAngle, 0.0f, 0.0f, 1.0f);  // cancel wheel rotation

        // hanger rod
        setColor(100, 100, 100);
        drawLine(0.0f, 0.0f, 0.0f, -6.0f, 1.5f);

        // cabin body
        setColor(cabinColors[i][0], cabinColors[i][1], cabinColors[i][2]);
        drawRectangle(-CABIN_W, -6.0f - CABIN_H, CABIN_W, -6.0f);

        // cabin window
        setColorAlpha(200, 235, 255, 0.85f);
        drawRectangle(-CABIN_W * 0.45f, -6.0f - CABIN_H * 0.75f,
                       CABIN_W * 0.45f, -6.0f - CABIN_H * 0.20f);

        // cabin roof
        setColor(cabinColors[i][0] * 0.7f,
                 cabinColors[i][1] * 0.7f,
                 cabinColors[i][2] * 0.7f);
        drawTriangle(-CABIN_W, -6.0f,
                      CABIN_W, -6.0f,
                      0.0f,    -6.0f + CABIN_W * 0.65f);

        glPopMatrix();
    }

    glPopMatrix();   // end wheel rotation

    // ── centre hub ────────────────────────────────────────────────────────
    setColor(60, 60, 70);
    drawFilledCircle(0.0f, 0.0f, 7.0f, 20);
    setColor(140, 140, 160);
    drawFilledCircle(0.0f, 0.0f, 4.0f, 16);

    // night lights on rim
    if (nightBlend > 0.0f)
    {
        for (int i = 0; i < CABINS * 2; ++i)
        {
            const float a  = 2.0f * PI * i / (CABINS * 2) + windmillAngle * PI / 180.0f;
            const float lx2 = std::cos(a) * RADIUS;
            const float ly2 = std::sin(a) * RADIUS;
            setColorAlpha(255, 230, 80, 0.85f * nightBlend);
            drawFilledCircle(lx2, ly2, 2.5f, 8);
        }
    }

    glPopMatrix();
}

void drawBoat()
{
    // boatT=0   → bottom of screen (y~10)
    // boatT=1.0 → just under bridge (y~252)
    // boatT=1.35→ fully past bridge (hidden above)
    // Bridge deck is at y≈285. Boat is hidden when its centre y > 278.
    const float yBot  = 10.0f;
    const float yTop  = 252.0f;
    // clamp t to [0,1] for position — beyond 1.0 the boat just keeps moving upward
    const float tPos  = boatT < 1.0f ? boatT : 1.0f + (boatT - 1.0f);
    const float boatY = yBot + tPos * (yTop - yBot);

    // Hide completely when above the bridge deck
    if (boatY > 278.0f)
        return;

    // Position on left third of the river (clamp bank lookup to valid range)
    const float lookupY = boatY < 260.0f ? boatY : 260.0f;
    const float lx = riverBankL(lookupY);
    const float rx = riverBankR(lookupY);
    const float boatCX = lx + (rx - lx) * 0.22f;

    // Perspective: full size at bottom, ~50% near bridge
    const float tScale = boatT < 1.0f ? boatT : 1.0f;
    const float sc = 1.0f - tScale * 0.50f;

    // Gentle rocking
    const float rockAngle = std::sin(boatT * 80.0f) * 1.5f;

    glPushMatrix();
    glTranslatef(boatCX, boatY, 0.0f);
    glScalef(sc, sc, 1.0f);
    glRotatef(rockAngle, 0.0f, 0.0f, 1.0f);

    // Flip horizontally when returning so bow faces downward direction
    if (!boatGoingUp)
        glScalef(-1.0f, 1.0f, 1.0f);

    // ── V-SHAPED WAKE behind stern ───────────────────────────────────────────
    setColorAlpha(200, 238, 252, 0.45f);
    drawLine(  0, -30, -48, -64, 1.8f);
    drawLine(  0, -30,  48, -64, 1.8f);
    setColorAlpha(220, 248, 255, 0.25f);
    drawLine(  0, -30, -66, -76, 1.2f);
    drawLine(  0, -30,  66, -76, 1.2f);
    setColorAlpha(240, 252, 255, 0.35f);
    drawLine(  0, -30,   0, -68, 1.3f);
    drawLine(-10, -40, -10, -62, 0.9f);
    drawLine( 10, -40,  10, -62, 0.9f);

    // ── SUBMERGED KEEL (dark strip below waterline) ──────────────────────────
    setColor(28, 16, 8);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(0, -42);           // keel tip (deeper)
    glVertex2f(-14, -28);
    glVertex2f(-20, -24);
    glVertex2f( 20, -24);
    glVertex2f( 14, -28);
    glVertex2f(  0, -42);
    glEnd();

    // ── CURVED HULL BODY (rear view — rounded elliptical stern) ─────────────
    // Main hull colour — dark navy
    setColor(42, 62, 105);

    // Stern face — bottom is now wider and sits lower
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(0, -2);   // centre of stern face
    // bottom wide edge (pushed down and out)
    glVertex2f(-36, -26);
    glVertex2f(-40, -16);
    // port side curving up
    glVertex2f(-38,  -4);
    glVertex2f(-34,   6);
    glVertex2f(-28,  14);
    // rounded top
    glVertex2f(-18,  20);
    glVertex2f( -8,  24);
    glVertex2f(  0,  25);
    glVertex2f(  8,  24);
    glVertex2f( 18,  20);
    // starboard side curving down
    glVertex2f( 28,  14);
    glVertex2f( 34,   6);
    glVertex2f( 38,  -4);
    glVertex2f( 40, -16);
    glVertex2f( 36, -26);
    // close bottom
    glVertex2f(-36, -26);
    glEnd();

    // Hull side shadow (left/port side darker)
    setColorAlpha(20, 35, 70, 0.45f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(-24, 2);
    glVertex2f(-36, -26);
    glVertex2f(-40, -16);
    glVertex2f(-38,  -4);
    glVertex2f(-34,   6);
    glVertex2f(-28,  14);
    glVertex2f(-18,  20);
    glVertex2f(-10,  22);
    glVertex2f(-10,  -2);
    glEnd();

    // Hull highlight (starboard lighter sheen)
    setColorAlpha(80, 110, 170, 0.30f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(16, 4);
    glVertex2f(14, -26);
    glVertex2f(36, -26);
    glVertex2f(40, -16);
    glVertex2f(38,  -4);
    glVertex2f(34,   6);
    glVertex2f(28,  14);
    glVertex2f(18,  20);
    glVertex2f(10,  22);
    glEnd();

    // ── HORIZONTAL PLANKING LINES on stern face ──────────────────────────────
    setColorAlpha(25, 42, 80, 0.55f);
    // extra plank lines to cover the taller bottom section
    float plankY[] = { -22.0f, -16.0f, -10.0f, -4.0f, 2.0f, 8.0f, 14.0f, 19.0f };
    float plankW[] = {  28.0f,  36.0f,  38.0f,  36.0f, 32.0f, 28.0f, 20.0f, 12.0f };
    for (int i = 0; i < 8; ++i)
        drawLine(-plankW[i], plankY[i], plankW[i], plankY[i], 1.0f);

    // ── WATERLINE STRIPE ─────────────────────────────────────────────────────
    setColor(200, 180, 60);
    glBegin(GL_QUADS);
    glVertex2f(-38, -28);
    glVertex2f( 38, -28);
    glVertex2f( 36, -22);
    glVertex2f(-36, -22);
    glEnd();

    // ── TRANSOM NAME PLATE ────────────────────────────────────────────────────
    setColor(180, 155, 80);
    drawRectangle(-16, -2, 16, 6);
    setColor(38, 22, 10);
    drawText(-14, 0, "NODI");

    // ── GUNWALE / TOP RAIL ────────────────────────────────────────────────────
    setColor(160, 128, 72);
    glLineWidth(2.8f);
    glBegin(GL_LINE_STRIP);
    glVertex2f(-36, -26);
    glVertex2f(-40, -16);
    glVertex2f(-38,  -4);
    glVertex2f(-34,   6);
    glVertex2f(-28,  14);
    glVertex2f(-18,  20);
    glVertex2f( -8,  24);
    glVertex2f(  0,  25);
    glVertex2f(  8,  24);
    glVertex2f( 18,  20);
    glVertex2f( 28,  14);
    glVertex2f( 34,   6);
    glVertex2f( 38,  -4);
    glVertex2f( 40, -16);
    glVertex2f( 36, -26);
    glEnd();
    glLineWidth(1.0f);

    // ── DECK (visible top surface behind gunwale) ─────────────────────────────
    setColor(130, 95, 52);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(0, 30);
    glVertex2f(-18, 20);
    glVertex2f( -8, 24);
    glVertex2f(  0, 25);
    glVertex2f(  8, 24);
    glVertex2f( 18, 20);
    glEnd();

    // Deck plank lines
    setColorAlpha(100, 70, 36, 0.60f);
    drawLine(-16, 21,  16, 21, 1.0f);
    drawLine(-10, 23,  10, 23, 1.0f);

    // ── SMALL CABIN / WHEELHOUSE on deck ─────────────────────────────────────
    setColor(155, 115, 62);
    drawRectangle(-10, 24, 10, 38);
    // cabin roof (slight pitch)
    setColor(120, 85, 42);
    drawTriangle(-12, 38,  12, 38,  0, 44);
    // cabin windows
    setColor(160, 210, 230);
    drawRectangle(-7, 28, -2, 35);
    drawRectangle( 2, 28,  7, 35);
    // window frames
    setColor(100, 70, 35);
    drawLine(-7, 31, -2, 31, 1.0f);
    drawLine(-5, 28, -5, 35, 1.0f);
    drawLine( 2, 31,  7, 31, 1.0f);
    drawLine( 4, 28,  4, 35, 1.0f);
    // cabin door
    setColor(90, 60, 28);
    drawRectangle(-2, 24, 2, 32);

    // ── RAILING POSTS on deck ─────────────────────────────────────────────────
    setColor(140, 105, 58);
    // port side posts
    drawLine(-18, 20, -18, 28, 1.5f);
    drawLine(-13, 22, -13, 28, 1.5f);
    // starboard side posts
    drawLine( 18, 20,  18, 28, 1.5f);
    drawLine( 13, 22,  13, 28, 1.5f);
    // rail wire
    setColorAlpha(160, 120, 65, 0.80f);
    drawLine(-18, 26, -10, 26, 1.2f);
    drawLine( 18, 26,  10, 26, 1.2f);

    // ── MAST ─────────────────────────────────────────────────────────────────
    setColor(100, 72, 40);
    drawLine(0, 38, 0, 105, 3.5f);
    // mast collar at base
    setColor(120, 88, 48);
    drawRectangle(-3, 36, 3, 42);

    // ── BOOM ─────────────────────────────────────────────────────────────────
    setColor(95, 68, 38);
    drawLine(-2, 48, 38, 54, 2.2f);

    // ── MAINSAIL ─────────────────────────────────────────────────────────────
    // Luff on mast, leech curves out to the right, foot along boom
    setColor(238, 232, 210);
    drawQuad(2, 48,   2, 98,   40, 72,  38, 54);

    // Sail billow highlight (brighter centre)
    setColorAlpha(255, 252, 235, 0.40f);
    drawQuad(4, 56,   4, 90,   28, 72,  24, 58);

    // Sail shadow near mast
    setColorAlpha(155, 145, 115, 0.30f);
    drawQuad(2, 48,   2, 98,   12, 94,  12, 52);

    // Horizontal batten/seam lines
    setColorAlpha(195, 185, 160, 0.65f);
    drawLine(3, 58, 32, 60, 1.0f);
    drawLine(3, 68, 30, 70, 1.0f);
    drawLine(3, 78, 26, 80, 1.0f);
    drawLine(3, 88, 18, 89, 1.0f);

    // ── TOPSAIL ───────────────────────────────────────────────────────────────
    setColor(228, 220, 196);
    drawTriangle(1, 94,   1, 108,  22, 100);
    setColorAlpha(185, 175, 150, 0.55f);
    drawLine(1, 100, 16, 101, 1.0f);

    // ── RIGGING ───────────────────────────────────────────────────────────────
    setColorAlpha(75, 58, 32, 0.75f);
    drawLine(0, 105,  28, 40, 1.0f);   // starboard shroud
    drawLine(0, 105, -24, 40, 1.0f);   // port shroud
    drawLine(0,  75,  28, 40, 1.0f);   // lower shroud
    drawLine(0, 105,  38, 54, 1.0f);   // backstay to boom end

    // ── FLAG at masthead ──────────────────────────────────────────────────────
    setColor(200, 45, 35);
    drawTriangle(0, 108,  18, 103,  0, 98);
    setColorAlpha(225, 80, 60, 0.45f);
    drawTriangle(0, 108,  12, 104,  0, 101);

    // ── RUDDER (below keel centre) ────────────────────────────────────────────
    setColor(50, 30, 12);
    drawQuad(-4, -20,  4, -20,  3, -32, -3, -32);
    // rudder stock
    setColor(70, 45, 20);
    drawLine(0, -18, 0, -32, 1.5f);

    glPopMatrix();
}

void drawSun()
{
    // During night transition, sun moves right and down below horizon
    float sunX = 870.0f;
    float sunY = 555.0f;
    if (sunSetProgress > 0.0f)
    {
        sunX = 870.0f + sunSetProgress * 180.0f;   // slides right
        sunY = 555.0f - sunSetProgress * 620.0f;   // drops below horizon
    }
    // Fully set: don't draw
    if (sunSetProgress >= 1.0f) return;

    const float sunAlpha = 1.0f - sunSetProgress;

    glPushMatrix();
    glTranslatef(sunX, sunY, 0.0f);
    glScalef(sunScale, sunScale, 1.0f);

    setColorAlpha(255, 190, 72, 0.18f * sunAlpha);
    drawFilledCircle(0, 0, 78, 64);

    setColorAlpha(255, 211, 92, 0.25f * sunAlpha);
    drawFilledCircle(0, 0, 62, 64);

    setColorAlpha(232, 122, 49, sunAlpha);
    drawFilledCircle(0, 0, 47, 64);

    setColorAlpha(246, 169, 58, sunAlpha);
    drawFilledCircle(-4, 4, 41, 64);

    setColorAlpha(255, 216, 105, sunAlpha);
    drawFilledCircle(-10, 12, 28, 48);

    setColorAlpha(255, 244, 184, 0.78f * sunAlpha);
    drawFilledCircle(-18, 22, 11, 32);

    setColorAlpha(247, 142, 49, 0.36f * sunAlpha);
    drawFilledCircle(14, -16, 21, 48);

    setColorAlpha(255, 196, 74, 0.72f * sunAlpha);
    for (int i = 0; i < 16; ++i)
    {
        const float angle = 2.0f * PI * i / 16.0f;
        const float x1 = std::cos(angle) * 55.0f;
        const float y1 = std::sin(angle) * 55.0f;
        const float x2 = std::cos(angle) * ((i % 2 == 0) ? 88.0f : 76.0f);
        const float y2 = std::sin(angle) * ((i % 2 == 0) ? 88.0f : 76.0f);
        drawLine(x1, y1, x2, y2, (i % 2 == 0) ? 3.0f : 2.0f);
    }

    glPopMatrix();
}

static float riverBankL(float fy)
{
    const float SRC_Y = 260.0f, MID_Y = 95.0f;
    float bx;
    if (fy >= MID_Y) {
        float f = (fy - MID_Y) / (SRC_Y - MID_Y);
        bx = 392.0f + f * (460.0f - 392.0f);
    } else {
        float f = fy / MID_Y;
        bx = 180.0f + f * (392.0f - 180.0f);
    }
    return bx + std::sin(fy * 0.055f + 0.3f) * (3.5f + (1.0f - fy / SRC_Y) * 6.0f);
}

static float riverBankR(float fy)
{
    const float SRC_Y = 260.0f, MID_Y = 95.0f;
    float bx;
    if (fy >= MID_Y) {
        float f = (fy - MID_Y) / (SRC_Y - MID_Y);
        bx = 608.0f - f * (608.0f - 540.0f);
    } else {
        float f = fy / MID_Y;
        bx = 820.0f - f * (820.0f - 608.0f);
    }
    return bx - std::sin(fy * 0.055f + 1.4f) * (3.5f + (1.0f - fy / SRC_Y) * 6.0f);
}

void drawWaterSystem()
{
    const float t = waterHighlightOffset;

    const int   STEPS = 80;
    const float SRC_Y = 260.0f;
    const float BOT_Y = 0.0f;

    // ── Layer 1: deep base fill ───────────────────────────────────────────
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= STEPS; ++i)
    {
        const float fy   = BOT_Y + (SRC_Y - BOT_Y) * i / STEPS;
        const float frac = fy / SRC_Y;
        const float lx   = riverBankL(fy);
        const float rx   = riverBankR(fy);

        const float dr = 18.0f + frac * 28.0f;
        const float dg = 62.0f + frac * 52.0f;
        const float db = 112.0f + frac * 58.0f;
        const float nr = dr * (1.0f - nightBlend * 0.55f);
        const float ng = dg * (1.0f - nightBlend * 0.40f);
        const float nb = db * (1.0f - nightBlend * 0.15f);
        setColor(nr, ng, nb);
        glVertex2f(lx, fy);
        glVertex2f(rx, fy);
    }
    glEnd();

    // ── Layer 2: mid-depth teal subsurface band ───────────────────────────
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= STEPS; ++i)
    {
        const float fy    = BOT_Y + (SRC_Y - BOT_Y) * i / STEPS;
        const float lx    = riverBankL(fy);
        const float rx    = riverBankR(fy);
        const float w2    = (rx - lx) * 0.38f;
        const float cx    = (lx + rx) * 0.5f + std::sin(fy * 0.04f + t * 0.012f) * 6.0f;
        const float na    = 0.32f * (1.0f - nightBlend * 0.5f);
        setColorAlpha(48, 148, 188, na);
        glVertex2f(cx - w2, fy);
        glVertex2f(cx + w2, fy);
    }
    glEnd();

    // ── Layer 3: sky/sun reflection streak ────────────────────────────────
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= STEPS; ++i)
    {
        const float fy    = BOT_Y + (SRC_Y - BOT_Y) * i / STEPS;
        const float lx    = riverBankL(fy);
        const float rx    = riverBankR(fy);
        const float riverW = rx - lx;
        const float sway  = std::sin(fy * 0.032f + t * 0.008f) * (4.0f + riverW * 0.03f);
        const float halfW = 10.0f + riverW * 0.06f + std::sin(fy * 0.11f) * 3.0f;
        const float cx    = (lx + rx) * 0.5f + sway;
        const float rr = 210.0f * (1.0f - nightBlend) + 180.0f * nightBlend;
        const float rg = 242.0f * (1.0f - nightBlend) + 200.0f * nightBlend;
        const float rb = 250.0f * (1.0f - nightBlend) + 240.0f * nightBlend;
        setColorAlpha(rr, rg, rb, 0.22f);
        glVertex2f(cx - halfW, fy);
        glVertex2f(cx + halfW, fy);
    }
    glEnd();

    // ── Layer 4: animated surface ripple lines ────────────────────────────
    // Speed is faster (current stronger) at the narrow mountain source
    for (int i = 0; i < 28; ++i)
    {
        const float baseY = BOT_Y + std::fmod(t * 1.1f + i * 10.5f, SRC_Y - 4.0f);
        const float lx    = riverBankL(baseY);
        const float rx    = riverBankR(baseY);
        const float riverW = rx - lx;
        const float halfW  = riverW * 0.34f;
        const float cx     = (lx + rx) * 0.5f + std::sin(baseY * 0.04f + t * 0.01f) * riverW * 0.06f;
        const float alpha  = 0.22f + 0.14f * std::sin(i * 1.7f);
        // narrower river = choppier ripples
        const float chopFreq = 0.08f + (1.0f - baseY / SRC_Y) * 0.10f;

        glLineWidth(1.2f);
        glBegin(GL_LINE_STRIP);
        setColorAlpha(195, 238, 252, alpha * (1.0f - nightBlend * 0.3f));
        for (int s = 0; s <= 14; ++s)
        {
            const float sx = (cx - halfW) + s * halfW * 2.0f / 14.0f;
            const float sy = baseY + std::sin(sx * chopFreq + t * 0.04f) * (1.8f + riverW * 0.008f);
            glVertex2f(sx, sy);
        }
        glEnd();
        glLineWidth(1.0f);
    }

    // ── Layer 5: caustic shimmer ──────────────────────────────────────────
    for (int i = 0; i < 30; ++i)
    {
        const float baseY = BOT_Y + std::fmod(i * 31.0f, SRC_Y - 4.0f);
        const float lx    = riverBankL(baseY) + 6.0f;
        const float rx    = riverBankR(baseY) - 6.0f;
        const float cx    = lx + std::fmod(i * 37.0f, rx - lx);
        const float len   = 4.0f + std::fmod(i * 7.0f, 8.0f);
        const float angle = std::fmod(i * 47.0f + t * 0.5f, 60.0f) - 30.0f;
        const float rad   = angle * PI / 180.0f;
        const float alpha = 0.16f + 0.12f * std::sin(i * 1.3f + t * 0.07f);
        setColorAlpha(240, 252, 255, alpha * (1.0f - nightBlend * 0.6f));
        glLineWidth(1.0f);
        glBegin(GL_LINES);
        glVertex2f(cx - std::cos(rad) * len, baseY - std::sin(rad) * len);
        glVertex2f(cx + std::cos(rad) * len, baseY + std::sin(rad) * len);
        glEnd();
    }

    // ── Layer 6: bank shadow gradients ────────────────────────────────────
    // left bank
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= STEPS; ++i)
    {
        const float fy = BOT_Y + (SRC_Y - BOT_Y) * i / STEPS;
        const float lx = riverBankL(fy);
        setColorAlpha(16, 48, 88, 0.40f);
        glVertex2f(lx - 1.0f, fy);
        setColorAlpha(16, 48, 88, 0.0f);
        glVertex2f(lx + 16.0f + (riverBankR(fy) - lx) * 0.06f, fy);
    }
    glEnd();
    // right bank
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= STEPS; ++i)
    {
        const float fy = BOT_Y + (SRC_Y - BOT_Y) * i / STEPS;
        const float rx = riverBankR(fy);
        setColorAlpha(16, 48, 88, 0.0f);
        glVertex2f(rx - 16.0f - (rx - riverBankL(fy)) * 0.06f, fy);
        setColorAlpha(16, 48, 88, 0.40f);
        glVertex2f(rx + 1.0f, fy);
    }
    glEnd();

    // ── Layer 7: foam edge line along both banks ──────────────────────────
    glLineWidth(1.5f);
    glBegin(GL_LINE_STRIP);
    setColorAlpha(200, 238, 252, 0.55f);
    for (int i = 0; i <= STEPS; ++i)
    {
        const float fy = BOT_Y + (SRC_Y - BOT_Y) * i / STEPS;
        const float lx = riverBankL(fy) + std::sin(fy * 0.22f + t * 0.05f) * 1.8f;
        glVertex2f(lx, fy);
    }
    glEnd();
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= STEPS; ++i)
    {
        const float fy = BOT_Y + (SRC_Y - BOT_Y) * i / STEPS;
        const float rx = riverBankR(fy) + std::sin(fy * 0.19f + t * 0.05f + 1.0f) * 1.8f;
        glVertex2f(rx, fy);
    }
    glEnd();
    glLineWidth(1.0f);

    // ── Layer 8: mountain-source waterfall/rapids at y≈260 ───────────────
    // Narrow churning white water where river exits the mountains
    {
        const float srcCX = 500.0f;
        const float srcHW = 42.0f;
        for (int i = 0; i < 8; ++i)
        {
            const float fy    = SRC_Y - 4.0f - std::fmod(t * 2.2f + i * 10.0f, 32.0f);
            const float sway  = std::sin(fy * 0.18f + t * 0.08f) * 6.0f;
            const float alpha = 0.45f + 0.3f * std::sin(i * 1.9f + t * 0.1f);
            setColorAlpha(220, 245, 255, alpha);
            glLineWidth(2.2f);
            glBegin(GL_LINES);
            glVertex2f(srcCX - srcHW * 0.5f + sway, fy);
            glVertex2f(srcCX + srcHW * 0.5f + sway, fy + 5.0f);
            glEnd();
        }
        glLineWidth(1.0f);
        // spray mist
        setColorAlpha(230, 248, 255, 0.18f);
        drawFilledCircle(srcCX, SRC_Y - 6.0f, 36.0f, 24);
        setColorAlpha(240, 252, 255, 0.10f);
        drawFilledCircle(srcCX - 18.0f, SRC_Y - 2.0f, 22.0f, 18);
        drawFilledCircle(srcCX + 20.0f, SRC_Y - 4.0f, 20.0f, 18);
    }

    // ── Layer 9: night moonlight shimmer + dark tint ──────────────────────
    if (nightBlend > 0.0f)
    {
        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= STEPS; ++i)
        {
            const float fy    = BOT_Y + (SRC_Y - BOT_Y) * i / STEPS;
            const float lx    = riverBankL(fy);
            const float rx    = riverBankR(fy);
            const float sway  = std::sin(fy * 0.05f + t * 0.015f) * 7.0f;
            const float halfW = 8.0f + (rx - lx) * 0.05f + std::sin(fy * 0.12f) * 3.0f;
            const float cx    = (lx + rx) * 0.5f + sway;
            setColorAlpha(200, 215, 240, 0.18f * nightBlend);
            glVertex2f(cx - halfW, fy);
            glVertex2f(cx + halfW, fy);
        }
        glEnd();

        // dark blue night tint
        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= STEPS; ++i)
        {
            const float fy = BOT_Y + (SRC_Y - BOT_Y) * i / STEPS;
            setColorAlpha(8, 18, 48, 0.30f * nightBlend);
            glVertex2f(riverBankL(fy), fy);
            glVertex2f(riverBankR(fy), fy);
        }
        glEnd();
    }
}


void drawBridge()
{
    setColor(96, 57, 32);
    drawRectangle(378, 290, 410, 314);
    drawRectangle(590, 290, 622, 314);

    setColor(70, 42, 25);
    drawLine(BRIDGE_LEFT_X, BRIDGE_ANCHOR_Y, 500.0f, BRIDGE_MIDDLE_Y + 8.0f, 4.0f);
    drawLine(500.0f, BRIDGE_MIDDLE_Y + 8.0f, BRIDGE_RIGHT_X, BRIDGE_ANCHOR_Y, 4.0f);
    drawLine(BRIDGE_LEFT_X, BRIDGE_ANCHOR_Y + 18.0f, 500.0f, BRIDGE_MIDDLE_Y + 26.0f, 3.0f);
    drawLine(500.0f, BRIDGE_MIDDLE_Y + 26.0f, BRIDGE_RIGHT_X, BRIDGE_ANCHOR_Y + 18.0f, 3.0f);

    for (int i = 0; i <= 8; ++i)
    {
        const float t = i / 8.0f;
        const float x = BRIDGE_LEFT_X + (BRIDGE_RIGHT_X - BRIDGE_LEFT_X) * t;
        const float deckY = getBridgeDeckY(x);
        const float cableY = BRIDGE_ANCHOR_Y + 18.0f + ((BRIDGE_MIDDLE_Y + 26.0f) - (BRIDGE_ANCHOR_Y + 18.0f)) * (1.0f - std::fabs(2.0f * t - 1.0f));
        drawLine(x, deckY + 2.0f, x, cableY, 2.0f);
    }

    setColor(126, 78, 43);
    for (int i = 0; i < 9; ++i)
    {
        const float t1 = i / 9.0f;
        const float t2 = (i + 1) / 9.0f;
        const float x1 = BRIDGE_LEFT_X + (BRIDGE_RIGHT_X - BRIDGE_LEFT_X) * t1;
        const float x2 = BRIDGE_LEFT_X + (BRIDGE_RIGHT_X - BRIDGE_LEFT_X) * t2;
        const float y1 = getBridgeDeckY(x1);
        const float y2 = getBridgeDeckY(x2);
        drawLine(x1, y1, x2, y2, 7.0f);
    }
}

void drawBridgeCar()
{
    if (bridgeCarDelayFrames > 0)
    {
        return;
    }

    const float carY = getBridgeDeckY(bridgeCarX) + 11.0f;

    glPushMatrix();
    glTranslatef(bridgeCarX, carY, 0.0f);

    setColor(205, 63, 48);
    drawRectangle(-18, 0, 18, 13);

    setColor(236, 151, 58);
    drawRectangle(-7, 13, 10, 24);

    setColor(139, 202, 224);
    drawRectangle(-4, 15, 7, 22);

    setColor(28, 28, 28);
    drawFilledCircle(-11, -1, 5, 20);
    drawFilledCircle(12, -1, 5, 20);

    setColor(230, 230, 230);
    drawFilledCircle(-11, -1, 2, 14);
    drawFilledCircle(12, -1, 2, 14);

    // headlights at night (car travels left, so lights on left side)
    if (nightBlend > 0.0f)
    {
        const float la = nightBlend;
        // headlight lenses
        setColorAlpha(255, 248, 200, la);
        drawRectangle(-20, 4, -17, 10);
        // wide cone beam fanning left
        setColorAlpha(255, 248, 180, 0.18f * la);
        drawTriangle(-17, 7, -17 - 80, 7 + 28, -17 - 80, 7 - 28);
        setColorAlpha(255, 252, 210, 0.10f * la);
        drawTriangle(-17, 7, -17 - 120, 7 + 46, -17 - 120, 7 - 46);
    }

    glPopMatrix();
}

void setJumperShirtColor(int shirtColor)
{
    if (shirtColor == 0)
    {
        setColor(244, 185, 52);
    }
    else
    {
        setColor(92, 178, 231);
    }
}

void drawStandingBridgeJumper(const BridgeJumper &jumper)
{
    glPushMatrix();
    glTranslatef(jumper.x, jumper.y, 0.0f);

    setColor(75, 48, 34);
    drawLine(-4, 0, -8, 17, 2.0f);
    drawLine(4, 0, 8, 17, 2.0f);
    drawLine(-6, 31, -15, 21, 2.0f);
    drawLine(6, 31, 15, 21, 2.0f);

    setJumperShirtColor(jumper.shirtColor);
    drawRectangle(-8, 17, 8, 38);

    setColor(232, 174, 126);
    drawFilledCircle(0, 49, 8, 22);

    setColor(35, 26, 21);
    drawFilledCircle(0, 55, 5, 16);

    glPopMatrix();
}

void drawJumpingBridgeJumper(const BridgeJumper &jumper)
{
    glPushMatrix();
    glTranslatef(jumper.x, jumper.y, 0.0f);
    glRotatef(-18.0f, 0.0f, 0.0f, 1.0f);

    setColor(75, 48, 34);
    drawLine(-5, 0, -15, 10, 2.0f);
    drawLine(5, 0, 17, 6, 2.0f);
    drawLine(-7, 30, -22, 35, 2.0f);
    drawLine(7, 30, 22, 27, 2.0f);

    setJumperShirtColor(jumper.shirtColor);
    drawRectangle(-8, 9, 8, 32);

    setColor(232, 174, 126);
    drawFilledCircle(0, 43, 8, 22);

    setColor(35, 26, 21);
    drawFilledCircle(0, 49, 5, 16);

    glPopMatrix();
}

void drawSwimmingBridgeJumper(const BridgeJumper &jumper)
{
    const float wave = std::sin(jumper.swimPhase) * 4.0f;

    glPushMatrix();
    glTranslatef(jumper.x, jumper.y + wave, 0.0f);

    setJumperShirtColor(jumper.shirtColor);
    drawLine(-14, 2, 16, 2, 5.0f);

    setColor(232, 174, 126);
    drawFilledCircle(22, 5, 7, 20);

    setColor(75, 48, 34);
    drawLine(-2, 2, -16, 12, 2.0f);
    drawLine(7, 2, 20, -6, 2.0f);

    setColor(35, 26, 21);
    drawFilledCircle(25, 9, 4, 14);

    setColor(206, 241, 249);
    drawLine(-24, -3, -4, -7, 2.0f);
    drawLine(12, -5, 34, -2, 2.0f);

    glPopMatrix();
}

void drawBridgeJumpers()
{
    for (int i = 0; i < BRIDGE_JUMPER_COUNT; ++i)
    {
        if (bridgeJumpers[i].state == JUMPER_GONE)
        {
            continue;
        }
        else if (bridgeJumpers[i].state == JUMPER_SWIMMING)
        {
            drawSwimmingBridgeJumper(bridgeJumpers[i]);
        }
        else if (bridgeJumpers[i].state == JUMPER_JUMPING)
        {
            drawJumpingBridgeJumper(bridgeJumpers[i]);
        }
        else
        {
            drawStandingBridgeJumper(bridgeJumpers[i]);
        }
    }
}

void drawFootballPlayer(const Player &player)
{
    glPushMatrix();
    glTranslatef(player.x, player.y, 0.0f);

    setColor(90, 55, 35);
    drawLine(-7, -16, -15, -35, 3.0f);
    drawLine(7, -16, 15, -35, 3.0f);
    drawLine(-12, 7, -24, -6, 3.0f);
    drawLine(12, 7, 24, -6, 3.0f);

    if (player.team == 0)
    {
        setColor(35, 94, 204);
    }
    else
    {
        setColor(220, 55, 48);
    }

    drawRectangle(-13, -16, 13, 15);

    setColor(238, 188, 139);
    drawFilledCircle(0, 28, 10, 24);

    setColor(42, 32, 25);
    drawFilledCircle(0, 35, 7, 18);

    glPopMatrix();
}

void drawFootball()
{
    setColor(245, 245, 245);
    drawFilledCircle(footballX, footballY, 8, 28);

    setColor(25, 25, 25);
    drawLine(footballX - 5, footballY, footballX + 5, footballY, 2.0f);
    drawLine(footballX, footballY - 5, footballX, footballY + 5, 2.0f);
}

void drawFootballGoalPosts()
{
    setColor(200, 218, 205);
    drawLine(16, 133, 16, 183, 3.0f);
    drawLine(16, 183, 33, 192, 3.0f);
    drawLine(16, 133, 33, 142, 3.0f);

    drawLine(379, 133, 379, 183, 3.0f);
    drawLine(379, 183, 362, 192, 3.0f);
    drawLine(379, 133, 362, 142, 3.0f);

    setColor(222, 234, 225);
    for (int i = 0; i < 4; ++i)
    {
        const float y = 145.0f + i * 11.0f;
        drawLine(18, y - 8.0f, 35, y, 1.0f);
        drawLine(377, y - 8.0f, 360, y, 1.0f);
    }
    for (int i = 0; i < 3; ++i)
    {
        const float xOffset = i * 6.0f;
        drawLine(18 + xOffset, 136.0f + xOffset * 0.5f, 18 + xOffset, 181.0f + xOffset * 0.5f, 1.0f);
        drawLine(377 - xOffset, 136.0f + xOffset * 0.5f, 377 - xOffset, 181.0f + xOffset * 0.5f, 1.0f);
    }

    setColor(248, 252, 248);
    drawLine(35, 142, 18, 142, 4.0f);
    drawLine(18, 142, 18, 192, 4.0f);
    drawLine(18, 192, 35, 192, 4.0f);

    drawLine(360, 142, 377, 142, 4.0f);
    drawLine(377, 142, 377, 192, 4.0f);
    drawLine(377, 192, 360, 192, 4.0f);

    setColor(168, 188, 174);
    drawLine(18, 142, 16, 133, 2.0f);
    drawLine(18, 192, 16, 183, 2.0f);
    drawLine(377, 142, 379, 133, 2.0f);
    drawLine(377, 192, 379, 183, 2.0f);
}

void drawFootballScene()
{
    setColor(73, 155, 76);
    drawRectangle(35, 100, 360, 235);

    setColor(230, 245, 230);
    drawLine(45, 108, 350, 108, 2.0f);
    drawLine(350, 108, 350, 226, 2.0f);
    drawLine(350, 226, 45, 226, 2.0f);
    drawLine(45, 226, 45, 108, 2.0f);
    drawLine(197, 108, 197, 226, 2.0f);
    drawFilledCircle(197, 167, 28, 40);

    setColor(73, 155, 76);
    drawFilledCircle(197, 167, 25, 40);

    drawFootballGoalPosts();

    if (hotWeatherBlend < 0.55f)
    {
        for (int i = 0; i < FOOTBALL_PLAYER_COUNT; ++i)
        {
            drawFootballPlayer(footballPlayers[i]);
        }

        drawFootball();
    }
}

void drawClouds()
{
    glPushMatrix();
    glTranslatef(cloudOffset, 0.0f, 0.0f);

    drawCloud(80, 555);
    drawCloud(360, 595);
    drawCloud(670, 548);
    drawCloud(945, 595);

    glPopMatrix();
}

// ── helper: draw a hanging lantern at (x,y), lit only at night ──────────────
void drawLantern(float x, float y)
{
    // string
    setColorAlpha(60, 40, 20, 1.0f);
    drawLine(x, y + 10, x, y, 1.0f);

    // body
    setColorAlpha(220, 80, 30, 1.0f);
    drawRectangle(x - 5, y - 12, x + 5, y);

    // glow only at night
    if (nightBlend > 0.0f)
    {
        setColorAlpha(255, 200, 80, 0.55f * nightBlend);
        drawFilledCircle(x, y - 6, 14, 20);
        setColorAlpha(255, 230, 120, 0.30f * nightBlend);
        drawFilledCircle(x, y - 6, 22, 20);
        setColorAlpha(255, 160, 40, 0.85f * nightBlend);
        drawRectangle(x - 4, y - 11, x + 4, y - 1);
    }
    else
    {
        setColorAlpha(255, 220, 100, 0.70f);
        drawRectangle(x - 4, y - 11, x + 4, y - 1);
    }

    // cap & bottom
    setColorAlpha(80, 50, 20, 1.0f);
    drawRectangle(x - 6, y - 1, x + 6, y + 1);
    drawRectangle(x - 6, y - 13, x + 6, y - 11);
    drawRectangle(x - 3, y - 16, x + 3, y - 13);
}

// ── single shop stall ────────────────────────────────────────────────────────
// ox,oy = bottom-left corner of stall, w=width, color of canopy
void drawStall(float ox, float oy, float w,
               float cr, float cg, float cb,   // canopy colour
               float sr, float sg, float sb)   // stripe colour
{
    const float h   = 38.0f;   // wall height
    const float ch  = 14.0f;  // canopy overhang height
    const float co  = 8.0f;   // canopy overhang beyond stall

    // back wall
    setColor(200, 178, 140);
    drawRectangle(ox, oy, ox + w, oy + h);

    // counter top
    setColor(160, 120, 70);
    drawRectangle(ox, oy + h - 10, ox + w, oy + h - 6);

    // goods on counter – colourful small items
    for (int i = 0; i < (int)(w / 9); ++i)
    {
        const float gx = ox + 4.0f + i * 9.0f;
        // alternate item colours
        if      (i % 4 == 0) setColor(220, 60,  60);
        else if (i % 4 == 1) setColor(60,  180, 80);
        else if (i % 4 == 2) setColor(240, 200, 50);
        else                  setColor(80,  130, 220);
        drawFilledCircle(gx, oy + h - 3, 3.5f, 8);
    }

    // canopy
    setColor(cr, cg, cb);
    drawTriangle(ox - co,       oy + h + ch,
                 ox + w + co,   oy + h + ch,
                 ox + w / 2.0f, oy + h + ch + 10.0f);
    drawRectangle(ox - co, oy + h, ox + w + co, oy + h + ch);

    // canopy stripes
    setColorAlpha(sr, sg, sb, 0.45f);
    for (int i = 0; i < (int)((w + 2 * co) / 8); ++i)
    {
        const float sx = ox - co + i * 8.0f;
        drawRectangle(sx, oy + h, sx + 4.0f, oy + h + ch);
    }

    // two wooden posts
    setColor(100, 65, 30);
    drawRectangle(ox + 2,     oy - 4, ox + 6,     oy + h);
    drawRectangle(ox + w - 6, oy - 4, ox + w - 2, oy + h);
}

// ── a simple villager figure at (px, py=feet) ───────────────────────────────
void drawVillager(float px, float py,
                  float sr, float sg, float sb,   // shirt
                  float pr2, float pg2, float pb2) // pants
{
    // legs
    setColor(pr2, pg2, pb2);
    drawRectangle(px - 5, py, px - 1, py + 16);
    drawRectangle(px + 1, py, px + 5, py + 16);

    // body
    setColor(sr, sg, sb);
    drawRectangle(px - 6, py + 16, px + 6, py + 30);

    // arm
    setColor(sr, sg, sb);
    drawLine(px - 6, py + 26, px - 14, py + 20, 2.0f);
    drawLine(px + 6, py + 26, px + 12, py + 22, 2.0f);

    // head
    setColor(210, 160, 110);
    drawFilledCircle(px, py + 37, 7, 16);

    // hair
    setColor(50, 30, 10);
    drawRectangle(px - 7, py + 39, px + 7, py + 44);
}

void drawVillageFair()
{
    // Placed on the existing ground (right side): x 670-998, y 95-260
    const float GX  = 670.0f;
    const float GY  = 95.0f;
    const float GW  = 328.0f;
    const float GH  = 165.0f;

    // ── entrance arch ───────────────────────────────────────────────────────
    setColor(130, 80, 30);
    drawRectangle(GX + 58,  GY + GH - 6,  GX + 68,  GY + GH + 30);
    drawRectangle(GX + GW - 68, GY + GH - 6, GX + GW - 58, GY + GH + 30);
    // arch beam
    setColor(160, 100, 40);
    drawRectangle(GX + 58, GY + GH + 24, GX + GW - 58, GY + GH + 30);
    // "MELA" sign on arch
    setColor(220, 60, 40);
    drawRectangle(GX + 110, GY + GH + 18, GX + GW - 110, GY + GH + 28);
    setColor(255, 240, 180);
    drawText(GX + 122, GY + GH + 20, "** VILLAGE MELA **");

    // decorative flags on arch
    float flagColors[6][3] = {
        {220,50,50}, {50,180,80}, {240,200,40},
        {60,120,220}, {200,60,180}, {240,140,30}
    };
    for (int i = 0; i < 6; ++i)
    {
        const float fx = GX + 68 + i * (GW - 136) / 5.0f;
        setColor(flagColors[i][0], flagColors[i][1], flagColors[i][2]);
        drawTriangle(fx, GY + GH + 30, fx + 10, GY + GH + 30, fx + 5, GY + GH + 20);
    }

    // ── row 1 stalls – back row (y ~ GY+GH-10 feet) ─────────────────────────
    // Stall 1 – sweets
    drawStall(GX + 8,  GY + 98,  52,
              190, 60,  60,   255, 255, 255);
    // Stall 2 – clothing
    drawStall(GX + 68, GY + 98,  52,
              50,  120, 200,  255, 255, 200);
    // Stall 3 – toys
    drawStall(GX + 128, GY + 98, 52,
              220, 160, 30,   255, 100, 30);
    // Stall 4 – vegetables
    drawStall(GX + 210, GY + 98, 52,
              60,  160, 70,   255, 255, 100);
    // Stall 5 – pottery
    drawStall(GX + 270, GY + 98, 52,
              160, 100, 50,   220, 180, 120);

    // ── row 2 stalls – front row (y ~ GY+48 feet) ───────────────────────────
    // Stall 6 – snacks
    drawStall(GX + 8,  GY + 48,  48,
              200, 80,  40,   255, 230, 180);
    // Stall 7 – bangles/jewellery
    drawStall(GX + 65, GY + 48,  48,
              180, 50,  160,  255, 200, 240);
    // Stall 8 – fruits
    drawStall(GX + 122, GY + 48, 48,
              40,  160, 80,   255, 220, 60);
    // Stall 9 – pots/utensils
    drawStall(GX + 220, GY + 48, 48,
              100, 80,  50,   200, 160, 80);
    // Stall 10 – sweets/mithai
    drawStall(GX + 275, GY + 48, 48,
              220, 100, 30,   255, 240, 180);

    // ── big hut / main tent in centre-back ──────────────────────────────────
    const float HX = GX + 118;
    const float HY = GY + 108;
    const float HW = 94.0f;

    // hut walls
    setColor(210, 185, 140);
    drawRectangle(HX, HY, HX + HW, HY + 52);

    // hut door
    setColor(100, 65, 30);
    drawRectangle(HX + HW/2 - 10, HY, HX + HW/2 + 10, HY + 28);

    // hut window left
    setColor(139, 195, 220);
    drawRectangle(HX + 8, HY + 18, HX + 28, HY + 36);
    setColor(100, 65, 30);
    drawLine(HX + 18, HY + 18, HX + 18, HY + 36, 1.5f);
    drawLine(HX + 8,  HY + 27, HX + 28, HY + 27, 1.5f);

    // hut window right
    setColor(139, 195, 220);
    drawRectangle(HX + HW - 28, HY + 18, HX + HW - 8, HY + 36);
    setColor(100, 65, 30);
    drawLine(HX + HW - 18, HY + 18, HX + HW - 18, HY + 36, 1.5f);
    drawLine(HX + HW - 28, HY + 27, HX + HW - 8,  HY + 27, 1.5f);

    // thatched roof
    setColor(160, 120, 50);
    drawTriangle(HX - 10, HY + 52, HX + HW / 2, HY + 90, HX + HW + 10, HY + 52);
    setColor(140, 100, 35);
    // thatch lines
    for (int i = 0; i < 5; ++i)
    {
        const float ty = HY + 56 + i * 6;
        const float tx1 = HX - 10 + (i + 1) * 10;
        const float tx2 = HX + HW + 10 - (i + 1) * 10;
        drawLine(tx1, ty, tx2, ty, 1.5f);
    }
    // roof ridge
    setColor(120, 80, 25);
    drawLine(HX + HW/2 - 4, HY + 88, HX + HW/2 + 4, HY + 88, 3.0f);

    // sign above door
    setColor(180, 50, 30);
    drawRectangle(HX + HW/2 - 22, HY + 50, HX + HW/2 + 22, HY + 60);
    setColor(255, 240, 180);
    drawText(HX + HW/2 - 19, HY + 52, "CHAI SHOP");

    // ── villagers scattered around ───────────────────────────────────────────
    // person 1 – in front of sweets stall
    drawVillager(GX + 35,  GY + 50,  220, 80,  40,  60,  60,  120);
    // person 2 – near clothing stall
    drawVillager(GX + 95,  GY + 50,  180, 60,  160, 80,  60,  30);
    // person 3 – near toy stall  (facing other way, slight offset)
    drawVillager(GX + 155, GY + 50,  50,  130, 200, 140, 100, 50);
    // person 4 – vegetable buyer
    drawVillager(GX + 238, GY + 50,  240, 200, 40,  60,  40,  100);
    // person 5 – near pottery
    drawVillager(GX + 295, GY + 50,  160, 80,  200, 40,  80,  140);
    // person 6 – sitting/crouching near hut (shorter draw)
    setColor(200, 140, 60);
    drawRectangle(GX + 180, GY + 98, GX + 192, GY + 110); // body
    setColor(210, 160, 110);
    drawFilledCircle(GX + 186, GY + 116, 6, 14);           // head

    // ── hanging lantern rope across fair ────────────────────────────────────
    // rope
    setColorAlpha(80, 55, 25, 0.7f);
    drawLine(GX + 58, GY + GH + 30, GX + GW - 58, GY + GH + 30, 1.0f);
    // second rope lower
    drawLine(GX + 68, GY + GH + 14, GX + GW - 68, GY + GH + 14, 1.0f);

    // lanterns along the top rope
    for (int i = 0; i < 7; ++i)
    {
        const float lx = GX + 80 + i * (GW - 120) / 6.0f;
        drawLantern(lx, GY + GH + 30);
    }
    // lanterns along lower rope
    for (int i = 0; i < 5; ++i)
    {
        const float lx = GX + 90 + i * (GW - 140) / 4.0f;
        drawLantern(lx, GY + GH + 14);
    }

    // ── festive ground decorations ───────────────────────────────────────────
    // rangoli-style dots on ground
    float dotC[4][3] = {{220,60,60},{60,180,80},{240,200,40},{80,130,220}};
    for (int i = 0; i < 12; ++i)
    {
        const float dx = GX + 72 + std::fmod(i * 23.0f, GW - 80);
        const float dy = GY + 6  + std::fmod(i * 17.0f, 36.0f);
        setColor(dotC[i%4][0], dotC[i%4][1], dotC[i%4][2]);
        drawFilledCircle(dx, dy, 2.5f, 8);
    }
}

void drawScene()
{
    drawRealisticSky();

    drawRealisticLand();

    drawMountain(-95, 260, 260, 420, 112, 154, 137);
    drawMountain(770, 260, 1095, 430, 104, 145, 131);
    drawMountain(35, 260, 430, 465, 82, 145, 113);
    drawMountain(570, 260, 965, 480, 77, 134, 111);

    drawWaterSystem();
    drawBoat();
    drawBridge();
    drawBridgeCar();
    drawBridgeJumpers();

    drawSun();
    drawStars();
    drawMoon();
    drawClouds();

    drawFootballScene();
    drawFieldLight();
    drawVillageFair();
    drawFerrisWheel(650, 183);
    drawTargetPracticeScene();
    drawBadmintonScene();

    drawEveningLighting();
    drawHotWeatherLighting();
    drawRain();
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    drawScene();
    glutSwapBuffers();
}

void resetScene()
{
    cloudOffset = -320.0f;
    windmillAngle = 0.0f;
    sunScale = 1.0f;
    boatT           = 0.0f;
    boatSpeed       = 0.0015f;
    boatGoingUp     = true;
    waterHighlightOffset = 0.0f;
    bridgeCarX = BRIDGE_RIGHT_X;
    bridgeCarDelayFrames = 0;
    eveningBlend = 0.0f;
    hotWeatherBlend = 0.0f;
    rainOffset = 0.0f;
    rainFramesRemaining = 0;
    fireFramesRemaining = 0;
    eveningRequested = false;
    hotWeatherRequested = false;
    nightRequested = false;
    isNight = false;
    nightBlend = 0.0f;
    sunSetProgress = 0.0f;
    moonRiseProgress = 0.0f;
    shuttleT     = 0.0f;
    shuttleDir   = 1.0f;
    badmintonArm1 = 0.0f;
    badmintonArm2 = 0.0f;
    ballHolderIndex = 0;
    ballTargetIndex = 0;
    footballX = footballPlayers[0].x + 10.0f;
    footballY = footballPlayers[0].y - 22.0f;
    footballMoving = false;
    resetBridgeJumpers();
    animationPaused = false;
    autoFrame        = 0;
    autoJump1Done    = false;
    autoJump2Done    = false;
    autoRainDone     = false;
    autoFireDone     = false;
    autoNightDone    = false;
    autoFireCooldown = 0;
    autoFireShots    = 0;
}

void timer(int)
{
    if (!animationPaused)
    {
        cloudOffset += 1.2f;
        if (cloudOffset > SCENE_WIDTH)
        {
            cloudOffset = -1030.0f;
        }

        windmillAngle -= 0.5f;
        if (windmillAngle < -360.0f)
        {
            windmillAngle += 360.0f;
        }

        // Vertical boat animation — t: 0=bottom, 1=at bridge, 1.35=fully past bridge
        if (boatGoingUp)
        {
            boatT += boatSpeed;
            if (boatT >= 1.35f)
            {
                boatT       = 1.35f;
                boatGoingUp = false;   // start returning
            }
        }
        else
        {
            boatT -= boatSpeed;
            if (boatT <= 0.0f)
            {
                boatT       = 0.0f;
                boatGoingUp = true;    // restart journey upward
            }
        }

        waterHighlightOffset += 2.0f;
        if (waterHighlightOffset > 240.0f)
        {
            waterHighlightOffset = 0.0f;
        }

        if (eveningRequested && eveningBlend < 1.0f)
        {
            eveningBlend += EVENING_TRANSITION_SPEED;
            if (eveningBlend > 1.0f)
            {
                eveningBlend = 1.0f;
            }
        }

        if (rainFramesRemaining > 0)
        {
            --rainFramesRemaining;
            rainOffset += 13.0f;
            if (rainOffset > SCENE_HEIGHT + 95.0f)
            {
                rainOffset = 0.0f;
            }

            if (rainFramesRemaining == 0)
            {
                hotWeatherRequested = true;
            }
        }

        if (hotWeatherRequested)
        {
            if (hotWeatherBlend < 1.0f)
            {
                hotWeatherBlend += HOT_WEATHER_TRANSITION_SPEED;
                if (hotWeatherBlend > 1.0f)
                {
                    hotWeatherBlend = 1.0f;
                }
            }

            if (eveningBlend > 0.0f)
            {
                eveningBlend -= HOT_WEATHER_TRANSITION_SPEED;
                if (eveningBlend < 0.0f)
                {
                    eveningBlend = 0.0f;
                }
            }
        }

        if (fireFramesRemaining > 0)
        {
            --fireFramesRemaining;
        }

        // Night transition animation
        if (nightRequested)
        {
            // Phase 1: sun sets (sunSetProgress 0->1) over first half
            if (sunSetProgress < 1.0f)
            {
                sunSetProgress += NIGHT_TRANSITION_SPEED * 1.6f;
                if (sunSetProgress > 1.0f) sunSetProgress = 1.0f;
            }
            // Phase 2: night darkens and moon rises (starts slightly after sun begins setting)
            if (sunSetProgress > 0.3f)
            {
                const float moonSpeed = NIGHT_TRANSITION_SPEED * 0.9f;
                if (moonRiseProgress < 1.0f)
                {
                    moonRiseProgress += moonSpeed;
                    if (moonRiseProgress > 1.0f) moonRiseProgress = 1.0f;
                }
                if (nightBlend < 1.0f)
                {
                    nightBlend += NIGHT_TRANSITION_SPEED * 1.1f;
                    if (nightBlend > 1.0f) nightBlend = 1.0f;
                }
            }
            if (sunSetProgress >= 1.0f && moonRiseProgress >= 1.0f && nightBlend >= 1.0f)
            {
                isNight = true;
            }
        }

        // Badminton animation (only at night)
        if (nightBlend > 0.5f)
        {
            shuttleT += shuttleSpeed;

            // shuttleT always 0->1 for the current leg.
            // shuttleDir: +1 = going p1->p2, -1 = going p2->p1
            if (shuttleDir > 0.0f)
            {
                // p1 hitting: arm up near t=0, down near t=1
                badmintonArm1 = 1.0f - shuttleT;
                // p2 receiving: arm rises as shuttle approaches
                badmintonArm2 = shuttleT;
            }
            else
            {
                // p2 hitting: arm up near t=0, down near t=1
                badmintonArm2 = 1.0f - shuttleT;
                // p1 receiving: arm rises as shuttle approaches
                badmintonArm1 = shuttleT;
            }

            if (shuttleT >= 1.0f)
            {
                shuttleT  = 0.0f;
                shuttleDir = -shuttleDir; // flip direction
            }
        }

        if (bridgeCarDelayFrames > 0)
        {
            --bridgeCarDelayFrames;
            if (bridgeCarDelayFrames == 0)
            {
                bridgeCarX = BRIDGE_RIGHT_X;
            }
        }
        else
        {
            bridgeCarX -= 0.9f;
            if (bridgeCarX < BRIDGE_LEFT_X)
            {
                bridgeCarDelayFrames = 125;
            }
        }

        updateBridgeJumpers();

        if (footballMoving)
        {
            const float targetX = footballPlayers[ballTargetIndex].x + 10.0f;
            const float targetY = footballPlayers[ballTargetIndex].y - 22.0f;
            const float dx = targetX - footballX;
            const float dy = targetY - footballY;
            const float distance = std::sqrt(dx * dx + dy * dy);

            if (distance <= 3.5f)
            {
                footballX = targetX;
                footballY = targetY;
                ballHolderIndex = ballTargetIndex;
                footballMoving = false;
            }
            else
            {
                footballX += dx / distance * 3.5f;
                footballY += dy / distance * 3.5f;
            }
        }

        // ── Automation sequencer ─────────────────────────────────────────────
        ++autoFrame;

        // Phase 1: Football passing — keep triggering every interval, stop once night begins
        if (!autoNightDone && !nightRequested)
        {
            if (autoFrame % AUTO_FOOTBALL_PASS_INTERVAL == 0 && !footballMoving)
                startFootballPass();
        }

        // Phase 2: Bridge jumpers — jump1 then jump2 with 2.5s gap
        if (!autoJump1Done && autoFrame >= AUTO_JUMP1_FRAME)
        {
            startNextBridgeJump();
            autoJump1Done = true;
        }
        if (!autoJump2Done && autoFrame >= AUTO_JUMP2_FRAME)
        {
            startNextBridgeJump();
            autoJump2Done = true;
        }

        // Phase 3: Rain (key A behaviour)
        if (!autoRainDone && autoFrame >= AUTO_RAIN_FRAME)
        {
            eveningRequested      = true;
            hotWeatherRequested   = false;
            hotWeatherBlend       = 0.0f;
            rainFramesRemaining   = RAIN_DURATION_FRAMES;
            rainOffset            = 0.0f;
            autoRainDone          = true;
        }

        // Phase 4: Auto-fire shots after hot weather has kicked in
        if (autoRainDone && !autoFireDone && hotWeatherBlend > 0.75f)
        {
            if (autoFireShots < AUTO_FIRE_SHOTS)
            {
                if (autoFireCooldown <= 0)
                {
                    fireFramesRemaining = 8;
                    ++autoFireShots;
                    autoFireCooldown = AUTO_FIRE_INTERVAL;
                }
                else
                {
                    --autoFireCooldown;
                }
            }
            else
            {
                autoFireDone = true;
                autoFireCooldown = AUTO_NIGHT_DELAY; // reuse as night delay counter
            }
        }

        // Phase 5: Night — trigger after fire shots done + delay, only once
        if (autoFireDone && !autoNightDone)
        {
            if (autoFireCooldown > 0)
            {
                --autoFireCooldown;
            }
            else
            {
                nightRequested  = true;
                autoNightDone   = true;
            }
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

void keyboard(unsigned char key, int, int)
{
    switch (key)
    {
    case 'p':
    case 'P':
        animationPaused = !animationPaused;
        break;

    case '+':
    case '=':
        sunScale += 0.08f;
        if (sunScale > 1.8f)
        {
            sunScale = 1.8f;
        }
        break;

    case '-':
    case '_':
        sunScale -= 0.08f;
        if (sunScale < 0.55f)
        {
            sunScale = 0.55f;
        }
        break;

    case 'r':
    case 'R':
        resetScene();
        break;

    case 'n':
    case 'N':
        if (!nightRequested)
        {
            nightRequested = true;
            isNight = false;
            sunSetProgress = 0.0f;
            moonRiseProgress = 0.0f;
            nightBlend = 0.0f;
        }
        break;

    case 'a':
    case 'A':
        eveningRequested = true;
        hotWeatherRequested = false;
        hotWeatherBlend = 0.0f;
        rainFramesRemaining = RAIN_DURATION_FRAMES;
        rainOffset = 0.0f;
        break;

    case 'f':
    case 'F':
        if (hotWeatherBlend > 0.75f)
        {
            fireFramesRemaining = 8;
        }
        break;

    case ' ':
        startNextBridgeJump();
        break;

    case 27:
        std::exit(0);
        break;
    }

    glutPostRedisplay();
}

void specialKeyboard(int key, int, int)
{
    switch (key)
    {
    case GLUT_KEY_UP:
        boatSpeed += 0.001f;
        if (boatSpeed > 0.012f)
        {
            boatSpeed = 0.012f;
        }
        break;

    case GLUT_KEY_DOWN:
        boatSpeed -= 0.001f;
        if (boatSpeed < 0.001f)
        {
            boatSpeed = 0.001f;
        }
        break;

    default:
        break;
    }

    glutPostRedisplay();
}

void mouse(int button, int state, int, int)
{
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
    {
        startFootballPass();
        glutPostRedisplay();
    }
}

void reshape(int width, int height)
{
    windowWidth = width;
    windowHeight = height;

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, SCENE_WIDTH, 0, SCENE_HEIGHT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void init()
{
    glClearColor(135.0f / 255.0f, 206.0f / 255.0f, 235.0f / 255.0f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    resetBridgeJumpers();
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(100, 80);
    glutCreateWindow("2D Transformation Village Scenario");

    init();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeyboard);
    glutMouseFunc(mouse);
    glutTimerFunc(0, timer, 0);

    glutMainLoop();
    return 0;
}
