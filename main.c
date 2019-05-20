#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>

#define VTResetScreen "c"
#define VTCursorDown "[B"
#define VTCursorUp "[A"
#define VTCursorRight "[C"
#define VTCursorLeft "[D"

#define EnterGraphicsMode 14
#define ExitGraphicsMode 15

#define UpArrow 0
#define DownArrow 1
#define RightArrow 2
#define LeftArrow 3

#define vtCursorXY(X, Y) (printf("\e[%d;%df", Y, X))
#define putgchar(C) (printf("%c%c%c", EnterGraphicsMode, C, ExitGraphicsMode))

// Config File Reader
typedef struct KVPair {
  char* key;
  size_t keysize;
  char* val;
  size_t valsize;
  FILE* file;
} KVPair;

KVPair loadConfig() { 
  char* confDir;
  char prefix[9];
  prefix[0] = '\0';
  if ( getenv("XDG_CONFIG_HOME") != NULL )
   confDir = getenv("XDG_CONFIG_HOME");
  else {
    confDir = getenv("HOME");
    strcpy(prefix, "/.config");
  }
  char* configLocation = malloc(strlen(confDir) + strlen(prefix) + strlen("/tdm.conf") + 1);
  sprintf(configLocation, "%s%s/tdm.conf", confDir, prefix);
  KVPair config = (KVPair){malloc(1), 1, malloc(1), 1, fopen(configLocation, "r")};

  // If the file is not found, throw an error and exit
  if (config.file == NULL) {
    printf("Cannot locate config file at '%s'\n", configLocation);
    exit(EXIT_FAILURE);
  }
  free(configLocation);
  return config;
}

char* stripstr(char* str, int direction, char delimeter) {
  size_t len = strlen(str);
  int index = direction ? 0 : len - 1;
  while (0 <= index && index <= len && str[index] == delimeter)
    direction ? index++ : index--;
  
  if (!direction)
    str[index + 1] = '\0';
  else
    memmove(str, str + index, len - index + 1);
  return str;
}

int nextPair(KVPair* pair) {
  char* line = NULL;
  size_t len = 0;
  // Find a line that isn't a comment or blank line until we hit the end of the file
  while (line == NULL || line[0] == '#' || line[0] == '\n')
    if (getline(&line, &len, pair->file) == -1) {
      fclose(pair->file);
      pair->file = NULL;
      free(line);
      return 0;
    }
  len = strlen(line);
  //  Remove newline from getline
  if (line[len-1] == '\n')
    line[len-1] = '\0';
  
  // If the line does not contain a "=" mark it as an invalid line
  if (strchr(line, '=') == NULL) {
    printf("Invalid Configuration:\n '%s'", line);
    exit(EXIT_FAILURE);
  }
  
  char* key = stripstr(strtok(line, "="), 0, ' ');
  char* val = stripstr(strtok(NULL, ""), 1, ' ');
  size_t keysize = strlen(key) + 1, valsize = strlen(val) + 1;

  // Allocate more memory if the structure members are not large enough
  if (pair->keysize < keysize) 
    pair->keysize = keysize, pair->key = realloc(pair->key, keysize);
  if (pair->valsize < valsize)
    pair->valsize = valsize, pair->val = realloc(pair->val, valsize);

  strcpy(pair->key, key);
  strcpy(pair->val, val);
  free(line);
  return 1;
};

void cleanKV(KVPair* pair) {
  free(pair -> key);
  free(pair -> val);
  if (pair -> file) {
    fclose(pair -> file);
  }
}

// User input functions
typedef struct key {
  int special;
  char code;
} key;

int keycmp(key k, char code, int special) {
  if (k.special == special && k.code == k.code)
    return 1;
  return 0;
}

key getkey() {
  char k = getchar();
  // Check if it is an arrow key
  if (k == 27 && getchar() == 91)
    return (key){1, getchar()-65};
  return (key){0, k};
}

