#pragma once

void CS16Steam_Init(void);
void CS16Steam_Frame(double time);
void CS16Steam_Shutdown(void);
void CS16Steam_QueueAvatar(int playerIndex, unsigned long long steamId,
    int x, int y, int size, int alpha, float time);
