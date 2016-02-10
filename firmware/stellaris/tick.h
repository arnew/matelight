#pragma once


extern volatile unsigned long g_ulSysTickCount;
extern volatile unsigned long last_frame;

void SysTickIntHandler(void);
