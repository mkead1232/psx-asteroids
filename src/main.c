/*
  This is an Asteroids clone I, Aden Kirk (mkead1232) wrote in 2 files. main.c,
  and my_image.s. I built it off of Lameguy64's PS1 tutorial series for PSY-Q.
  This is my first *real* playstation project that is a game and not a tech
  demo. I also wrote this on my steam deck on vacation.

  Sorry for the messy file structure!
  -Aden

  Made for the mips GCC compiler
*/

#include <libapi.h> // API header, has InitPAD() and StartPAD() defs
#include <libetc.h> // Includes some functions that controls the display
#include <libgpu.h> // GPU library header
#include <libgte.h> // GTE header, not really used but libgpu.h depends on it
#include <stdio.h>  // Not necessary but include it anyway
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> // This provides typedefs needed by libgte.h and libgpu.h

typedef enum {
    MENU_1_PLAYER,
    MENU_2_PLAYER,
    MENU_OPTION_COUNT
} MenuOption;

int twoPMessageTimer = 0;
int twoPMessageFade = 255;

int selectedOption = 0;

int titleDelay = 60; // frames, e.g. 1 second delay at 60fps

int alive;

#define MAX_EXPLOSIONS 16

int score;

typedef struct {
    int x, y;
    int frame;
    int active;
} Explosion;

Explosion explosions[MAX_EXPLOSIONS];

typedef struct {
    int x, y;   // 12.20 position
    int vx, vy; // velocity
    int angle;  // current rotation
    int spin;   // spin speed
    int scale;  // sprite scale
    int active;
} Asteroid;

#define MAX_ASTEROIDS 8
Asteroid asteroids[MAX_ASTEROIDS];
#define BULLET_RADIUS 4
#define ASTEROID_RADIUS 32 // Match the visual size of 64x64 sprite

typedef struct {
    int x, y;   // 12.20 fixed point position
    int vx, vy; // 12.20 fixed point velocity
    int active;
} Bullet;

#define MAX_BULLETS 1
Bullet bullets[MAX_BULLETS];

#define OTLEN                                                                  \
    8 // Ordering table length (recommended to set as a define
      // so it can be changed easily)

DISPENV disp[2]; // Display/drawing buffer parameters
DRAWENV draw[2];
int db = 0;

// PSn00bSDK requires having all u_long types replaced with
// u_int, as u_long in modern GCC that PSn00bSDK uses defines it as a 64-bit
// integer.

u_long ot[2][OTLEN];    // Ordering table length
char pribuff[2][32768]; // Primitive buffer
char *nextpri;          // Next primitive pointer

int tim_mode; // TIM image parameters
RECT tim_prect, tim_crect;
int tim_uoffs, tim_voffs;

int notImplementedTimer = 0;

// Pad stuff (omit when using PSn00bSDK)
#define PAD_SELECT 1
#define PAD_L3 2
#define PAD_R3 4
#define PAD_START 8
#define PAD_UP 16
#define PAD_RIGHT 32
#define PAD_DOWN 64
#define PAD_LEFT 128
#define PAD_L2 256
#define PAD_R2 512
#define PAD_L1 1024
#define PAD_R1 2048
#define PAD_TRIANGLE 4096
#define PAD_CIRCLE 8192
#define PAD_CROSS 16384
#define PAD_SQUARE 32768

typedef struct _PADTYPE {
    unsigned char stat;
    unsigned char len : 4;
    unsigned char type : 4;
    unsigned short btn;
    unsigned char rs_x, rs_y;
    unsigned char ls_x, ls_y;
} PADTYPE;

int shake_timer = 0;
int shake_magnitude = 0;

// pad buffer arrays
u_char padbuff[2][34];

SVECTOR player_tri[] = {{0, -10, 0}, {10, 20, 0}, {-10, 20, 0}};

// Global player position, velocity, angle
int pos_x, pos_y, angle;
int vel_x, vel_y;

typedef enum {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_GAME_OVER
} GameState;

GameState gameState = STATE_TITLE;

// Forward declarations
void display(void);
void loadstuff(void);
void init(void);
void sortRotSprite(int x, int y, int pw, int ph, int angle, int scale);
void spawnAsteroid(void);
int checkCollision(int x1, int y1, int r1, int x2, int y2, int r2);
void updateTitleScreen(PADTYPE *pad);
void updateGame(PADTYPE *pad);
void updateGameOver(PADTYPE *pad);

