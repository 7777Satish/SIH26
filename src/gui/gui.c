#include "gui/gui.h"
#include <math.h>
#include <stdio.h>

static float mouse_x;
static float mouse_y;
static bool mouse_down;
static bool previous_mouse_down;
static TTF_Font *body_font;
static TTF_Font *label_font;
static int open_dropdown = -1;
static bool dropdown_visible;
static int dropdown_x;
static int dropdown_y;
static int dropdown_width;
static int dropdown_height;
static const char *const *dropdown_options;
static int dropdown_option_count;
static int *dropdown_selected;

static void rounded_fill(SDL_Renderer *renderer, float x, float y, float width, float height, float radius, SDL_Color color)
{
    GUI_SetColor(renderer, color);
    const int end = (int)height;
    for (int row = 0; row < end; ++row) {
        const float edge = row < radius ? radius - row : (row >= end - radius ? row - (end - radius - 1) : 0.0f);
        const float inset = edge > 0.0f ? radius - sqrtf(radius * radius - edge * edge) : 0.0f;
        SDL_RenderLine(renderer, x + inset, y + row, x + width - inset, y + row);
    }
}

static void rounded_outline(SDL_Renderer *renderer, float x, float y, float width, float height, float radius, SDL_Color color)
{
    GUI_SetColor(renderer, color);
    SDL_FPoint points[37];
    const float pi = 3.14159265359f;
    int point_count = 0;
    const float centers_x[] = { x + width - radius, x + width - radius, x + radius, x + radius };
    const float centers_y[] = { y + radius, y + height - radius, y + height - radius, y + radius };
    const float start_angles[] = { -pi * 0.5f, 0.0f, pi * 0.5f, pi };
    for (int corner = 0; corner < 4; ++corner) {
        for (int step = 0; step <= 8; ++step) {
            const float angle = start_angles[corner] + (pi * 0.5f * step / 8.0f);
            points[point_count++] = (SDL_FPoint){ centers_x[corner] + cosf(angle) * radius, centers_y[corner] + sinf(angle) * radius };
        }
    }
    points[point_count] = points[0];
    SDL_RenderLines(renderer, points, point_count + 1);
}

static void centered_label(SDL_Renderer *renderer, int x, int y, int width, int height, const char *text, SDL_Color color)
{
    if (body_font == NULL || text == NULL) return;
    int text_width;
    int text_height;
    if (!TTF_GetStringSize(body_font, text, 0, &text_width, &text_height)) return;
    SDL_Surface *surface = TTF_RenderText_Blended(body_font, text, 0, color);
    if (surface == NULL) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture != NULL) {
        const int content_width = width - 16;
        const float scale = text_width > content_width ? (float)content_width / (float)text_width : 1.0f;
        const float drawn_width = surface->w * scale;
        const float drawn_height = surface->h * scale;
        SDL_RenderTexture(renderer, texture, NULL, &(SDL_FRect){ x + (width - drawn_width) * 0.5f, y + (height - drawn_height) * 0.5f, drawn_width, drawn_height });
        SDL_DestroyTexture(texture);
    }
    SDL_DestroySurface(surface);
}

static bool inside(int x, int y, int width, int height)
{
    return mouse_x >= x && mouse_x <= x + width && mouse_y >= y && mouse_y <= y + height;
}

static bool label_inside(int x, int y, const char *label)
{
    int width;
    int height;
    return body_font != NULL && TTF_GetStringSize(body_font, label, 0, &width, &height) && inside(x, y, width, height + 4);
}

void GUI_Begin(SDL_Renderer *renderer)
{
    float window_x;
    float window_y;
    previous_mouse_down = mouse_down;
    mouse_down = (SDL_GetMouseState(&window_x, &window_y) & SDL_BUTTON_LMASK) != 0;
    SDL_RenderCoordinatesFromWindow(renderer, window_x, window_y, &mouse_x, &mouse_y);
    dropdown_visible = false;
}

