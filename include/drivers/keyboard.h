#ifndef __WAVOS__DRIVERS__KEYBOARD_H
#define __WAVOS__DRIVERS__KEYBOARD_H
#include <common/types.h>

typedef struct key_packet{
    bool shift;
    bool ctrl;
    bool capslock;
    bool alt;
    bool printable;
    char value;
} key_packet;


void kb_init(void);
void keyboard_input(void);
bool kb_self_test(void);
key_packet kb_fetch(void);
#endif