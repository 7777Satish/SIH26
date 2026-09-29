#include "gui/gui.h"
#include <stdio.h>

static float mouse_x;
static float mouse_y;
static bool mouse_down;
static bool previous_mouse_down;

static bool inside(int x, int y, int width, int height)
{
    return mouse_x >= x && mouse_x <= x + width && mouse_y >= y && mouse_y <= y + height;
}

void GUI_Begin(SDL_Renderer *renderer)
{
    float window_x;
    float window_y;
    previous_mouse_down = mouse_down;
    mouse_down = (SDL_GetMouseState(&window_x, &window_y) & SDL_BUTTON_LMASK) != 0;
    SDL_RenderCoordinatesFromWindow(renderer, window_x, window_y, &mouse_x, &mouse_y);
}

void GUI_SetColor(SDL_Renderer *renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void GUI_Label(SDL_Renderer *renderer, int x, int y, const char *text)
{
    GUI_SetColor(renderer, (SDL_Color){ 178, 193, 208, 255 });
    SDL_RenderDebugText(renderer, (float)x, (float)y, text);
}

bool GUI_Button(SDL_Renderer *renderer, int x, int y, int width, int height, const char *label)
{
    return GUI_ChoiceButton(renderer, x, y, width, height, label, false);
}

bool GUI_ChoiceButton(SDL_Renderer *renderer, int x, int y, int width, int height, const char *label, bool active)
{
    const bool hover = inside(x, y, width, height);
    const SDL_Color fill = active ? (SDL_Color){ 31, 91, 99, 255 } : (hover ? (SDL_Color){ 40, 104, 126, 255 } : (SDL_Color){ 27, 47, 61, 255 });
    const SDL_Color border = active ? (SDL_Color){ 255, 195, 80, 255 } : (hover ? (SDL_Color){ 117, 225, 218, 255 } : (SDL_Color){ 65, 101, 117, 255 });
    GUI_SetColor(renderer, fill);
    SDL_RenderFillRect(renderer, &(SDL_FRect){ (float)x, (float)y, (float)width, (float)height });
    GUI_SetColor(renderer, border);
    SDL_RenderRect(renderer, &(SDL_FRect){ (float)x, (float)y, (float)width, (float)height });
    GUI_Label(renderer, x + 10, y + 8, label);
    return hover && mouse_down && !previous_mouse_down;
}

bool GUI_Slider(SDL_Renderer *renderer, int x, int y, int width, const char *label, float *value, float min, float max)
{
    char value_text[64];
    const bool hover = inside(x, y, width, 24);
    if (hover && mouse_down) {
        float ratio = (mouse_x - x) / (float)width;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        *value = min + ratio * (max - min);
    }
    GUI_Label(renderer, x, y, label);
    SDL_FRect track = { (float)x, (float)(y + 20), (float)width, 3.0f };
    GUI_SetColor(renderer, (SDL_Color){ 35, 59, 72, 255 });
    SDL_RenderFillRect(renderer, &track);
    const float ratio = (*value - min) / (max - min);
    SDL_FRect knob = { (float)(x + ratio * width - 4), (float)(y + 16), 8.0f, 11.0f };
    GUI_SetColor(renderer, (SDL_Color){ 117, 225, 218, 255 });
    SDL_RenderFillRect(renderer, &knob);
    snprintf(value_text, sizeof(value_text), "%.1f", *value);
    GUI_Label(renderer, x + width + 12, y, value_text);
    return hover && mouse_down;
}

bool GUI_Toggle(SDL_Renderer *renderer, int x, int y, const char *label, bool *value)
{
    const bool clicked = GUI_Button(renderer, x, y, 18, 18, *value ? "ON" : "  ");
    if (clicked) *value = !*value;
    GUI_Label(renderer, x + 28, y + 3, label);
    return clicked;
}
