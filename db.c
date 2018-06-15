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
  META_SUCCESS,
  META_UNRECOGNIZED
};
typedef enum MetaCommandResult_t MetaCommandResult;

MetaCommandResult do_meta_command(char* cmd) {
  if (strcmp(cmd, ".exit") == 0) {
    exit(EXIT_SUCCESS);
  } else {
    return META_UNRECOGNIZED;
  }
}




/*
  STATEMENT
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

Statement* make_statement() {
  Statement* statement = malloc(sizeof(Statement));
  return statement;
}




/*
  PREPARED STATEMENT
*/

enum PrepareResult_t {
  PREPARE_SUCCESS,
  PREPARE_UNRECOGNIZED
};
typedef enum PrepareResult_t PrepareResult;

PrepareResult prepare_statement(Buffer* buf, Statement* statement) {
  if (strncmp(buf->line, "insert", 6) == 0) {
    statement->type = STATEMENT_INSERT;
    return PREPARE_SUCCESS;
  }

  if (strcmp(buf->line, "select") == 0) {
    statement->type = STATEMENT_SELECT;
    return PREPARE_SUCCESS;
  }

  // otherwise
  return PREPARE_UNRECOGNIZED;
}



/*
  FUNCTIONS
*/

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

bool is_meta (char* str) {
  if (str[0] == '.') {
    return true;
  } else {
    return false;
  }
}

void execute_statement(Statement* statement) {
  switch (statement->type) {
    case (STATEMENT_INSERT):
      printf("Insertion stub\n");
      break;
    case (STATEMENT_SELECT):
      printf("Select stub\n");
      break;
  }
}


int main(int argc, char* argv[]) {
  Buffer* line_buffer = make_buffer();

  while (true) {
    print_prompt();
    read_input(line_buffer);

    // case 1: meta command

    if (is_meta(line_buffer->line)) {
      switch (do_meta_command(line_buffer->line)) {
        case (META_SUCCESS):
          continue;
        case (META_UNRECOGNIZED):
          printf("Unrecognized meta command '%s'\n", line_buffer->line);
          continue;
      }
    }


    // case 2: prepare and execute sql statement (mutative)

    Statement* statement = make_statement();
    switch (prepare_statement(line_buffer, statement)) {
      case (PREPARE_SUCCESS):
        break;
      case (PREPARE_UNRECOGNIZED):
        printf("Unrecognized keyword at start of '%s'\n", line_buffer->line);
        continue;
    }


    // execute statement

    execute_statement(statement);
    printf("Executed.\n");
  }
}
