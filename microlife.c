/*
  nostdlib / nostartfiles implementation of Game of Life
  it also uses packed bitarray for representing state of the game, which is hacky, but that's the point :)
  for now implementation is strictly built against mingw/windows, but it shouldn't be hard to port to other systems and compilers too
  could use UPX for stripping few extra KB
*/

// todo: loading of 'images' of starting states
// todo: retrieve clock time to implement 'slowed' execution
// todo: ways to modify speed
// todo: edit mode (by toggling single cell under cursor that moves by arrow keys)

#include "nocrt0/nocrt0c.c"

typedef void*         HANDLE;
typedef const void*   LPCVOID;
typedef unsigned long DWORD;
typedef DWORD*        LPDWORD;
typedef int           BOOL;

// overlapped structure is left as void* as we will not use it in any way as console output couldn't be async
extern BOOL   __stdcall WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, void* lpOverlapped);
extern BOOL   __stdcall ReadFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, void* lpOverlapped);
extern HANDLE __stdcall GetStdHandle(DWORD nStdHandle);
extern int getchar(void);

#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)

#if _WIN64
typedef unsigned long long mask_type;
#elif _WIN32
typedef unsigned int mask_type;
#else
#error "currently only windows supported"
#endif
#define MASK_SIZE   (sizeof(mask_type) * 8U)

#define BOARD_DIM   32U
#define BOARD_AREA  (BOARD_DIM * BOARD_DIM)
#define BOARD_MASKS (BOARD_AREA / MASK_SIZE)

_Static_assert(MASK_SIZE % BOARD_DIM == 0 && BOARD_DIM != 0, "invalid board dimensions");
_Static_assert(BOARD_MASKS != 0, "invalid board dimensions");

static const char FLUSH_PRELUDE[] = "\e[1;1H\e[2J\e[?25l"; // clear screen, set cursor at (0, 0), hide curesor

static const char COMMANDS_INFO[] =
"r : shuffle board randomly\n"
"g : put glider at the center of the screen\n"
"_ : step simulation\n";

static unsigned long shuffle_seed = 12937171UL;

static mask_type global_board[BOARD_MASKS];

#define ALIVE_CELL_CHAR '#'
#define DEAD_CELL_CHAR  '.'


static _Bool get_value_at(mask_type board[], unsigned int x, unsigned int y) {
  unsigned int idx = x + y * BOARD_DIM;
  return (_Bool)(board[idx / MASK_SIZE] & ((mask_type)1 << (idx % MASK_SIZE)));
}


static void set_value_at(mask_type board[], unsigned int x, unsigned int y, _Bool value) {
  unsigned int idx = x + y * BOARD_DIM;
  if (value) {
    board[idx / MASK_SIZE] |= ((mask_type)1 << (idx % MASK_SIZE));
  } else {
    board[idx / MASK_SIZE] &= ~((mask_type)1 << (idx % MASK_SIZE));
  }
}


static _Bool random_bool(void) {
  // xorshift algo
  shuffle_seed ^= shuffle_seed << 13;
  shuffle_seed ^= shuffle_seed >> 17;
  shuffle_seed ^= shuffle_seed << 5;
  return (_Bool)(shuffle_seed & 0b1);
}


static void flush_command_info(void) {
  DWORD chars_read;
  (void)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), COMMANDS_INFO, sizeof(COMMANDS_INFO) - 1U, &chars_read, NULL);
  (void)chars_read;
}

static void board_flush(void) {
  char string_buffer[BOARD_AREA + BOARD_DIM + sizeof(FLUSH_PRELUDE) - 1U];
  // todo: it's probably better to write prelude independently, no real reason to offset by it all the time
  for (unsigned long long n = 0ULL; n < sizeof(FLUSH_PRELUDE) - 1ULL; n++)
    string_buffer[n] = FLUSH_PRELUDE[n];

  unsigned int n = 0U;
  unsigned int newlines = 0U;
  while (n != BOARD_AREA) {
    string_buffer[sizeof(FLUSH_PRELUDE) - 1U + n + newlines] =
      get_value_at(global_board, n % BOARD_DIM, n / BOARD_DIM) ? ALIVE_CELL_CHAR : DEAD_CELL_CHAR;
    n++;
    if (n % BOARD_DIM == 0U) {
      string_buffer[sizeof(FLUSH_PRELUDE) - 1U + n + newlines] = '\n';
      newlines++;
    }
  }
  DWORD chars_read;
  (void)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), string_buffer, BOARD_AREA + BOARD_DIM + sizeof(FLUSH_PRELUDE) - 1U, &chars_read, NULL);
  (void)chars_read;
  flush_command_info();
}