void display() {
    DrawSync(0); // Wait for any graphics processing to finish
    VSync(0);    // Wait for vertical retrace

    PutDispEnv(&disp[db]); // Apply display environment

    DRAWENV shaken_env = draw[db]; // Copy draw env for this frame

    if (shake_timer > 0) {
        int xoff = (rand() % (shake_magnitude * 2 + 1)) - shake_magnitude;
        int yoff = (rand() % (shake_magnitude * 2 + 1)) - shake_magnitude;
        shaken_env.ofs[0] += xoff;
        shaken_env.ofs[1] += yoff;
        shake_timer--;
    }

    PutDrawEnv(&shaken_env); // Only use this
    SetDispMask(1);          // Enable display

    DrawOTag(ot[db] + OTLEN - 1); // Draw ordering table

    db = !db;                 // Swap buffers
    nextpri = pribuff[db];    // Reset primitive pointer
}

// Texture upload function
void LoadTexture(u_long *tim, TIM_IMAGE *tparam) {

    // Read TIM parameters (PsyQ)
    OpenTIM(tim);
    ReadTIM(tparam);

    // Upload pixel data to framebuffer
    LoadImage(tparam->prect, (u_long *)tparam->paddr);
    DrawSync(0);

    // Upload CLUT to framebuffer if present
    if (tparam->mode & 0x8) {

        LoadImage(tparam->crect, (u_long *)tparam->caddr);
        DrawSync(0);
    }
}

void loadstuff(void) {

    TIM_IMAGE my_image; // TIM image parameters

    extern u_long tim_my_image[];

    // Load the texture
    LoadTexture(tim_my_image, &my_image);

    // Copy the TIM coordinates
    tim_prect = *my_image.prect;
    tim_crect = *my_image.crect;
    tim_mode = my_image.mode;

    // Calculate U,V offset for TIMs that are not page aligned
    tim_uoffs = (tim_prect.x % 64) << (2 - (tim_mode & 0x3));
    tim_voffs = (tim_prect.y & 0xff);
}

// To make main look tidy, init stuff has to be moved here
void init(void) {

    // Reset graphics
    ResetGraph(0);

    // First buffer
    SetDefDispEnv(&disp[0], 0, 0, 320, 240);
    SetDefDrawEnv(&draw[0], 0, 240, 320, 240);
    // Second buffer
    SetDefDispEnv(&disp[1], 0, 240, 320, 240);
    SetDefDrawEnv(&draw[1], 0, 0, 320, 240);

    draw[0].isbg = 1; // Enable clear
    setRGB0(&draw[0], 0, 0, 0);
    draw[1].isbg = 1;
    setRGB0(&draw[1], 0, 0, 0);

    nextpri = pribuff[0]; // Set initial primitive pointer address

    // load textures and possibly other stuff
    loadstuff();

    // set tpage of lone texture as initial tpage
    draw[0].tpage = getTPage(tim_mode & 0x3, 0, tim_prect.x, tim_prect.y);
    draw[1].tpage = getTPage(tim_mode & 0x3, 0, tim_prect.x, tim_prect.y);

    // apply initial drawing environment
    PutDrawEnv(&draw[!db]);

    // Initialize the pads
    InitPAD(padbuff[0], 34, padbuff[1], 34);

    // Begin polling
    StartPAD();

    // To avoid VSync Timeout error, may not be defined in PsyQ
    ChangeClearPAD(1);

    // Load the font texture on the upper-right corner of the VRAM
    FntLoad(960, 0);

    // Define a font window of 100 characters covering the whole screen
    FntOpen(0, 8, 320, 224, 0, 200);

    draw[0].dtd = 1; // Enable dithering
    draw[1].dtd = 1;
}

