#pragma once

#include "global/types.h"

#include <stdbool.h>
#include <stdint.h>

typedef int16_t S_INPUT_KEYCODE;

void S_Input_Init(void);
void S_Input_Shutdown(void);

INPUT_STATE S_Input_GetCurrentState(void);

S_INPUT_KEYCODE S_Input_ReadKeyCode(void);

const char *S_Input_GetKeyCodeName(S_INPUT_KEYCODE key);

bool S_Input_IsKeyConflicted(INPUT_ROLE role);
void S_Input_SetKeyAsConflicted(INPUT_ROLE role, bool is_conflicted);

S_INPUT_KEYCODE S_Input_GetAssignedKeyCode(int16_t layout_num, INPUT_ROLE role);
void S_Input_AssignKeyCode(
    int16_t layout_num, INPUT_ROLE role, S_INPUT_KEYCODE key_code);
