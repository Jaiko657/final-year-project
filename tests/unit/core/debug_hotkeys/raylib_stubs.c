#include "raylib.h"

static int g_screen_w = 800;
static int g_screen_h = 600;

void raylib_debug_stub_reset(void)
{
    g_screen_w = 800;
    g_screen_h = 600;
}

void raylib_debug_stub_set_screen(int w, int h)
{
    g_screen_w = w;
    g_screen_h = h;
}

int GetScreenWidth(void)
{
    return g_screen_w;
}

int GetScreenHeight(void)
{
    return g_screen_h;
}

Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera)
{
    (void)camera;
    return position;
}

bool IsKeyPressed(int key)
{
    (void)key;
    return false;
}

bool DirectoryExists(const char* dir)
{
    (void)dir;
    return true;
}

bool MakeDirectory(const char* dir)
{
    (void)dir;
    return true;
}

bool FileExists(const char* fileName)
{
    (void)fileName;
    return false;
}

void TakeScreenshot(const char* fileName)
{
    (void)fileName;
}