void sortRotSprite(int x, int y, int pw, int ph, int angle, int scale) {
    POLY_FT4 *quad;
    SVECTOR s[4];
    SVECTOR v[4];

    int i, cx, cy;

    // calculate the pivot point (center) of the sprite
    cx = pw >> 1;
    cy = ph >> 1;

    // increment by 0.5 on the bottom and right coords so scaling
    // would increment a bit smoother
    s[0].vx = -(((pw * scale) >> 12) - cx);
    s[0].vy = -(((ph * scale) >> 12) - cy);

    s[1].vx = (((pw * scale) + 2048) >> 12) - cx;
    s[1].vy = s[0].vy;

    s[2].vx = -(((pw * scale) >> 12) - cx);
    s[2].vy = (((ph * scale) + 2048) >> 12) - cy;

    s[3].vx = (((pw * scale) + 2048) >> 12) - cx;
    s[3].vy = s[2].vy;

    // a simple but pretty effective optimization trick
    cx = ccos(angle);
    cy = csin(angle);

    // calculate rotated sprite coordinates
    for (i = 0; i < 4; i++) {
        v[i].vx = (((s[i].vx * cx) - (s[i].vy * cy)) >> 12) + x;
        v[i].vy = (((s[i].vy * cx) + (s[i].vx * cy)) >> 12) + y;
    }

    // initialize the quad primitive for the sprite
    quad = (POLY_FT4 *)nextpri;
    setPolyFT4(quad);

    // set CLUT and tpage to the primitive
    setTPage(quad, tim_mode & 0x3, 0, tim_prect.x, tim_prect.y);
    setClut(quad, tim_crect.x, tim_crect.y);

    // set color, screen coordinates and texture coordinates of primitive
    setRGB0(quad, 128, 128, 128);
    setXY4(quad, v[0].vx, v[0].vy, v[1].vx, v[1].vy, v[2].vx, v[2].vy, v[3].vx,
           v[3].vy);
    setUVWH(quad, tim_uoffs, tim_voffs, pw, ph);

    // add it to the ordering table
    addPrim(ot[db], quad);
    nextpri += sizeof(POLY_FT4);
}

void spawnAsteroid() {
    int i;
    for (i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) {
            int side = rand() % 4; // 0 = left, 1 = right, 2 = top, 3 = bottom
            int x, y;

            switch (side) {
            case 0:
                x = -32;
                y = rand() % 240;
                break; // Left
            case 1:
                x = 320 + 32;
                y = rand() % 240;
                break; // Right
            case 2:
                x = rand() % 320;
                y = -32;
                break; // Top
            case 3:
                x = rand() % 320;
                y = 240 + 32;
                break; // Bottom
            }

            int angle = rand() % 4096;
            int speed = (rand() % 3 + 1) << 11; // speed: 0.5 - 1.5

            asteroids[i].x = x << 12;
            asteroids[i].y = y << 12;
            asteroids[i].vx = (csin(angle) * speed) >> 12;
            asteroids[i].vy = (ccos(angle) * speed) >> 12;
            asteroids[i].angle = rand() % 4096;
            asteroids[i].spin = (rand() % 7) - 3; // spin between -3 and +3
            asteroids[i].scale = ONE;
            asteroids[i].active = 1;
            break;
        }
    }
}

int checkCollision(int x1, int y1, int r1, int x2, int y2, int r2) {
    int dx = (x1 - x2) >> 12;
    int dy = (y1 - y2) >> 12;
    int distSq = dx * dx + dy * dy;
    int radSum = r1 + r2;
    return distSq <= radSum * radSum;
}

