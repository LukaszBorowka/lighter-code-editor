#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include <termios.h>  // raw mode
#include <unistd.h>   // read/write
#include <stdlib.h>   // atexit, exit
#include <stdio.h>    // snprintf, printf
#include <signal.h>   // SIGWINCH, signal()
#include <sys/ioctl.h> // ioctl, TIOCGWINSZ
#include <string.h>   // strlen

// Global state
struct termios orig_termios;
int screen_rows, screen_cols;
int cursor_x = 1, cursor_y = 1;

// Terminal control
void disable_raw_mode()
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
	write(STDOUT_FILENO, "\x1b[?25h", 6); // show cursor
	write(STDOUT_FILENO, "\x1b[0m", 4);   // reset colors
	write(STDOUT_FILENO, "\x1b[2J", 4);   // clear creen
	write(STDOUT_FILENO, "\x1b[H", 3);    // move home
}

void enable_raw_mode()
{
	tcgetattr(STDIN_FILENO, &orig_termios);
	atexit(disable_raw_mode);

	struct termios raw = orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON | ISIG); // raw mode
	raw.c_iflag &= ~(IXON | ICRNL );        // no ctrl+s/q, no CR->NL
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

	write(STDOUT_FILENO, "\x1b[?25l", 6);   // hide cursor
}

void get_window_size()
{
	struct winsize ws;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	screen_rows = ws.ws_row;
	screen_cols = ws.ws_col;
}

void handle_winch(int sig)
{
	(void)sig;
	get_window_size();
}

void clear_screen()
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
}

void draw_screen()
{
	clear_screen();
	for (int y = 1; y <= screen_rows; y++) {
        for (int x = 1; x <= screen_cols; x++) {
            if (x == cursor_x && y == cursor_y) {
                write(STDOUT_FILENO, "\x1b[38;2;0;0;0m", 13);    // black foreground
				write(STDOUT_FILENO, "\x1b[48;2;200;200;50m", 19); // yellow background
				write(STDOUT_FILENO, "█", 3);                     // full block character
				write(STDOUT_FILENO, "\x1b[0m", 4);              // reset
            } else {
                write(STDOUT_FILENO, ".", 1);
            }
        }
        write(STDOUT_FILENO, "\r\n", 2);
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", cursor_y, cursor_x);
    write(STDOUT_FILENO, buf, strlen(buf));
}

// --- Input handling ---
int read_key() {
    char c;
    if (read(STDIN_FILENO, &c, 1) == -1) return -1;
    if (c == '\x1b') { // escape sequence
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) == 0) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) == 0) return '\x1b';
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return 1000; // up
                case 'B': return 1001; // down
                case 'C': return 1002; // right
                case 'D': return 1003; // left
            }
        }
        return '\x1b';
    }
    return c;
}

// --- Main ---
int main() {
    enable_raw_mode();
    get_window_size();
    signal(SIGWINCH, handle_winch);

    while (1) {
        draw_screen();
        int key = read_key();
        if (key == -1) continue;

        if (key == ('q' & 0x1f)) break; // Ctrl-Q exits

        switch (key) {
            case 1000: if (cursor_y > 1) cursor_y--; break; // up
            case 1001: if (cursor_y < screen_rows) cursor_y++; break; // down
            case 1002: if (cursor_x < screen_cols) cursor_x++; break; // right
            case 1003: if (cursor_x > 1) cursor_x--; break; // left
        }
    }
    return 0;
}