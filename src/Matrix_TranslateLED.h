/*
    For Durability,
    Translate LED in Panel
    x: -1~1
    y: -1~1
*/
#include "Arduino.h"
#define PROGMEM

const int8_t Matrix_TranslateLED[][2] PROGMEM = {
    {0, 0},
    {1, 0},
    {1, -1},
    {0, -1},
    {-1, -1},
    {-1, 0},
    {-1, 1},
    {0, 1},
    {1, 1}};