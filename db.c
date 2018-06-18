#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>




/*
  TYPE DEFINITIONS
*/



// BUFFER
struct Buffer_t {
  char* line;
  size_t line_length;
  ssize_t input_length; // getline can return -1
};
typedef struct Buffer_t Buffer;



// ROW


const uint32_t COL_USERNAME_SIZE = 32;
const uint32_t COL_EMAIL_SIZE = 255;

struct Row_t {
  uint32_t id;
  char username[COL_USERNAME_SIZE + 1]; // + 1 for null term
  char email[COL_EMAIL_SIZE + 1]; // ditto
};
typedef struct Row_t Row;



// PAGER


const uint32_t PAGE_SIZE = 4096; // 4kb
const uint32_t TABLE_MAX_PAGES = 100;

struct Pager_t {
  int file_descriptor;
  uint32_t file_length;
  uint32_t num_pages;
  void* pages[TABLE_MAX_PAGES];
};
typedef struct Pager_t Pager;




// TABLE

#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute);
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);

const uint32_t ID_OFFSET = 0; // because it is the first field
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

struct Table_t {
  Pager* pager;
  uint32_t root_page_num;
};
typedef struct Table_t Table;




// CURSOR


struct Cursor_t {
  Table* table;
  uint32_t page_num;
  uint32_t cell_num;
  bool end_of_table;
};
typedef struct Cursor_t Cursor;




// BTREE NODE


// Common Node Headers
const uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);
const uint32_t NODE_TYPE_OFFSET = 0;
const uint32_t IS_ROOT_SIZE = sizeof(uint8_t);
const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE;
const uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);
const uint32_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE;
const uint8_t COMMON_NODE_HEADER_SIZE = NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE;

// Internal Node Headers
const uint32_t INTERNAL_NODE_NUM_KEYS_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_NUM_KEYS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t INTERNAL_NODE_RIGHT_CHILD_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_RIGHT_CHILD_OFFSET = INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE;
const uint32_t INTERNAL_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE;

// Internal Node Body
const uint32_t INTERNAL_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_CHILD_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_CELL_SIZE = INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE;

// Leaf Node Headers
const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_NEXT_LEAF_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NEXT_LEAF_OFFSET = LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE;
const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE + LEAF_NODE_NEXT_LEAF_SIZE;

// Leaf Body
const uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_KEY_OFFSET = 0;
const uint32_t LEAF_NODE_VALUE_SIZE = ROW_SIZE; // row to insert
const uint32_t LEAF_NODE_VALUE_OFFSET = LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE;
const uint32_t LEAF_NODE_CELL_SIZE = LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;
const uint32_t LEAF_NODE_SPACE_FOR_CELLS = PAGE_SIZE - LEAF_NODE_HEADER_SIZE; // each leaf node corresponds w/ a page size
const uint32_t LEAF_NODE_MAX_CELLS = LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;

// Split constants
const uint32_t LEAF_NODE_RIGHT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) / 2; // N original cells + one new one
const uint32_t LEAF_NODE_LEFT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT;

enum NodeType_t {
  NODE_INTERNAL,
  NODE_LEAF
};
typedef enum NodeType_t NodeType;




// STATEMENT


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




// PREPARE


enum PrepareResult_t {
  PREPARE_SUCCESS,
  PREPARE_NEGATIVE_ID,
  PREPARE_SYNTAX_ERROR,
  PREPARE_STRING_TOO_LONG,
  PREPARE_UNRECOGNIZED
};
typedef enum PrepareResult_t PrepareResult;




// EXECUTE


enum ExecuteResult_t {
  EXECUTE_SUCCESS,
  EXECUTE_DUPLICATE_KEY,
  EXECUTE_TABLE_FULL
};
typedef enum ExecuteResult_t ExecuteResult;




// META


enum MetaCommandResult_t {
  META_SUCCESS,
  META_UNRECOGNIZED
};
typedef enum MetaCommandResult_t MetaCommandResult;








