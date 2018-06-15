#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct InputBuffer_t {
  char* buffer; // aka "string" of characters
  size_t buffer_length; // size type
  ssize_t input_length; // signed type
};

// this creates alias in global namespace for the struct
typedef struct InputBuffer_t InputBuffer;

// instantiation. returns pointer to an InputBuffer
InputBuffer* new_input_buffer() {
  InputBuffer* input_buffer = malloc(sizeof(InputBuffer));
  input_buffer->buffer = NULL;
  input_buffer->buffer_length = 0;
  input_buffer->input_length = 0;

  return input_buffer;
}

// void return
void print_prompt() { printf("db > "); }

void read_input(InputBuffer* input_buffer) {
  // getline sig: (note, this is not a "pure function")
  // 1: char **lineptr -- pass reference to "string" to write to
  // 2: size_t *n -- pass reference to length of lineptr
  // 3: FILE *stream -- pass reference to stream to read from
  // returns: bytes read in line from stream
  ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);

  if (bytes_read <= 0) {
    printf("Error reading input \n");
    exit(EXIT_FAILURE);
  }

  // ignore trailing newline
  input_buffer->input_length = bytes_read -1;

  // overwrite trailing newline with zero
  input_buffer->buffer[bytes_read - 1] = 0;
}

int main(int argc, char* argv[]) {
  // pointer to an input buffer
  InputBuffer* input_buffer = new_input_buffer();

  // repl
  while (true) {
    print_prompt();
    read_input(input_buffer); // pass by reference

    // input_buffer->buffer: deref + return value of "buffer" field

    if (strcmp(input_buffer->buffer, ".exit") == 0) {
      exit(EXIT_SUCCESS);
    } else {
      printf("Unrecognized command '%s'. \n", input_buffer->buffer);
    }
  }
}
