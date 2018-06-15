#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Buffer_t {
  char* line;
  size_t line_length;
  ssize_t input_length; // getline can return -1
};
typedef struct Buffer_t Buffer;

Buffer* make_buffer() {
  Buffer* buf = malloc(sizeof(Buffer));
  buf->line = NULL;
  buf->line_length = 0;
  buf->input_length = 0;

  return buf;
};

void print_prompt() { printf("db > "); }

void read_input(Buffer* buf) {
  ssize_t bytes_read = getline(&(buf->line), &(buf->line_length), stdin);

  if (bytes_read <= 0) {
    printf("Error reading input\n");
    exit(EXIT_FAILURE);
  }

  // update input length (ignore trailing newline)
  buf->input_length = bytes_read - 1;

  // trim newline from what was read into the buffer
  buf->line[bytes_read - 1] = 0;
};


int main(int argc, char* argv[]) {
  Buffer* line_buffer = make_buffer();

  while (true) {
    print_prompt();
    read_input(line_buffer);

    if (strcmp(line_buffer->line, ".exit") == 0) {
      exit(EXIT_SUCCESS);
    } else {
      printf("Unrecognized command '%s'.\n", line_buffer->line);
    }
  }
}