// ---- GAME UPDATE ----
void updateGame(PADTYPE *pad) {
    static int spawn_timer = 0;
    int i;

    // Player input
    if (!(pad->btn & PAD_UP)) {
        vel_x += csin(angle) >> 3;
        vel_y -= ccos(angle) >> 3;
    } else if (!(pad->btn & PAD_DOWN)) {
        vel_x -= csin(angle) >> 3;
        vel_y += ccos(angle) >> 3;
    }
    if (!(pad->btn & PAD_LEFT)) {
        angle -= 32;
    } else if (!(pad->btn & PAD_RIGHT)) {
        angle += 32;
    }

    // Fire bullet with Triangle
    if (!(pad->btn & PAD_TRIANGLE)) {
        for (i = 0; i < MAX_BULLETS; i++) {
            if (!bullets[i].active) {
                int bx = player_tri[0].vx << 12;
                int by = player_tri[0].vy << 12;

                int speed = 3 << 12;
                int bvx = (csin(angle) * speed) >> 12;
                int bvy = -(ccos(angle) * speed) >> 12;

                bullets[i].x = pos_x + bx;
                bullets[i].y = pos_y + by;
                bullets[i].vx = bvx;
                bullets[i].vy = bvy;
                bullets[i].active = 1;
                break;
            }
        }
    }

    spawn_timer++;
    if (spawn_timer > 60) { // ~1 sec @ 60fps
        spawnAsteroid();
        spawn_timer = 0;
    }

    // Apply friction
    vel_x = (vel_x * 4000) >> 12;
    vel_y = (vel_y * 4000) >> 12;

    // Move player
    pos_x += vel_x;
    pos_y += vel_y;

    // Screen wrap for player
    if ((pos_x >> 12) < 0) pos_x += (320 << 12);
    if ((pos_x >> 12) > 320) pos_x -= (320 << 12);
    if ((pos_y >> 12) < 0) pos_y += (240 << 12);
    if ((pos_y >> 12) > 240) pos_y -= (240 << 12);

    // Rotate and draw player triangle
    SVECTOR v[3];
    for (i = 0; i < 3; i++) {
        v[i].vx = (((player_tri[i].vx * ccos(angle)) -
                    (player_tri[i].vy * csin(angle))) >>
                   12) +
                  (pos_x >> 12);
        v[i].vy = (((player_tri[i].vy * ccos(angle)) +
                    (player_tri[i].vx * csin(angle))) >>
                   12) +
                  (pos_y >> 12);
    }

    POLY_F3 *tri = (POLY_F3 *)nextpri;
    setPolyF3(tri);
    setRGB0(tri, 255, 255, 255);
    setXY3(tri, v[0].vx, v[0].vy, v[1].vx, v[1].vy, v[2].vx, v[2].vy);
    addPrim(ot[db], tri);
    nextpri += sizeof(POLY_F3);

    // Update and draw bullets
    for (i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            bullets[i].x += bullets[i].vx;
            bullets[i].y += bullets[i].vy;

            int sx = bullets[i].x >> 12;
            int sy = bullets[i].y >> 12;

            for (int e = 0; e < MAX_EXPLOSIONS; e++) {
                if (!explosions[e].active) {
                    explosions[e].x = bullets[i].x;
                    explosions[e].y = bullets[i].y;
                    explosions[e].frame = 0;
                    explosions[e].active = 1;
                    break;
                }
            }


            if (sx < 0 || sx > 320 || sy < 0 || sy > 240) {
                bullets[i].active = 0;
                continue;
            }

            POLY_F4 *b = (POLY_F4 *)nextpri;
            setPolyF4(b);
            setRGB0(b, 255, 255, 255);
            setXY4(b, sx - 2, sy - 2, sx + 2, sy - 2, sx - 2, sy + 2, sx + 2,
                   sy + 2);
            addPrim(ot[db], b);
            nextpri += sizeof(POLY_F4);
        }
    }

    // Update and draw asteroids
    for (i = 0; i < MAX_ASTEROIDS; i++) {
        if (asteroids[i].active) {
            asteroids[i].x += asteroids[i].vx;
            asteroids[i].y += asteroids[i].vy;
            asteroids[i].angle += asteroids[i].spin;

            int sx = asteroids[i].x >> 12;
            int sy = asteroids[i].y >> 12;

            if (sx < -64 || sx > 384 || sy < -64 || sy > 304) {
                asteroids[i].active = 0;
                continue;
            }

            sortRotSprite(sx, sy, 64, 64, asteroids[i].angle, asteroids[i].scale);
        }
    }

    // Bullet-Asteroid collisions
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].active)
            continue;

        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active)
                continue;

            if (checkCollision(bullets[b].x, bullets[b].y, BULLET_RADIUS,
                               asteroids[a].x, asteroids[a].y, ASTEROID_RADIUS)) {
                bullets[b].active = 0;
                asteroids[a].active = 0;
                score += 1;

                // Create explosion
                for (int e = 0; e < MAX_EXPLOSIONS; e++) {
                    if (!explosions[e].active) {
                        explosions[e].x = bullets[b].x;
                        explosions[e].y = bullets[b].y;
                        explosions[e].frame = 0;
                        explosions[e].active = 1;
                        break;
                    }
                }
            }
        }
    }

    // Player collision with asteroids
    for (int a = 0; a < MAX_ASTEROIDS; a++) {
        if (!asteroids[a].active)
            continue;

        if (checkCollision(pos_x, pos_y, 16, asteroids[a].x, asteroids[a].y,
                           ASTEROID_RADIUS)) {
            score = 0;

            pos_x = ONE * (disp[0].disp.w >> 1);
            pos_y = ONE * (disp[0].disp.h >> 1);

            angle = 0;
            vel_x = 0;
            vel_y = 0;

            asteroids[a].active = 0;

            alive = 0;
            shake_timer = 30;
            shake_magnitude = 4;

            gameState = STATE_GAME_OVER;
        }
    }

    // Update and draw explosions
    for (int e = 0; e < MAX_EXPLOSIONS; e++) {
        if (!explosions[e].active)
            continue;

        int sx = explosions[e].x >> 12;
        int sy = explosions[e].y >> 12;
        int size = 6 + explosions[e].frame * 2;
        int fade = 255 - explosions[e].frame * 32;
        if (fade < 0)
            fade = 0;

        TILE *t = (TILE *)nextpri;
        setTile(t);
        setXY0(t, sx - (size >> 1), sy - (size >> 1));
        setWH(t, size, size);
        setRGB0(t, fade, fade >> 1, 0);
        addPrim(ot[db], t);
        nextpri += sizeof(TILE);

        explosions[e].frame++;
        if (explosions[e].frame > 7) {
            explosions[e].active = 0;
        }
    }

    // Draw score
    FntPrint("SCORE: %d", score);
}