void GUI_SetColor(SDL_Renderer *renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void GUI_SetFonts(TTF_Font *new_body_font, TTF_Font *new_label_font)
{
    body_font = new_body_font;
    label_font = new_label_font;
}

void GUI_Label(SDL_Renderer *renderer, int x, int y, const char *text)
{
    if (body_font == NULL || text == NULL || text[0] == '\0') return;
    SDL_Surface *surface = TTF_RenderText_Blended(body_font, text, 0, (SDL_Color){ 206, 187, 193, 255 });
    if (surface == NULL) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture != NULL) {
        SDL_RenderTexture(renderer, texture, NULL, &(SDL_FRect){ (float)x, (float)y, (float)surface->w, (float)surface->h });
        SDL_DestroyTexture(texture);
    }
    SDL_DestroySurface(surface);
}

bool GUI_Button(SDL_Renderer *renderer, int x, int y, int width, int height, const char *label)
{
    return GUI_ChoiceButton(renderer, x, y, width, height, label, false);
}

bool GUI_ChoiceButton(SDL_Renderer *renderer, int x, int y, int width, int height, const char *label, bool active)
{
    const int padded_height = height + 4;
    const bool hover = inside(x, y, width, padded_height);
    const SDL_Color fill = active ? (SDL_Color){ 82, 73, 39, 255 } : (hover ? (SDL_Color){ 48, 27, 37, 255 } : (SDL_Color){ 38, 20, 29, 255 });
    const SDL_Color border = active ? (SDL_Color){ 117, 126, 44, 255 } : (hover ? (SDL_Color){ 93, 49, 65, 255 } : (SDL_Color){ 55, 31, 41, 255 });
    rounded_fill(renderer, (float)x, (float)y, (float)width, (float)padded_height, 6.0f, fill);
    rounded_outline(renderer, (float)x, (float)y, (float)width, (float)padded_height, 6.0f, border);
    centered_label(renderer, x, y, width, padded_height, label, active ? (SDL_Color){ 207, 238, 75, 255 } : (SDL_Color){ 206, 187, 193, 255 });
    return hover && mouse_down && !previous_mouse_down;
}

bool GUI_Dropdown(SDL_Renderer *renderer, int x, int y, int width, int height, const char *value, const char *const options[], int option_count, int *selected, int id)
{
    const int padded_height = height + 4;
    const bool field_hover = inside(x, y, width, padded_height);
    const bool field_clicked = field_hover && mouse_down && !previous_mouse_down;
    if (field_clicked) open_dropdown = open_dropdown == id ? -1 : id;
    rounded_fill(renderer, (float)x, (float)y, (float)width, (float)padded_height, 6.0f, (SDL_Color){ 38, 20, 29, 255 });
    rounded_outline(renderer, (float)x, (float)y, (float)width, (float)padded_height, 6.0f, field_hover ? (SDL_Color){ 142, 124, 62, 255 } : (SDL_Color){ 69, 38, 51, 255 });
    centered_label(renderer, x + 8, y + 2, width - 16, padded_height - 4, value, (SDL_Color){ 206, 187, 193, 255 });
    if (open_dropdown != id) return field_clicked;
    dropdown_visible = true;
    dropdown_x = x;
    dropdown_y = y;
    dropdown_width = width;
    dropdown_height = padded_height;
    dropdown_options = options;
    dropdown_option_count = option_count;
    dropdown_selected = selected;
    const int option_height = padded_height;
    for (int option = 0; option < option_count; ++option) {
        const int option_y = y + padded_height + option * option_height;
        const bool option_hover = inside(x, option_y, width, option_height);
        if (option_hover && mouse_down && !previous_mouse_down) {
            *selected = option;
            open_dropdown = -1;
            dropdown_visible = false;
        }
    }
    return field_clicked;
}

