#include "libs/raylib.h"
#include <math.h>
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int Factor = 11;
const int Scope_F = Factor * 26;
const int Scope_Size = (Factor + (Factor % 3)) / 3;

#define FPS                60
#define COLOR_BG_DARK      CLITERAL(Color){18, 18, 18, 255}
#define COLOR_TEXT_PINK    CLITERAL(Color){255, 160, 216, 255}
#define VOLUME_MAX         1.2f
#define VOLUME_DEFAULT     0.5f
#define VOLUME_ADJUST_PACE 0.02f
#define INFO_DISPLAY_TIME  0.65f
#define BUFFER_SIZE        4096

#define DCEIL(DED, DOR) (DED / DOR + (DED % DOR != 0))

enum DRAW_MODE {
    DRAW_MODE_WAVE = 0,
    DRAW_MODE_FFT = 1,
    DRAW_MODE_SCOPE = 2,
};

static const char *Mode_Type_Str[] = {
    [DRAW_MODE_WAVE] = "Wave",
    [DRAW_MODE_FFT] = "FFT",
    [DRAW_MODE_SCOPE] = "Scope",
};

enum INFO_TYPE {
    INFO_NONE,
    INFO_VOLUME,
    INFO_DRAW_MODE,
};

typedef struct {
    enum DRAW_MODE draw_mode;
    enum INFO_TYPE info_type;

    float info_display_time;
    float volume;

    Music music;
    Font  font_modern_dos;

    size_t in_raw_size;
    float  in_raw[BUFFER_SIZE];
} Context;

Context *ctx = NULL;

static void audio_processor(void *bufferData, unsigned int frames);

void ctx_music_free()
{
    if (ctx == NULL) return;

    if (IsMusicValid(ctx->music)) {
        StopMusicStream(ctx->music);
        DetachAudioStreamProcessor(ctx->music.stream, audio_processor);
        UnloadMusicStream(ctx->music);
    }

    memset(ctx->in_raw, 0, sizeof(ctx->in_raw));
    ctx->in_raw_size = 0;
}

void ctx_global_free()
{
    if (ctx == NULL) return;

    ctx_music_free();
    UnloadFont(ctx->font_modern_dos);
    free(ctx);
}

int ctx_load_music(const char *path)
{
    if (path == NULL) return -1;

    ctx->music = LoadMusicStream(path);
    if (!IsMusicValid(ctx->music)) {
        fprintf(stderr, "Error: Failed to load music\n");
        return -1;
    }
    SetMusicVolume(ctx->music, ctx->volume);
    AttachAudioStreamProcessor(ctx->music.stream, audio_processor);
    ctx->music.looping = false;

    return 0;
}

int ctx_init()
{
    if (ctx != NULL) return -1;

    ctx = calloc(1, sizeof(Context));
    if (ctx == NULL) {
        fprintf(stderr, "Failed to initialize Context\n");
        return -1;
    }
    ctx->draw_mode = DRAW_MODE_WAVE;
    ctx->info_type = INFO_NONE;
    ctx->volume = VOLUME_DEFAULT;

    return 0;
}

static void audio_processor(void *bufferData, unsigned int frames)
{
    size_t append_n = (frames * 2 > BUFFER_SIZE) ? BUFFER_SIZE : frames * 2;
    size_t reserve_n = BUFFER_SIZE - append_n;

    memmove(ctx->in_raw, ctx->in_raw + append_n, reserve_n * sizeof(float));
    memcpy(ctx->in_raw + reserve_n, (float *)bufferData, append_n * sizeof(float));
}

// https://github.com/tsoding/musializer/blob/master/src/plug.c
// https://cp-algorithms.com/algebra/fft.html
static void fft(float in[], float complex out[], size_t n)
{
    for (size_t i = 0; i < n; ++i) out[i] = in[i];

    for (size_t i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;

        if (i < j) {
            float complex temp = out[i];
            out[i] = out[j];
            out[j] = temp;
        }
    }

    for (size_t len = 2; len <= n; len <<= 1) {
        float ang = 2 * PI / len;
        float complex wlen = cosf(ang) + sinf(ang)*I;

        for (size_t i = 0; i < n; i+= len) {
            float complex w = 1;
            for (size_t j = 0; j < len / 2; ++j) {
                float complex u = out[i+j], v = out[i+j+len/2] * w;
                out[i+j] = u + v;
                out[i+j+len/2] = u -v;
                w *= wlen;
            }
        }
    }
}