/*
  BUFFER
*/


Buffer* make_buffer() {
  Buffer* buf = malloc(sizeof(Buffer));
  buf->line = NULL;
  buf->line_length = 0;
  buf->input_length = 0;

  return buf;
};


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






/*
  ROW
*/


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
  PAGER
*/



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
  pager->num_pages = (file_length / PAGE_SIZE);

  if (file_length % PAGE_SIZE != 0) {
    printf("Db file is not a whole number of pages. Corrupt file\n");
    exit(EXIT_FAILURE);
  }

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

    if (page_num >= pager->num_pages) {
      pager->num_pages = page_num + 1;
    }
  }

  // return pointer to page
  return pager->pages[page_num];
};



void pager_flush(Pager* pager, uint32_t page_num) {
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
  ssize_t bytes_written = write(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE);
  if (bytes_written == -1) {
    printf("Error writing: %d\n", errno);
    exit(EXIT_FAILURE);
  }
};


// for now, not recycling free pages. just go to end of db file.
uint32_t get_unused_page_num(Pager* pager) {
  return pager->num_pages;
};



/*
  B-Tree
*/


// node getters + setters (in-place)



bool is_node_root(void* node) {
  uint8_t value = *((uint8_t*)(node + IS_ROOT_OFFSET));
  return (bool)value;
};



void set_node_root(void* node, bool is_root) {
  uint8_t value = is_root;
  *((uint8_t*)(node + IS_ROOT_OFFSET)) = value;
};



NodeType get_node_type(void* node) {
  uint8_t value = *((uint8_t*)(node + NODE_TYPE_OFFSET)); // casting to uint8_t
  return (NodeType)value;
};



void set_node_type(void* node, NodeType type) {
  uint8_t value = type;
  *((uint8_t*)(node + NODE_TYPE_OFFSET)) = value; // casting to uint8_t
};



uint32_t* internal_node_num_keys(void* node) {
  return node + INTERNAL_NODE_NUM_KEYS_OFFSET;
};



uint32_t* internal_node_right_child(void* node) {
  return node + INTERNAL_NODE_RIGHT_CHILD_OFFSET;
};



uint32_t* internal_node_cell(void* node, uint32_t cell_num) {
  return node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE;
};



uint32_t* internal_node_child(void* node, uint32_t child_num) {
  uint32_t num_keys = *internal_node_num_keys(node);
  if (child_num > num_keys) {
    printf("Tried to access child_num %d > num_keys %d\n", child_num, num_keys);
    exit(EXIT_FAILURE);
  } else if (child_num == num_keys) { // one beyond
    return internal_node_right_child(node);
  } else { // left_child
    return internal_node_cell(node, child_num);
  }
};



uint32_t* internal_node_key(void* node, uint32_t key_num) {
  return internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;
};



uint32_t* leaf_node_num_cells(void* node) {
  return node + LEAF_NODE_NUM_CELLS_OFFSET;
};



void* leaf_node_cell(void* node, uint32_t cell_num) {
  return node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
};



uint32_t* leaf_node_key(void* node, uint32_t cell_num) {
  return leaf_node_cell(node, cell_num);
};



void* leaf_node_value(void* node, uint32_t cell_num) {
  return leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
};


uint32_t* leaf_node_next_leaf(void* node) {
  return node + LEAF_NODE_NEXT_LEAF_OFFSET;
};



void initialize_leaf_node(void* node) {
  set_node_type(node, NODE_LEAF);
  set_node_root(node, false);     // not root
  *leaf_node_num_cells(node) = 0; // no children
  *leaf_node_next_leaf(node) = 0; // no sibling
};



void initialize_internal_node(void* node) {
  set_node_type(node, NODE_INTERNAL);
  set_node_root(node, false);
  *internal_node_num_keys(node) = 0;
};