void GUI_DrawDropdownOverlay(SDL_Renderer *renderer)
{
    if (!dropdown_visible || dropdown_options == NULL) return;
    rounded_fill(renderer, (float)dropdown_x, (float)(dropdown_y + dropdown_height), (float)dropdown_width, (float)(dropdown_height * dropdown_option_count), 6.0f, (SDL_Color){ 47, 25, 35, 255 });
    rounded_outline(renderer, (float)dropdown_x, (float)(dropdown_y + dropdown_height), (float)dropdown_width, (float)(dropdown_height * dropdown_option_count), 6.0f, (SDL_Color){ 93, 49, 65, 255 });
    for (int option = 0; option < dropdown_option_count; ++option) {
        const int option_y = dropdown_y + dropdown_height + option * dropdown_height;
        if (inside(dropdown_x, option_y, dropdown_width, dropdown_height)) {
            GUI_SetColor(renderer, (SDL_Color){ 82, 73, 39, 255 });
            SDL_RenderFillRect(renderer, &(SDL_FRect){ (float)dropdown_x, (float)option_y, (float)dropdown_width, (float)dropdown_height });
        }
        GUI_Label(renderer, dropdown_x + 10, option_y + 8, dropdown_options[option]);
    }
}

bool GUI_Slider(SDL_Renderer *renderer, int x, int y, int width, const char *label, float *value, float min, float max)
{
    char value_text[64];
    const bool hover = inside(x, y, width, 34);
    if (hover && mouse_down) {
        float ratio = (mouse_x - x) / (float)width;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        *value = min + ratio * (max - min);
    }
    GUI_Label(renderer, x, y, label);
    SDL_FRect track = { (float)x, (float)(y + 27), (float)width, 4.0f };
    rounded_fill(renderer, track.x, track.y, track.w, track.h, 2.0f, (SDL_Color){ 69, 38, 51, 255 });
    const float ratio = (*value - min) / (max - min);
    rounded_fill(renderer, track.x, track.y, track.w * ratio, track.h, 2.0f, (SDL_Color){ 117, 126, 44, 255 });
    rounded_fill(renderer, (float)(x + ratio * width - 6), (float)(y + 23), 12.0f, 12.0f, 6.0f, (SDL_Color){ 207, 238, 75, 255 });
    snprintf(value_text, sizeof(value_text), "%.1f", *value);
    centered_label(renderer, x + width - 50, y, 50, 18, value_text, (SDL_Color){ 207, 238, 75, 255 });
    return hover && mouse_down;
}

bool GUI_Toggle(SDL_Renderer *renderer, int x, int y, const char *label, bool *value)
{
    const bool hover = inside(x, y, 38, 24) || label_inside(x + 48, y + 5, label);
    const bool clicked = hover && mouse_down && !previous_mouse_down;
    if (clicked) *value = !*value;
    rounded_fill(renderer, (float)x, (float)y, 38.0f, 24.0f, 9.0f, *value ? (SDL_Color){ 82, 73, 39, 255 } : (SDL_Color){ 38, 20, 29, 255 });
    rounded_outline(renderer, (float)x, (float)y, 38.0f, 24.0f, 9.0f, *value ? (SDL_Color){ 117, 126, 44, 255 } : (SDL_Color){ 69, 38, 51, 255 });
    rounded_fill(renderer, (float)(*value ? x + 21 : x + 4), (float)y + 5.0f, 13.0f, 14.0f, 7.0f, *value ? (SDL_Color){ 207, 238, 75, 255 } : (SDL_Color){ 130, 101, 112, 255 });
    GUI_Label(renderer, x + 48, y + 5, label);
    return clicked;
}

bool GUI_Checkbox(SDL_Renderer *renderer, int x, int y, const char *label, bool *value)
{
    const bool hover = inside(x, y, 14, 14) || label_inside(x + 19, y - 1, label);
    const bool clicked = hover && mouse_down && !previous_mouse_down;
    if (clicked) *value = !*value;
    rounded_fill(renderer, (float)x, (float)y, 13.0f, 13.0f, 3.0f, *value ? (SDL_Color){ 207, 238, 75, 255 } : (SDL_Color){ 38, 20, 29, 255 });
    rounded_outline(renderer, (float)x, (float)y, 13.0f, 13.0f, 3.0f, *value ? (SDL_Color){ 207, 238, 75, 255 } : (SDL_Color){ 69, 38, 51, 255 });
    if (*value) GUI_Label(renderer, x + 2, y - 1, "x");
    GUI_Label(renderer, x + 19, y - 1, label);
    return clicked;
}
