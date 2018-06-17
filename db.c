#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>



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
  char username[COL_USERNAME_SIZE + 1]; // + 1 for null term
  char email[COL_EMAIL_SIZE + 1]; // ditto
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






struct Pager_t {
  int file_descriptor;
  uint32_t file_length;
  void* pages[TABLE_MAX_PAGES];
};
typedef struct Pager_t Pager;

Pager* pager_open(const char* filename) {
  int fd = open(
    filename,
    O_RDWR |  // Read/Write mode
    O_CREAT,  // Create file if it does not exist
    S_IWUSR | // User write permission
    S_IRUSR   // User read permission
  );

  if (fd == -1) {
    printf("Unable to open file\n");
    exit(EXIT_FAILURE);
  }

  // seek til EOF
  off_t file_length = lseek(fd, 0, SEEK_END);

  Pager* pager = malloc(sizeof(Pager));
  pager->file_descriptor = fd;
  pager->file_length = file_length;

  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
    pager->pages[i] = NULL; // init to null
  }

  return pager;
};

void* get_page(Pager* pager, uint32_t page_num) {
  // case 1: out of bounds page
  if (page_num > TABLE_MAX_PAGES) {
    printf("Cannot fetch out of bounds page number. %d > %d\n", page_num, TABLE_MAX_PAGES);
    exit(EXIT_FAILURE);
  }

  // case 2: no page found aka cache miss
  if (pager->pages[page_num] == NULL) {

    // allocate memory for page
    void* page = malloc(PAGE_SIZE);
    uint32_t num_pages = pager->file_length / PAGE_SIZE;

    // partial page
    if (pager->file_length % PAGE_SIZE) {
      num_pages += 1;
    }

    if (page_num <= num_pages) {
      // set offset to base of page to retrieve
      lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);

      // [disk] read in the full page into "page"
      ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);
      if (bytes_read == -1) {
        printf("Error reading file: %d\n", errno);
        exit(EXIT_FAILURE);
      }
    }

    pager->pages[page_num] = page;
  }

  // return pointer to page
  return pager->pages[page_num];
};

void pager_flush(Pager* pager, uint32_t page_num, uint32_t size) {
  if (pager->pages[page_num] == NULL) {
    printf("Tried to flush null page\n");
    exit(EXIT_FAILURE);
  }

  off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
  if (offset == -1) {
    printf("Error seeking: %d\n", errno);
    exit(EXIT_FAILURE);
  }

  // PERSIST TO DISK!
  ssize_t bytes_written = write(pager->file_descriptor, pager->pages[page_num], size);
  if (bytes_written == -1) {
    printf("Error writing: %d\n", errno);
    exit(EXIT_FAILURE);
  }
};








struct Table_t {
  Pager* pager;
  uint32_t num_rows;
};
typedef struct Table_t Table;

Table* db_open(char* filename) {
  Pager* pager = pager_open(filename);
  uint32_t num_rows = pager->file_length / ROW_SIZE;

  Table* table = malloc(sizeof(Table));
  table->pager = pager;
  table->num_rows = num_rows;

  return table;
};

void db_close(Table* table) {
  Pager* pager = table->pager;
  uint32_t num_full_pages = table->num_rows / ROWS_PER_PAGE;

  // flush + free full pages
  for (uint32_t i = 0; i < num_full_pages; i++) {
    if (pager->pages[i] == NULL) {
      continue;
    }
    pager_flush(pager, i, PAGE_SIZE);
    free(pager->pages[i]);
    pager->pages[i] = NULL;
  }

  // flush + free partial page at end
  uint32_t num_additional_rows = table->num_rows % ROWS_PER_PAGE;
  if (num_additional_rows > 0) {
    uint32_t page_num = num_full_pages;
    if (pager->pages[page_num] != NULL) {
      pager_flush(pager, page_num, num_additional_rows * ROW_SIZE);
      free(pager->pages[page_num]);
      pager->pages[page_num] = NULL;
    }
  }

  // close fd
  int result = close(pager->file_descriptor);
  if (result == -1) {
    printf("Error closing db file\n");
    exit(EXIT_FAILURE);
  }

  // free ALL pages
  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
    void* page = pager->pages[i];
    if (page) {
      free(page);
      pager->pages[i] = NULL;
    }
  }

  // free the pager
  free(pager);
};

void* row_address(Table* table, uint32_t row_num) {
  uint32_t page_num = row_num / ROWS_PER_PAGE;

  void* page = get_page(table->pager, page_num);

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
  PREPARE_NEGATIVE_ID,
  PREPARE_SYNTAX_ERROR,
  PREPARE_STRING_TOO_LONG,
  PREPARE_UNRECOGNIZED
};
typedef enum PrepareResult_t PrepareResult;

PrepareResult prepare_insert(Buffer* buf, Statement* statement) {
  statement->type = STATEMENT_INSERT;

  // tokenize

  char* keyword = strtok(buf->line, " ");
  if (keyword == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }

  char* id_string = strtok(NULL, " ");
  if (id_string == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }

  char* username = strtok(NULL, " ");
  if (username == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }

  char* email = strtok(NULL, " ");
  if (email == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }

  // validate

  int id = atoi(id_string);
  if (id <= 0) {
    return PREPARE_NEGATIVE_ID;
  }

  if (strlen(username) > COL_USERNAME_SIZE) {
    return PREPARE_STRING_TOO_LONG;
  }
  if (strlen(email) > COL_EMAIL_SIZE) {
    return PREPARE_STRING_TOO_LONG;
  }

  // write to statement
  statement->type = STATEMENT_INSERT;

  statement->row_to_insert.id = id;
  strcpy(statement->row_to_insert.username, username);
  strcpy(statement->row_to_insert.email, email);

  return PREPARE_SUCCESS;
};

PrepareResult prepare_select(Statement* statement) {
  statement->type = STATEMENT_SELECT;
  return PREPARE_SUCCESS;
};

PrepareResult prepare_statement(Buffer* buf, Statement* statement) {
  if (strncmp(buf->line, "insert", 6) == 0) {
    return prepare_insert(buf, statement);
  }

  if (strcmp(buf->line, "select") == 0) {
    return prepare_select(statement);
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

MetaCommandResult do_meta_command(char* cmd, Table* table) {
  if (strcmp(cmd, ".exit") == 0) {
    db_close(table);
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
  // args check
  if (argc < 2) {
    printf("Must supply db filename\n");
    exit(EXIT_FAILURE);
  }

  char* filename = argv[1];
  Table* table = db_open(filename);

  Buffer* line_buffer = make_buffer();
  Statement* statement = make_statement();

  while (true) {
    print_prompt();
    read_input(line_buffer);

    // case 1: meta command

    if (is_metacommand(line_buffer->line)) {
      switch (do_meta_command(line_buffer->line, table)) {
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
      case (PREPARE_STRING_TOO_LONG):
        printf("String is too long\n");
        continue;
      case (PREPARE_NEGATIVE_ID):
        printf("ID must be positive\n");
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