static void board_update(void) {
  mask_type buffer_board[BOARD_MASKS];
  for (unsigned int y = BOARD_DIM; y--;) {
    for (unsigned int x = BOARD_DIM; x--;) {
      // find neighboring cells
      unsigned int neighbors = 0U;
      if (get_value_at(global_board, (x + 1U) % BOARD_DIM, y))
        neighbors++;
      if (get_value_at(global_board, (x + BOARD_DIM - 1U) % BOARD_DIM, y))
        neighbors++;
      if (get_value_at(global_board, x, (y + 1U) % BOARD_DIM))
        neighbors++;
      if (get_value_at(global_board, x, (y + BOARD_DIM - 1U) % BOARD_DIM))
        neighbors++;
      if (get_value_at(global_board, (x + 1U) % BOARD_DIM, (y + 1U) % BOARD_DIM))
        neighbors++;
      if (get_value_at(global_board, (x + 1U) % BOARD_DIM, (y + BOARD_DIM - 1U) % BOARD_DIM))
        neighbors++;
      if (get_value_at(global_board, (x + BOARD_DIM - 1U) % BOARD_DIM, (y + 1U) % BOARD_DIM))
        neighbors++;
      if (get_value_at(global_board, (x + BOARD_DIM - 1U) % BOARD_DIM, (y + BOARD_DIM - 1U) % BOARD_DIM))
        neighbors++;

      // update state
      _Bool is_alive = get_value_at(global_board, x, y);
      if (!is_alive && (neighbors == 3U)) {
        set_value_at(buffer_board, x, y, (_Bool)1);
      } else if (is_alive && ((neighbors < 2U) || (neighbors > 3U))) {
        set_value_at(buffer_board, x, y, (_Bool)0);
      } else {
        set_value_at(buffer_board, x, y, is_alive);
      }
    }
  }
  for (unsigned int i = BOARD_MASKS; i--;) {
    global_board[i] = buffer_board[i];
  }
}


static void board_shuffle(void) {
  for (unsigned int y = BOARD_DIM; y--;) {
    for (unsigned int x = BOARD_DIM; x--;) {
      set_value_at(global_board, x, y, random_bool());
    }
  }
}


static void put_glider(unsigned int x, unsigned y) {
  if (x >= BOARD_DIM - 3U || y >= BOARD_DIM - 3U)
    return; // todo: should panic, probably
  set_value_at(global_board, x,      y,      (_Bool)1);
  set_value_at(global_board, x + 1U, y + 1U, (_Bool)1);
  set_value_at(global_board, x + 2U, y + 1U, (_Bool)1);
  set_value_at(global_board, x,      y + 2U, (_Bool)1);
  set_value_at(global_board, x + 1U, y + 2U, (_Bool)1);
}


static char read_char(void) {
  char ch;
  DWORD chars_read;
  BOOL st = ReadFile(GetStdHandle(STD_INPUT_HANDLE), &ch, (DWORD)1, &chars_read, NULL);
  if ((st != (BOOL)0) && (chars_read == (DWORD)1)) {
    return ch;
  }
  return '\0';
}


int main(int argc, char** argv, char** envp) {
  // theoretically we could strip handling of arguments completely, but they could be needed in the future, dunno
  // todo: offset seed by inputed characters?
  (void)argc;
  (void)argv;
  (void)envp;
  while (1) {
    board_flush();
    switch (read_char()) {
      case 'r':
        board_shuffle();
        break;
      case 'g':
        put_glider(BOARD_DIM / 2U, BOARD_DIM / 2U);
        break;
      default:
        board_update();
    }
  }
}