// Graphics Functions
void drawLine(int x, int y, int length, int vertical, int fCorner, int bCorner) {
  vtCursorXY(x, y);
  putgchar(fCorner);
  for (int i=0; i<length; i++) {
    vertical ? y++ : x++;
    vtCursorXY(x, y);
    putgchar(vertical ? 120 : 113);
  }
  vtCursorXY(x, y);
  putgchar(bCorner);
}

void makeBox(int x, int y, int width, int height, int xPadding, int yPadding) {
  x -= xPadding, y -= yPadding;
  width += xPadding*2, height += yPadding*2+1;
  drawLine(x, y, width, 0, 108, 107); 
  drawLine(x+width, y, height, 1, 107, 106);
  drawLine(x, y, height, 1, 108, 109);
  drawLine(x, y+height, width, 0, 109, 106);
}

int main() {
  // Reset screen and setup term mode
  struct termios oldt, newt;
  
  tcgetattr ( STDIN_FILENO, &oldt );
  newt = oldt;
  newt.c_lflag &= ~( ICANON | ECHO );
  tcsetattr ( STDIN_FILENO, TCSANOW, &newt );
  printf("\e%s", VTResetScreen);

  // Load config file and draw menu
  int selected = 0;
  int numOptions = 0;
  int boxWidth = 0;
  int xPadding = 0;
  int yPadding = 0;
  char** options = malloc(1);
  char** execs = malloc(1);
  KVPair config = loadConfig();
  while(nextPair(&config)) {
    if (strcmp(config.key, "xpadding") == 0)
      xPadding = atoi(config.val);
    else if (strcmp(config.key, "ypadding") == 0)
      yPadding = atoi(config.val);
    else {
      numOptions++;
      options = realloc(options, numOptions*sizeof(char*));
      execs = realloc(execs, numOptions*sizeof(char*));
      options[numOptions-1] = strdup(config.key);
      execs[numOptions-1] = strdup(config.val);
      if (strlen(config.key) > boxWidth)
        boxWidth = strlen(config.key);
    }
  }
  boxWidth += 3;
  int boxHeight = numOptions;
  struct winsize win;
  ioctl(0, TIOCGWINSZ, &win); 
  int boxX = (win.ws_col-boxWidth-2)/2;
  int boxY = (win.ws_row-boxHeight-2)/2;
  makeBox(boxX, boxY, boxWidth+2, boxHeight, xPadding, yPadding);
  // Draw each option inside the box
  for (int i; i<numOptions; i++) {
    vtCursorXY(boxX+1,boxY+1+i);
    if (selected == i)
      putgchar(96);
    else
      putchar(' ');
    printf(" %s", options[i]);
  }
  // Hide Cursor
  printf("\e%s", "[?25l");
  fflush(stdout);
  key k;
  for (;;) {
    k = getkey();
    if (k.code == 'q')
      break;
    if (k.code == '\n') {
      char* cmd = malloc(strlen("startx  2>/dev/null") + strlen(execs[selected]) + 1);
      sprintf(cmd, "startx %s 2>/dev/null", execs[selected]);
      system(cmd);
      free(cmd);
    }
    if (keycmp(k, UpArrow, 1)  || keycmp(k, DownArrow, 1)) {
      vtCursorXY(boxX+1, boxY+1+selected);
      putchar(' ');
      if (k.code == 0)
        selected == 0 ? (selected = numOptions - 1) : selected--; 
      if (k.code == 1)
        selected == numOptions - 1 ? (selected = 0) : selected++;
      
      vtCursorXY(boxX+1, boxY+1+selected);
      putgchar(96);
      fflush(stdout);
    }
  }
  for (int i = 0; i<numOptions; i++) {
    free(options[i]);
    free(execs[i]);
  }
  free(options);
  free(execs);
  printf("\e%s", "[?25h");

  cleanKV(&config);
  tcsetattr ( STDIN_FILENO, TCSANOW, &oldt );
  printf("\e%s", VTResetScreen);
  fflush(stdout);
  return 0;
}