static void update_mv()
{
    if (IsMusicValid(ctx->music)) UpdateMusicStream(ctx->music);

    float buffer_raw[BUFFER_SIZE];
    memcpy(buffer_raw, ctx->in_raw, BUFFER_SIZE * sizeof(float));
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    const int waf = width / 2;
    const int haf = height / 2;

    if (ctx->draw_mode == DRAW_MODE_WAVE) {
        const int hof = height / 4;
        const int htf = (height / 4) * 3;

        DrawTextEx(ctx->font_modern_dos, "L", (Vector2){0.0f, 0.0f}, 32.0f, 2.0f, LIGHTGRAY);
        DrawTextEx(ctx->font_modern_dos, "R", (Vector2){0.0f, haf}, 32.0f, 2.0f, LIGHTGRAY);

        // Separator line
        DrawLineEx((Vector2){0.0f, haf}, (Vector2){width, haf}, 2, DARKBLUE);
        // Reference line of two channels
        DrawLineEx((Vector2){0.0f, hof}, (Vector2){width, hof}, 1, LIGHTGRAY);
        DrawLineEx((Vector2){0.0f, htf}, (Vector2){width, htf}, 1, LIGHTGRAY);

        const int rect_width = (width > (BUFFER_SIZE / 2)) ? DCEIL(width, (BUFFER_SIZE / 2)) : 1;
        const int buffer_range = (width > (BUFFER_SIZE / 2)) ? BUFFER_SIZE / 2 : width;
        for (int i = 0; i < buffer_range; ++i) {
            float rawL = buffer_raw[2 * i] * hof;
            float rawR = buffer_raw[2 * i + 1] * hof;
            int   ampL = (int)fabsf(rawL);
            int   ampR = (int)fabsf(rawR);
            int   yL = (rawL > 0) ? hof - ampL : hof;
            int   yR = (rawR > 0) ? htf - ampR : htf;

            DrawRectangle(i * rect_width, yL, rect_width, ampL, RAYWHITE);
            DrawRectangle(i * rect_width, yR, rect_width, ampR, RAYWHITE);
        }
    }

    if (ctx->draw_mode == DRAW_MODE_FFT) {
        // Apply the Hann window on buffer_raw
        // https://en.wikipedia.org/wiki/Hann_function
        float in_win[BUFFER_SIZE];
        for (size_t i = 0; i < BUFFER_SIZE; ++i) {
            float t = (float)i / (BUFFER_SIZE - 1);
            float hann = 0.5 - 0.5*cosf(2*PI * t);
            in_win[i] = buffer_raw[i] * hann;
        }

        float complex out_raw[BUFFER_SIZE];
        fft(in_win, out_raw, BUFFER_SIZE);
    }

    if (ctx->draw_mode == DRAW_MODE_SCOPE) {
        Vector2 tsize = MeasureTextEx(ctx->font_modern_dos, "X-L", 32.0f, 2.0f);
        DrawTextEx(ctx->font_modern_dos, "X-L", (Vector2){width - tsize.x, height - tsize.y}, 32.0f, 2.0f, LIGHTGRAY);
        DrawTextEx(ctx->font_modern_dos, "Y-R", (Vector2){0.0f, 0.0f}, 32.0f, 2.0f, LIGHTGRAY);

        for (size_t i = 0; i < BUFFER_SIZE; i += 2) {
            DrawRectangle(waf + buffer_raw[i] * Scope_F, haf - buffer_raw[i + 1] * Scope_F, Scope_Size, Scope_Size, GREEN);
        }
    }
}

static void update_welcome()
{
    const char *text_general = "Drag&Drop to play";
    const char *text_small = "(WAV, OGG, MP3, FLAC, QOA, XM, MOD)\n\n"
                             "[Space]: Stop/Start playback\n"
                             "[9/0]: Volume down / up\n"
                             "[M]: Change drawing mode";

    Vector2 general_size = MeasureTextEx(ctx->font_modern_dos, text_general, 64.0f, 2.0f);
    DrawTextEx(ctx->font_modern_dos, text_general, (Vector2){100.0f, 100.0f}, 64.0f, 2.0f, COLOR_TEXT_PINK);
    DrawTextEx(ctx->font_modern_dos, text_small, (Vector2){100.0f, ceil(100.0f + general_size.y)}, 48.0f, 2.0f, WHITE);
}