uint32_t get_node_max_key(void* node) {
  switch(get_node_type(node)) {
    case NODE_INTERNAL: // rightmost key
      return *internal_node_key(node, *internal_node_num_keys(node) - 1);
    case NODE_LEAF: // max index
      return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
  }
};




Cursor* leaf_node_find(Table* table, uint32_t page_num, uint32_t key) {
  void* node = get_page(table->pager, page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);

  Cursor* cursor = malloc(sizeof(Cursor));
  cursor->table = table;
  cursor->page_num = page_num;

  // Binary search
  uint32_t min = 0;
  uint32_t max = num_cells;

  while (max != min) {
    uint32_t idx = (min + max) / 2;
    uint32_t key_at_index = *leaf_node_key(node, idx);

    // found
    if (key == key_at_index) {
      cursor->cell_num = idx;
      return cursor;
    }

    // keep searching
    if (key < key_at_index) {
      max = idx;
    } else {
      min = idx + 1;
    }
  }

  // no match. returns insertion point
  cursor->cell_num = min;
  return cursor;
};



void create_new_root(Table* table, uint32_t right_child_page_num) {
  void* root = get_page(table->pager, table->root_page_num);
  void* right_child = get_page(table->pager, right_child_page_num);
  uint32_t left_child_page_num = get_unused_page_num(table->pager);
  void* left_child = get_page(table->pager, left_child_page_num);

  // 1. copy root data to left child
  memcpy(left_child, root, PAGE_SIZE);
  set_node_root(left_child, false);

  // 2. init new root data
  initialize_internal_node(root);
  set_node_root(root, true);
  *internal_node_num_keys(root) = 1;

  // 3. set up pointers to children
  *internal_node_child(root, 0) = left_child_page_num;
  uint32_t left_child_max_key = get_node_max_key(left_child);
  *internal_node_key(root, 0) = left_child_max_key;
  *internal_node_right_child(root) = right_child_page_num;
};



Cursor* internal_node_find(Table* table, uint32_t page_num, uint32_t key) {
  /*
    binary search across an internal node to find key.
    if key is leaf_node -> call leaf_node_find
    else recursive call to next level internal node
  */

  void* node = get_page(table->pager, page_num);
  uint32_t num_keys = *internal_node_num_keys(node);

  uint32_t min = 0;
  uint32_t max = num_keys;

  while (min != max) {
    uint32_t mid = (min + max) / 2;
    uint32_t key_to_right = *internal_node_key(node, mid);
    if (key_to_right >= key) {
      max = mid;
    } else {
      min = mid + 1;
    }
  }

  uint32_t child_num = *internal_node_child(node, min);
  void* child = get_page(table->pager, child_num);
  switch (get_node_type(child)) {
    case NODE_LEAF:
      return leaf_node_find(table, child_num, key);
    case NODE_INTERNAL:
      return internal_node_find(table, child_num, key);
  }
};



