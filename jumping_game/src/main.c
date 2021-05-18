#include "raylib.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static float gravity = 500.0f;
static float impulse = 250.0f;
static float run_speed = 200.0f;
static float accel = 5.0f;

static float cactus_min_dist = 200.0f;
static float cactus_max_dist = 600.0f;
static int cactus_min_pairs = 1;
static int cactus_max_pairs = 4;

static Sound score_reached_snd;
static Sound button_press_snd;
static Texture2D sprite_tex;

static int last_hundred_div = 0;
static int score = 0;
static double time_in_game = 0.0;
static float dino_vel_y = 0.0f;
static float dino_pos_y = 0.0f;
static Vector2 dino_pos;
static Vector2 dino_pos_world;
static bool in_game = false;
static bool lost_game = false;

float randf(float a, float b) {
    return a + ((float)rand() / (float)RAND_MAX) * (b - a);
}

int randi(int a, int b) {
    return (int)randf(a, b);
}

typedef struct {
    bool valid;
    float x_pos;
    //Rectangle bounds;
} cactus;

cactus cactuses[32] = {0};

cactus* get_last_cactus() {
    cactus* last_cactus = NULL;
    float cactus_max_dist = 0.0f;

    for (int i = 0; i < sizeof(cactuses) / sizeof(*cactuses); i++) {
        cactus* iter = &cactuses[i];

        if (iter->valid &&  iter->x_pos > cactus_max_dist) {
            cactus_max_dist = iter->x_pos;
            last_cactus = iter;
        }
    }

    return last_cactus;
}

void draw_sprite(Texture2D tex, Rectangle region, Vector2 dst, int alpha) {
    DrawTexturePro(tex, region,
        (Rectangle) {
        dst.x, dst.y, region.width, region.height
    },
        (Vector2) {0,0},
        0.0f,
        (Color) {255,255,255, alpha
        });
}

typedef enum {
    DINO_WAITING_1 = 0,
    DINO_WAITING_2,
    DINO_RUNNING_1,
    DINO_RUNNING_2,
    DINO_JUMPING,
} dino_state;

void draw_dino(Vector2 dst, dino_state state) {
    int alpha = lost_game ? (fmodf(GetTime(), 0.75f) > 0.375f ? 0 : 255) : 255;

    switch (state) {
    case DINO_WAITING_1: draw_sprite(sprite_tex, (Rectangle) { 44, 2, 44, 47 }, dst, alpha); break;
    case DINO_WAITING_2: draw_sprite(sprite_tex, (Rectangle) { 848 + 0, 2, 44, 47 }, dst, alpha); break;
    case DINO_RUNNING_1: draw_sprite(sprite_tex, (Rectangle) { 848 + 88, 2, 44, 47}, dst, alpha); break;
    case DINO_RUNNING_2: draw_sprite(sprite_tex, (Rectangle) { 848 + 132, 2, 44, 47 }, dst, alpha); break;
    case DINO_JUMPING: draw_sprite(sprite_tex, (Rectangle) { 848 + 0, 2, 44, 47}, dst, alpha); break;
    }
}

void draw_cactus(Vector2 dst) {
    draw_sprite(sprite_tex, (Rectangle) { 228, 2, 17, 35 }, dst, 255);
}

void draw_ground(Vector2 dst, float fraction, int alpha) {
    draw_sprite(sprite_tex, (Rectangle) { 2, 54, 1200.0f * fraction, 12 }, dst, alpha);
}

