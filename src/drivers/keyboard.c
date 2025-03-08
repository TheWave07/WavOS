#include <drivers/keyboard.h>
#include <io/screen.h>
#include <hardwarecomms/portio.h>


enum KB_ENC_IO {
    KB_ENC_INPUT_BUF = 0x60,
    KB_ENC_CMD_REG   = 0x60
};

enum KB_CTRL_IO {
    KB_CTRL_STAT_REG = 0x64,
    KB_CTRL_CMD_REG  = 0x64
};

enum KB_CTRL_STATS_MASK {
 
	KB_CTRL_STATS_MASK_OUT_BUF	=	1,		//00000001
	KB_CTRL_STATS_MASK_IN_BUF	=	2,		//00000010
	KB_CTRL_STATS_MASK_SYSTEM	=	4,		//00000100
	KB_CTRL_STATS_MASK_CMD_DATA	=	8,		//00001000
	KB_CTRL_STATS_MASK_LOCKED	=	0x10,		//00010000
	KB_CTRL_STATS_MASK_AUX_BUF	=	0x20,		//00100000
	KB_CTRL_STATS_MASK_TIMEOUT	=	0x40,		//01000000
	KB_CTRL_STATS_MASK_PARITY	=	0x80		//10000000
};

enum SPECIAL_KEYS {
    ESC = -1,
    BACKSPACE = -2,
    L_CTRL = -3,
    L_SHIFT = -4,
    R_SHIFT = -5,
    L_ALT = -6,
    CAPSLOCK = -7,
    S_BACKSPACE = -8,
};
#define KB_BUFFER_SIZE 128
//buffer for output of kb
key_packet kb_buffer[KB_BUFFER_SIZE];
size_t buffer_end = 0;

char kbdus[] = {
	0, ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', BACKSPACE,
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
	'\n', L_CTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
	L_SHIFT, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', R_SHIFT,
	'*', L_ALT, ' ', CAPSLOCK, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '7', '8', '9',
	'-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};
char kbdus_shift[] = {
	0, ESC, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', BACKSPACE,
	'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
	'\n', L_CTRL, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~',
	L_SHIFT, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', R_SHIFT,
	'*', L_ALT, ' ', CAPSLOCK, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '7', '8', '9',
	'-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};
char kbdus_caps[] = {
	0, ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', BACKSPACE,
	'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']',
	'\n', L_CTRL, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`',
	L_SHIFT, '\\', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', R_SHIFT,
	'*', L_ALT, ' ', CAPSLOCK, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '7', '8', '9',
	'-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

char kbdus_shift_caps[] = {
	0, ESC, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', BACKSPACE,
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}',
	'\n', L_CTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '\"', '~',
	L_SHIFT, '|', 'z', 'x', 'c', 'v', 'b', 'n', 'm', '<', '>', '?', R_SHIFT,
	'*', L_ALT, ' ', CAPSLOCK, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '7', '8', '9',
	'-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};
bool is_pressed_map[84] = {false};

bool _numlock, _scrolllock, _capslock;
bool _shift, _alt, _ctrl;

//read status from keyboard controller
uint8_t kb_ctrl_read_status ()
{
 
	return inb(KB_CTRL_CMD_REG);
}

//sends a command to keyboard controller
void kb_ctrl_send_cmd(uint8_t cmd)
{
    while (1)
        if ((kb_ctrl_read_status() & KB_CTRL_STATS_MASK_IN_BUF) == 0)
            break;
    outb(KB_CTRL_CMD_REG, cmd);
}

uint8_t kb_enc_read_buf () {
 
	return inb(KB_ENC_INPUT_BUF);
}

//sends a command to keyboard encoder
void kb_enc_send_cmd(uint8_t cmd)
{
    while (1)
        if ((kb_ctrl_read_status() & KB_CTRL_STATS_MASK_IN_BUF) == 0)
            break;
    outb(KB_ENC_CMD_REG, cmd);
}
bool _handle_irq = false;
/// @brief fetches the first item from buffer
/// @return the item
key_packet kb_fetch()
{
    //waits for buffer to fill up
    while(buffer_end == 0);
    _handle_irq = false;
    key_packet out = kb_buffer[0];
    for (size_t i = 0; i < buffer_end; i++)
    {
        kb_buffer[i] = kb_buffer[i+1];
    }
    buffer_end--;
    _handle_irq = true;
    return out;
}

/// @brief adds a char to keyboard buffer
/// @param c  the char
void add_to_buffer(key_packet p)
{
    
    if(buffer_end < KB_BUFFER_SIZE)
    {
        kb_buffer[buffer_end] = p;
    } else {
        kb_fetch();
        kb_buffer[buffer_end] = p;
    }
    buffer_end++;
}

void key_pressed(uint32_t scan) 
{
    char *key_map = kbdus;
    if (_capslock)
        key_map = kbdus_caps;
    if (_shift)
        key_map = _capslock ? kbdus_shift_caps : kbdus_shift;
    if(key_map[scan]) {
        key_packet packet;
        is_pressed_map[scan] = true;
        //printable char
        if(key_map[scan] > 0)
            packet.printable = true;
        else
            packet.printable = false;
        //shift
        if(key_map[scan] == L_SHIFT || key_map[scan] == R_SHIFT )
            _shift = true;
        //alt
        if(key_map[scan] == L_ALT)
            _alt = true;
        //ctrl
        if(key_map[scan] == L_CTRL)
            _ctrl = true;
        //capslock
        if(key_map[scan] == CAPSLOCK)
            _capslock = !_capslock;

        packet.shift = _shift;
        packet.alt = _alt;
        packet.capslock = _capslock;
        packet.ctrl = _ctrl;
        packet.value = key_map[scan];

        add_to_buffer(packet);
    }
}

void key_released(uint32_t scan) 
{
    char *key_map = kbdus;
    if (_capslock)
        key_map = kbdus_caps;
    if (_shift)
        key_map = _capslock ? kbdus_shift_caps : kbdus_shift;
    if(key_map[scan]){
        is_pressed_map[scan] = false;
        //shift
        if(key_map[scan] == L_SHIFT || key_map[scan] == R_SHIFT )
            _shift = false;
        //alt
        if(key_map[scan] == L_ALT)
            _alt = false;
        //ctrl
        if(key_map[scan] == L_CTRL)
            _ctrl = false;
    }
}

void keyboard_input(void)
{
    uint32_t scan;
    if (!_handle_irq)
        return;
    scan = kb_enc_read_buf();
    if (scan < 0x81) {
        key_pressed(scan);
    } else if(scan < 0xD8) {
        key_released(scan - 0x80);
    }
}

//tests keyboard
bool kb_self_test() {
    //disable_interrupts();
    _handle_irq = false;
	//! send command
	kb_ctrl_send_cmd(0xAA);
	//! wait for output buffer to be full
	while (1)
		if (kb_ctrl_read_status () & KB_CTRL_STATS_MASK_OUT_BUF)
			break;
    bool is_ok = (kb_enc_read_buf() == 0x55) ? true : false;
    _handle_irq = true;
    //enable_interrupts();
	//! if output buffer == 0x55, test passed
	return is_ok;
}

void kb_init(void)
{
    //! set lock keys and led lights
	_numlock = _scrolllock = _capslock = false;

	//! shift, ctrl, and alt keys
	_shift = _alt = _ctrl = false;
    _handle_irq = true;
}