// Assumes 40 chars wide font window, 8px wide chars
void centerPrint(const char *str) {
    int len = strlen(str);
    int total_width = 40; // 40 characters max per line
    int pad = 0;

    if (len < total_width)
        pad = (total_width - len) / 2;
    
    for (int i = 0; i < pad; i++)
        FntPrint(" ");

    FntPrint("%s\n", str);
}

void updateTitleScreen(PADTYPE *pad) {

    FntPrint("\n\n\n\n\n\n\n\n\n\n");
    centerPrint("ASTEROIDS");
    centerPrint("by Aden Kirk\n");

    const char *options[] = {
        "1 PLAYER",
        "2 PLAYER"
    };

    char buffer[32];

    for (int i = 0; i < MENU_OPTION_COUNT; i++) {
        if (i == selectedOption)
            sprintf(buffer, "> %s <", options[i]);
        else
            sprintf(buffer, "  %s", options[i]);

        centerPrint(buffer);
    }
    // Handle input
    static int prev_buttons = 0;

    if ((pad->btn & PAD_DOWN) && !(prev_buttons & PAD_DOWN)) {
        selectedOption = (selectedOption + 1) % MENU_OPTION_COUNT;
    } else if ((pad->btn & PAD_UP) && !(prev_buttons & PAD_UP)) {
        selectedOption = (selectedOption - 1 + MENU_OPTION_COUNT) % MENU_OPTION_COUNT;
    }

    if ((pad->btn & PAD_START) && !(prev_buttons & PAD_START)) {
        if (selectedOption == MENU_1_PLAYER) {
            // Start game
            alive = 1;
            score = 0;
            pos_x = ONE * (disp[0].disp.w >> 1);
            pos_y = ONE * (disp[0].disp.h >> 1);
            angle = 0;
            vel_x = 0;
            vel_y = 0;

            for (int i = 0; i < MAX_ASTEROIDS; i++)
                asteroids[i].active = 0;
            for (int i = 0; i < MAX_BULLETS; i++)
                bullets[i].active = 0;
            for (int i = 0; i < MAX_EXPLOSIONS; i++)
                explosions[i].active = 0;

            gameState = STATE_PLAYING;
        }
        
    }

    centerPrint("2P NOT IMPLEMENTED YET");

    prev_buttons = pad->btn;
}


// ---- GAME OVER SCREEN UPDATE ----
void updateGameOver(PADTYPE *pad) {
    FntPrint("\n\n\n\n\n\n\n\n\n\n");
    centerPrint("GAME OVER!\n\n");
    centerPrint("PRESS X TO RESTART\n");

    if (!(pad->btn & PAD_CROSS)) {
        alive = 1;
        score = 0;
        pos_x = ONE * (disp[0].disp.w >> 1);
        pos_y = ONE * (disp[0].disp.h >> 1);
        angle = 0;
        vel_x = 0;
        vel_y = 0;

        for (int i = 0; i < MAX_ASTEROIDS; i++)
            asteroids[i].active = 0;
        for (int i = 0; i < MAX_BULLETS; i++)
            bullets[i].active = 0;
        for (int i = 0; i < MAX_EXPLOSIONS; i++)
            explosions[i].active = 0;

        gameState = STATE_PLAYING;
    }
}

int main() {
    // Initialize everything
    init();

    // Initialize player position and state
    pos_x = ONE * (disp[0].disp.w >> 1);
    pos_y = ONE * (disp[0].disp.h >> 1);
    angle = 0;
    vel_x = 0;
    vel_y = 0;
    alive = 1;
    score = 0;
    gameState = STATE_TITLE;

    while (1) {
        PADTYPE *pad = (PADTYPE *)padbuff[0];

        ClearOTagR(ot[db], OTLEN);

        switch (gameState) {
        case STATE_TITLE:
            updateTitleScreen(pad);
            break;
        case STATE_PLAYING:
            updateGame(pad);
            break;
        case STATE_GAME_OVER:
            updateGameOver(pad);
            break;
        }

        FntFlush(-1);

        display();
    }

    return 0;
}