int main(void) {
    srand(time(NULL));

    const int font_size = 20;
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitAudioDevice();
    SetMasterVolume(1.0f);
    InitWindow(screenWidth, screenHeight, "Google Chrome");

    SetTargetFPS(60);

    score_reached_snd = LoadSound("rsc/sounds/score-reached.mp3");
    button_press_snd = LoadSound("rsc/sounds/button-press.mp3");
    sprite_tex = LoadTexture("rsc/sprites/sprites.png");

    while (!WindowShouldClose()) {
        BeginDrawing();

        ClearBackground(WHITE);

        static float lost_time = 0.0f;

        if (in_game) {
            /* timing */
            double time_since_start = GetTime() - time_in_game;

            if (lost_game) {
                time_since_start = lost_time;
            }

            const double feet_speed = sinf(time_since_start * 8.0f * PI);

            if (!lost_game) {
                last_hundred_div = score / 100;
                score = time_since_start * 10.0f;

                if (last_hundred_div != score / 100) {
                    PlaySound(score_reached_snd);
                }

                /* game physics */
                if (dino_pos_y && IsKeyDown(KEY_DOWN)) {
                    dino_vel_y = -impulse * 1.1f;
                }
                else if (!dino_pos_y && (IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_UP))) {
                    dino_vel_y = impulse;
                    PlaySound(button_press_snd);
                }

                dino_vel_y -= gravity * GetFrameTime();
                dino_pos_y += dino_vel_y * GetFrameTime();
                dino_pos_y = max(dino_pos_y, 0.0f);

                if (!dino_pos_y)
                    dino_vel_y = 0.0f;

                dino_pos.y = screenHeight / 2 - 12 / 2 - 47 - dino_pos_y;

                /* rendering */
                dino_pos.x = 22.0f + (screenWidth / 2.0f - 44.0f) * sinf(min(time_since_start, PI / 2.0f));
                dino_pos_world = (Vector2){ dino_pos_world.x + run_speed * GetFrameTime(), 172.0f - dino_pos.y };
            }

            const float ground_offset = fmodf(time_since_start * -run_speed, run_speed * 2.0f);

            /* delete off-screen cactuses */
            for (int i = 0; i < sizeof(cactuses) / sizeof(*cactuses); i++) {
                cactus* iter = &cactuses[i];

                if (iter->valid && iter->x_pos < dino_pos_world.x - run_speed * 1.05f) {
                    memset(iter, 0, sizeof(cactus));
                }
            }

            /* create cactus if space is available & run collision checks */
            cactus* last_cactus = get_last_cactus();

            if (IsKeyPressed(KEY_SPACE) && lost_game) {
                in_game = false;
                lost_game = false;
            }

            if (last_cactus) {
                for (int i = 0; i < sizeof(cactuses) / sizeof(*cactuses); i++) {
                    cactus* iter = &cactuses[i];

                    if (!iter->valid) {
                        last_cactus = get_last_cactus();
                        *iter = (cactus){ true, randf(last_cactus->x_pos + cactus_min_dist,last_cactus->x_pos + cactus_max_dist) };
                        last_cactus = iter;
                    }

                    if (iter->valid ) {
                        float hitbox_padding = 6.0f;
                        Rectangle cactus_screen = (Rectangle){ iter->x_pos - (dino_pos_world.x - run_speed * 0.5f), 172.0f + 6.0f - 35.0f, 17.0f, 35.0f };
                        Rectangle dino_screen = (Rectangle){ dino_pos.x + hitbox_padding, dino_pos.y - 47.0f, 44.0f - hitbox_padding * 2.0f, 47.0f - hitbox_padding * 0.2f };

                        if (CheckCollisionRecs(cactus_screen, dino_screen)) {
                            lost_game = true;
                            lost_time = time_since_start;
                        }
                    }
                }
            }

            draw_dino(dino_pos, dino_pos_y ? DINO_JUMPING : (feet_speed < 0.0f ? DINO_RUNNING_1 : DINO_RUNNING_2));

            /* draw cacti */
            for (int i = 0; i < sizeof(cactuses) / sizeof(*cactuses); i++) {
                cactus* iter = &cactuses[i];

                if (iter->valid) {
                    draw_cactus((Vector2) { iter->x_pos - (dino_pos_world.x - run_speed * 0.5f), 172.0f + 6.0f });
                }
            }

            draw_ground((Vector2) { ground_offset, screenHeight / 2 - 18 }, min(time_since_start, 1.0f), min(time_since_start, 1.0f) * 255.0f);
            draw_ground((Vector2) { ground_offset + 1200.0f, screenHeight / 2 - 18 }, min(time_since_start, 1.0f), min(time_since_start, 1.0f) * 255.0f);
            
            if (!lost_game) {
                /* increase dino speed every second (makes game progressively harder) */
                run_speed += accel * GetFrameTime();
            }
        }
        else {
            /* title screen */
            time_in_game = GetTime();
            dino_pos = (Vector2){ 22, screenHeight / 2 - 12 / 2 - 47 };

            draw_dino(dino_pos, DINO_WAITING_1);

            const char* prompt = "PRESS SPACE TO START";
            const int text_w = MeasureText(prompt, font_size);
            DrawText(prompt, screenWidth / 2 - text_w / 2, screenHeight / 2 - font_size / 2, font_size, (Color) {130, 130, 130, (sinf(GetTime() * 2.0f * PI) * 0.5f + 0.5f) * 255.0f });

            if (IsKeyPressed(KEY_SPACE)) {
                last_hundred_div = 0;
                score = 0;
                in_game = true;
                lost_game = false;
                lost_time = 0.0f;

                memset(cactuses, 0, sizeof(cactuses));
                cactuses[0] = (cactus){ true, run_speed * 3.0f };
            }
        }

        /* score */
        int alpha = (lost_game && in_game) ? (fmodf(GetTime(), 0.75f) > 0.375f ? 0 : 255) : 255;

        char score_fmt[] = "00000";
        char score_str[32] = { 0 };
        sprintf(score_str, "%d", min(in_game ? score : 0, 99999));
        int score_fmt_len = strlen(score_fmt);
        int score_str_len = strlen(score_str);
        for (int i = 0; i < score_str_len; i++)
            score_fmt[score_fmt_len - score_str_len + i] = score_str[i];

        const int text_w = MeasureText(score_fmt, font_size);
        DrawText(score_fmt, screenWidth - text_w - 10, 10, font_size, (lost_game&& in_game) ? (Color) {255,0,0,alpha} : GRAY);

        if (lost_game && in_game) {
            const char* prompt = "GAME OVER";
            const int text_w = MeasureText(prompt, font_size);
            DrawText(prompt, screenWidth / 2 - text_w / 2, screenHeight / 2 - font_size / 2, font_size, (Color) { 130, 130, 130, alpha});
        }

        EndDrawing();
    }

    CloseWindow();
    
    return 0;
}