static void update_info()
{
    static char info_str[50];

    if (ctx->info_type == INFO_VOLUME) snprintf(info_str, 50, "Volume: %.0f\n", ctx->volume * 100.0f);
    if (ctx->info_type == INFO_DRAW_MODE) snprintf(info_str, 50, "Mode: %s\n", Mode_Type_Str[ctx->draw_mode]);
    DrawTextEx(ctx->font_modern_dos, info_str, (Vector2){50.0f, 30.0f}, 48.0f, 2.0f, WHITE);

    ctx->info_display_time -= GetFrameTime();
    if (ctx->info_display_time < 0.0f) {
        ctx->info_display_time = 0.0f;
        ctx->info_type = INFO_NONE;
    }
}

static void adjust_volume(KeyboardKey key)
{
    static float volume_adjust_delay = 0.0f;
    float        adjust_pace = (key == KEY_NINE) ? -VOLUME_ADJUST_PACE : VOLUME_ADJUST_PACE;

    if (IsKeyPressed(key)) {
        ctx->volume += adjust_pace;
        volume_adjust_delay = 0.3f;
    }
    else if (volume_adjust_delay > 0.0f) volume_adjust_delay -= GetFrameTime();
    else {
        ctx->volume += adjust_pace;
        volume_adjust_delay = 0.05f;
    }
    SetMusicVolume(ctx->music, ctx->volume);

    if (ctx->volume < 0.0f) ctx->volume = 0.0f;
    if (ctx->volume > VOLUME_MAX) ctx->volume = VOLUME_MAX;
    if (volume_adjust_delay < 0.0f) volume_adjust_delay = 0.0f;
    ctx->info_display_time = INFO_DISPLAY_TIME;
    ctx->info_type = INFO_VOLUME;
}

static void event_handle()
{
    if (IsFileDropped()) {
        FilePathList droppedFiles = LoadDroppedFiles();

        if (IsMusicValid(ctx->music)) ctx_music_free();
        if (droppedFiles.count > 0 && droppedFiles.paths[0] != NULL) {
            ctx_load_music(droppedFiles.paths[0]);
            UnloadDroppedFiles(droppedFiles);
        }

        PlayMusicStream(ctx->music);
    }

    if (IsKeyPressed(KEY_SPACE)) {
        if (IsMusicValid(ctx->music)) {
            if (IsMusicStreamPlaying(ctx->music)) PauseMusicStream(ctx->music);
            else ResumeMusicStream(ctx->music);
        }
    }

    if (IsKeyDown(KEY_NINE)) adjust_volume(KEY_NINE);
    if (IsKeyDown(KEY_ZERO)) adjust_volume(KEY_ZERO);

    if (IsKeyPressed(KEY_M)) {
        if (ctx->draw_mode == DRAW_MODE_SCOPE) ctx->draw_mode = DRAW_MODE_WAVE;
        else ++ctx->draw_mode;
        ctx->info_type = INFO_DRAW_MODE;
        ctx->info_display_time = INFO_DISPLAY_TIME;
    }
}

static int init()
{
    if (ctx_init() < 0) return -1;

    // https://www.dafont.com/modern-dos.font
    const char *modern_dos_path = "./resources/ModernDOS9x16.ttf";
    ctx->font_modern_dos = LoadFontEx(modern_dos_path, 16, NULL, 0);
    SetTextureFilter(ctx->font_modern_dos.texture, TEXTURE_FILTER_POINT);

    return 0;
}

int main(void)
{
    // FLAG_WINDOW_ALWAYS_RUN can't work with FLAG_VSYNC_HINT.
    // It will stop running when the window doesn't have focus and when it's minimized
    // Issue: https://github.com/raysan5/raylib/issues/5736
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(Factor * 160, Factor * 90, "Mallow.C");
    SetTargetFPS(FPS);
    InitAudioDevice();

    if (init() < 0) return 1;

    while (!WindowShouldClose()) {
        event_handle();
        BeginDrawing();
        {
            ClearBackground(COLOR_BG_DARK);
            if (IsMusicValid(ctx->music)) update_mv();
            else update_welcome();
            if (ctx->info_type != INFO_NONE && ctx->info_display_time > 0.0f) update_info();
        }
        EndDrawing();
    }

    ctx_global_free();
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
