#ifndef GUI_H
#define GUI_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdbool.h>
#include "types/types.h"

void GUI_Begin(SDL_Renderer *renderer);
bool GUI_Button(SDL_Renderer *renderer, int x, int y, int width, int height, const char *label);
bool GUI_ChoiceButton(SDL_Renderer *renderer, int x, int y, int width, int height, const char *label, bool active);
bool GUI_Dropdown(SDL_Renderer *renderer, int x, int y, int width, int height, const char *value, const char *const options[], int option_count, int *selected, int id);
bool GUI_Slider(SDL_Renderer *renderer, int x, int y, int width, const char *label, float *value, float min, float max);
bool GUI_Toggle(SDL_Renderer *renderer, int x, int y, const char *label, bool *value);
bool GUI_Checkbox(SDL_Renderer *renderer, int x, int y, const char *label, bool *value);
void GUI_DrawDropdownOverlay(SDL_Renderer *renderer);
void GUI_Label(SDL_Renderer *renderer, int x, int y, const char *text);
void GUI_SetColor(SDL_Renderer *renderer, SDL_Color color);
void GUI_SetFonts(TTF_Font *body_font, TTF_Font *label_font);

#endif