void leaf_node_split_and_insert(Cursor* cursor, uint32_t key, Row* value) {
  void* old_node = get_page(cursor->table->pager, cursor->page_num);

  // step 1: make a new node + point to sibling
  uint32_t new_page_num = get_unused_page_num(cursor->table->pager);
  void* new_node = get_page(cursor->table->pager, new_page_num);
  initialize_leaf_node(new_node);
  *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
  *leaf_node_next_leaf(old_node) = new_page_num;

  // step 2: split all keys (including new one) evenly between old and new node
  for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--) {
    void* dest_node;
    // left or right
    if (i >= LEAF_NODE_LEFT_SPLIT_COUNT) {
      dest_node = new_node;
    } else {
      dest_node = old_node;
    }

    // align
    uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
    void* dest = leaf_node_cell(dest_node, index_within_node);

    // write if new entry (key + value) else copy old to new
    if (i == cursor->cell_num) {
      serialize_row(
        value,
        leaf_node_value(dest_node, index_within_node) // necessary to avoid writing in key space
      );
      *leaf_node_key(dest_node, index_within_node) = key;
    } else if (i > cursor->cell_num) {
      memcpy(dest, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
    } else {
      memcpy(dest, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
    }
  }

  // step 3: update cell counts in headers
  *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
  *(leaf_node_num_cells(new_node)) = LEAF_NODE_RIGHT_SPLIT_COUNT;

  // step 4: update parent (root or otherwise)
  if (is_node_root(old_node)) {
    return create_new_root(cursor->table, new_page_num);
  } else {
    printf("Need to implement updating parent after split\n");
    exit(EXIT_FAILURE);
  }
};



void leaf_node_insert(Cursor* cursor, uint32_t key, Row* value) {
  void* node = get_page(cursor->table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);

  // case 1: split if node is full
  if (num_cells >= LEAF_NODE_MAX_CELLS) {
    leaf_node_split_and_insert(cursor, key, value);
    return;
  }

  // case 2: make room at insertion point ("cell_num")
  if (cursor->cell_num < num_cells) {
    for (uint32_t i = num_cells; i > cursor->cell_num; i--) {
      memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1), LEAF_NODE_CELL_SIZE);
    }
  }

  // increment count of cells
  *(leaf_node_num_cells(node)) += 1;

  // set key
  *(leaf_node_key(node, cursor->cell_num)) = key;

  // finally, insert value
  serialize_row(value, leaf_node_value(node, cursor->cell_num));
};






/*
  TABLE
*/




Table* db_open(char* filename) {
  Pager* pager = pager_open(filename);

  Table* table = malloc(sizeof(Table));
  table->pager = pager;

  // New DB file. Initialize page 0 as leaf node.
  if (pager->num_pages == 0) {
    void* root_node = get_page(pager, 0);
    initialize_leaf_node(root_node);
    set_node_root(root_node, true);
  }

  return table;
};




