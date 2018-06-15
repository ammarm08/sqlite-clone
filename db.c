#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
  BUFFER
*/

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




/*
  METACOMMAND
*/

enum MetaCommandResult_t {
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED_COMMAND
};
typedef enum MetaCommandResult_t MetaCommandResult;

MetaCommandResult do_meta_command(Buffer* buf) {
  if (strcmp(buf->line, ".exit") == 0) {
    exit(EXIT_SUCCESS);
  } else {
    return META_COMMAND_UNRECOGNIZED_COMMAND;
  }
};




/*
  SQL STATEMENT
*/

enum StatementType_t {
  STATEMENT_INSERT,
  STATEMENT_SELECT
};
typedef enum StatementType_t StatementType;

struct Statement_t {
  StatementType type;
};
typedef struct Statement_t Statement;




/*
  PREPARED STATEMENT
*/

enum PrepareResult_t {
  PREPARE_SUCCESS,
  PREPARE_UNRECOGNIZED_STATEMENT
};
typedef enum PrepareResult_t PrepareResult;

PrepareResult prepare_statement(Buffer* buf, Statement* statement) {
  // partial match: insert 1 foo bar
  if (strncmp(buf->line, "insert", 6) == 0) {
    statement->type = STATEMENT_INSERT;
    return PREPARE_SUCCESS;
  }

  // exact match: select
  if (strcmp(buf->line, "select") == 0) {
    statement->type = STATEMENT_SELECT;
    return PREPARE_SUCCESS;
  }

  // otherwise
  return PREPARE_UNRECOGNIZED_STATEMENT;
};





/*
  FUNCTIONS
*/

void execute_statement(Statement* statement) {
  switch(statement->type) {
    case (STATEMENT_INSERT):
      printf("This is where we would do an insert.\n");
      break;
    case (STATEMENT_SELECT):
      printf("This is where we would do a select.\n");
      break;
  }
}

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

    // case 1: meta command

    if (line_buffer->line[0] == '.') {
      switch (do_meta_command(line_buffer)) {
        case (META_COMMAND_SUCCESS):
          continue;
        case (META_COMMAND_UNRECOGNIZED_COMMAND):
          printf("Unrecognized command '%s'\n", line_buffer->line);
          continue;
      }
    }

    // case 2: prepare and execute sql statement

    Statement statement;
    switch (prepare_statement(line_buffer, &statement)) {
      case (PREPARE_SUCCESS):
        break;
      case (PREPARE_UNRECOGNIZED_STATEMENT):
        printf("Unrecognized keyword at start of '%s'.\n", line_buffer->line);
        continue;
    }

    execute_statement(&statement);
    printf("Executed.\n");
  }
}
