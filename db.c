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
  ROW
*/

const uint32_t COL_USERNAME_SIZE = 32;
const uint32_t COL_EMAIL_SIZE = 255;
struct Row_t {
  uint32_t id;
  char username[COL_USERNAME_SIZE];
  char email[COL_EMAIL_SIZE];
};
typedef struct Row_t Row;

#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute);
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);

const uint32_t ID_OFFSET = 0; // because it is the first field
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

void serialize_row(Row* src, void* dest) {
  memcpy(dest + ID_OFFSET, &(src->id), ID_SIZE);
  memcpy(dest + USERNAME_OFFSET, &(src->username), USERNAME_SIZE);
  memcpy(dest + EMAIL_OFFSET, &(src->email), EMAIL_SIZE);
};

void deserialize_row(void* src, Row* dest) {
  memcpy(&(dest->id), src + ID_OFFSET, ID_SIZE);
  memcpy(&(dest->username), src + USERNAME_OFFSET, USERNAME_SIZE);
  memcpy(&(dest->email), src + EMAIL_OFFSET, EMAIL_SIZE);
};

void print_row(Row* row) {
  printf("(%d, %s, %s)\n", row->id, row->username, row->email);
};






/*
  TABLE
*/

const uint32_t PAGE_SIZE = 4096; // 4kb
const uint32_t TABLE_MAX_PAGES = 100;
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

struct Table_t {
  void* pages[TABLE_MAX_PAGES];
  uint32_t num_rows;
};
typedef struct Table_t Table;

Table* make_table() {
  Table* table = malloc(sizeof(Table));
  table->num_rows = 0;

  return table;
};

void* row_address(Table* table, uint32_t row_num) {
  uint32_t page_num = row_num / ROWS_PER_PAGE;
  void* page = table->pages[page_num];

  if (!page) {
    page = table->pages[page_num] = malloc(PAGE_SIZE);
  }

  uint32_t row_offset = row_num % ROWS_PER_PAGE;
  uint32_t byte_offset = row_offset * ROW_SIZE;

  return page + byte_offset;
};







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
  Row row_to_insert; // for inserts only
};
typedef struct Statement_t Statement;

Statement* make_statement() {
  Statement* statement = malloc(sizeof(Statement));
  return statement;
}




/*
  PREPARE STATEMENT
*/

enum PrepareResult_t {
  PREPARE_SUCCESS,
  PREPARE_SYNTAX_ERROR,
  PREPARE_UNRECOGNIZED
};
typedef enum PrepareResult_t PrepareResult;

PrepareResult prepare_statement(Buffer* buf, Statement* statement) {
  if (strncmp(buf->line, "insert", 6) == 0) {
    // reads values to fields into statement
    int assigned_args = sscanf(
      buf->line,
      "insert %d %s %s",
      &(statement->row_to_insert.id), // %d value
      statement->row_to_insert.username, // %s char pointer
      statement->row_to_insert.email // %s char pointer
    );
    if (assigned_args < 3) {
      return PREPARE_SYNTAX_ERROR;
    }

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
  EXECUTE STATEMENT
*/

enum ExecuteResult_t {
  EXECUTE_SUCCESS,
  EXECUTE_TABLE_FULL
};
typedef enum ExecuteResult_t ExecuteResult;

ExecuteResult execute_insert(Statement* statement, Table* table) {
  if (table->num_rows >= TABLE_MAX_ROWS) {
    return EXECUTE_TABLE_FULL;
  }

  // append
  serialize_row(
    &(statement->row_to_insert),
    row_address(table, table->num_rows)
  );

  table->num_rows += 1;

  return EXECUTE_SUCCESS;
};

ExecuteResult execute_select(Statement* statement, Table* table) {
  // print every row in the table
  Row row;
  for (uint32_t i = 0; i < table->num_rows; i++) {
    deserialize_row(row_address(table, i), &row);
    print_row(&row);
  }

  return EXECUTE_SUCCESS;
};

ExecuteResult execute_statement(Statement* statement, Table* table) {
  switch (statement->type) {
    case (STATEMENT_INSERT):
      return execute_insert(statement, table);
    case (STATEMENT_SELECT):
      return execute_select(statement, table);
  }
};



/*
  METACOMMANDS
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

bool is_metacommand (char* str) {
  if (str[0] == '.') {
    return true;
  } else {
    return false;
  }
}






int main(int argc, char* argv[]) {
  Table* table = make_table();
  Buffer* line_buffer = make_buffer();
  Statement* statement = make_statement();

  while (true) {
    print_prompt();
    read_input(line_buffer);

    // case 1: meta command

    if (is_metacommand(line_buffer->line)) {
      switch (do_meta_command(line_buffer->line)) {
        case (META_SUCCESS):
          continue;
        case (META_UNRECOGNIZED):
          printf("Unrecognized meta command '%s'\n", line_buffer->line);
          continue;
      }
    }


    // case 2: prepare and execute sql statement (mutative)

    switch (prepare_statement(line_buffer, statement)) {
      case (PREPARE_SUCCESS):
        break;
      case (PREPARE_SYNTAX_ERROR):
        printf("Syntax error in statement '%s'\n", line_buffer->line);
        continue;
      case (PREPARE_UNRECOGNIZED):
        printf("Unrecognized keyword at start of '%s'\n", line_buffer->line);
        continue;
    }


    // execute statement

    switch (execute_statement(statement, table)) {
      case (EXECUTE_SUCCESS):
        printf("Executed.\n");
        break;
      case (EXECUTE_TABLE_FULL):
        printf("Error: Table full.\n");
        break;
    }
  }
}