void db_close(Table* table) {
  Pager* pager = table->pager;

  // flush + free full pages
  for (uint32_t i = 0; i < pager->num_pages; i++) {
    if (pager->pages[i] == NULL) {
      continue;
    }
    pager_flush(pager, i);
    free(pager->pages[i]);
    pager->pages[i] = NULL;
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






/*
  CURSOR
*/



Cursor* table_find(Table* table, uint32_t key) {
  uint32_t root_page_num = table->root_page_num;
  void* root_node = get_page(table->pager, root_page_num);

  if (get_node_type(root_node) == NODE_LEAF) {
    return leaf_node_find(table, root_page_num, key);
  } else {
    return internal_node_find(table, root_page_num, key);
  }
};





Cursor* table_start(Table* table) {
  Cursor* cursor = table_find(table, 0);

  void* node = get_page(table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);
  cursor->end_of_table = (num_cells == 0);

  return cursor;
};




void cursor_advance(Cursor* cursor) {
  uint32_t page_num = cursor->page_num;
  void* node = get_page(cursor->table->pager, page_num);

  cursor->cell_num += 1;

  // either the end, or jump to next page (sibling)
  if (cursor->cell_num >= (*leaf_node_num_cells(node))) {
    uint32_t next_page_num = *leaf_node_next_leaf(node);
    if (next_page_num == 0) {
      cursor->end_of_table = true;
    } else {
      cursor->page_num = next_page_num;
      cursor->cell_num = 0;
    }
  }
};




void* cursor_value(Cursor* cursor) {
  uint32_t page_num = cursor->page_num;
  void* page = get_page(cursor->table->pager, page_num);

  return leaf_node_value(page, cursor->cell_num);
};






/*
  STATEMENT
*/



Statement* make_statement() {
  Statement* statement = malloc(sizeof(Statement));
  return statement;
}




/*
  PREPARE STATEMENT
*/



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



ExecuteResult execute_insert(Statement* statement, Table* table) {
  void* node = get_page(table->pager, table->root_page_num);
  uint32_t num_cells = (*leaf_node_num_cells(node));

  Row* row_to_insert = &(statement->row_to_insert);
  uint32_t key_to_insert = row_to_insert->id;

  // scan tree, update cursor to insertion position
  Cursor* cursor = table_find(table, key_to_insert);


  // case: node was already found or an internal node must be traversed (not yet implemented)
  if (cursor->cell_num < num_cells) {
    uint32_t key_at_index = *leaf_node_key(node, cursor->cell_num);
    if (key_at_index == key_to_insert) {
      return EXECUTE_DUPLICATE_KEY;
    }
  }

  // case 2: node not found. cursor points to insertion point
  leaf_node_insert(cursor, row_to_insert->id, row_to_insert);
  free(cursor);

  return EXECUTE_SUCCESS;
};



ExecuteResult execute_select(Statement* statement, Table* table) {
  Cursor* cursor = table_start(table); // jump to start

  // print every row in the table
  Row row;
  while (!(cursor->end_of_table)) {
    deserialize_row(cursor_value(cursor), &row);
    print_row(&row);

    cursor_advance(cursor);
  }

  free(cursor);

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



void print_constants() {
  printf("ROW_SIZE: %d\n", ROW_SIZE);
  printf("COMMON_NODE_HEADER_SIZE: %d\n", COMMON_NODE_HEADER_SIZE);
  printf("LEAF_NODE_HEADER_SIZE: %d\n", LEAF_NODE_HEADER_SIZE);
  printf("LEAF_NODE_CELL_SIZE: %d\n", LEAF_NODE_CELL_SIZE);
  printf("LEAF_NODE_SPACE_FOR_CELLS: %d\n", LEAF_NODE_SPACE_FOR_CELLS);
  printf("LEAF_NODE_MAX_CELLS: %d\n", LEAF_NODE_MAX_CELLS);
};




void indent(uint32_t level) {
  for (uint32_t i = 0; i < level; i++) {
    printf("\t");
  }
};



void print_tree(Pager* pager, uint32_t page_num, uint32_t indent_level) {
  void* node = get_page(pager, page_num);
  uint32_t num_keys, child;

  switch (get_node_type(node)) {
    case (NODE_LEAF):
      num_keys = *leaf_node_num_cells(node);
      indent(indent_level);
      printf("- leaf (size %d)\n", num_keys);
      for (uint32_t i = 0; i < num_keys; i++) {
        indent(indent_level + 1);
        printf("- %d\n", *leaf_node_key(node, i));
      }
      break;
    case (NODE_INTERNAL):
      num_keys = *internal_node_num_keys(node);
      indent(indent_level);
      printf("- internal (size %d)\n", num_keys);
      for (uint32_t i = 0; i < num_keys; i++) {
        child = *internal_node_child(node, i);
        print_tree(pager, child, indent_level + 1);

        indent(indent_level);
        printf("- key %d\n", *internal_node_key(node, i));
      }
      child = *internal_node_right_child(node);
      print_tree(pager, child, indent_level + 1);
      break;
  }
};







MetaCommandResult do_meta_command(char* cmd, Table* table) {
  if (strcmp(cmd, ".exit") == 0) {
    db_close(table);
    exit(EXIT_SUCCESS);
  } else if (strcmp(cmd, ".constants") == 0) {
    printf("Constants:\n");
    print_constants();
    return META_SUCCESS;
  } else if (strcmp(cmd, ".btree") == 0) {
    printf("Tree:\n");
    print_tree(table->pager, 0, 0);
    return META_SUCCESS;
  } else {
    return META_UNRECOGNIZED;
  }
}


bool is_metacommand (char* str) {
  if (str[0] == '.') {
    return true;
  } else {
    return false;
  }
}






/*
  MAIN
*/


void print_prompt() { printf("db > "); }


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
      case (EXECUTE_DUPLICATE_KEY):
        printf("Error: Duplicate key.\n");
        break;
      case (EXECUTE_TABLE_FULL):
        printf("Error: Table full.\n");
        break;
    }
  }
}
