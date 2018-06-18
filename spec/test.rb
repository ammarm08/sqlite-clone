RSpec.describe 'database' do
  # delete and recompile db executable before starting suite
  before(:all) do
    `rm -rf a.out; gcc db.c`
  end

  # delete test dbfile before each test
  before(:each) do
    `rm -rf test.db`
  end

  def run_script(commands)
    raw_output = nil
    db_executable = "./a.out test.db"

    IO.popen(db_executable, "r+") do |pipe|
      commands.each do |command|
        begin
          pipe.puts command
        rescue Errno::EPIPE
          break
        end
      end

      pipe.close_write

      raw_output = pipe.gets(nil)
    end

    raw_output.split("\n")
  end

  it 'inserts and retrieves a row' do
    result = run_script([
      "insert 1 user1 person1@example.com",
      "select",
      ".exit"
    ])
    expect(result).to match_array([
      "db > Executed.",
      "db > (1, user1, person1@example.com)",
      "Executed.",
      "db > "
    ])
  end

  it 'prints error message when table is full' do
    script = (1..1401).map do |i|
      "insert #{i} user#{i} person#{i}@example.com"
    end
    script << ".exit"
    result = run_script(script)
    expect(result.last(2)).to match_array([
      "db > Executed.",
      "db > Need to implement splitting internal node"
    ])
  end

  it 'allows inserting strings that are the maximum length' do
    long_username = "a"*32
    long_email = "a"*255
    script = [
      "insert 1 #{long_username} #{long_email}",
      "select",
      ".exit"
    ]
    result = run_script(script)
    expect(result).to match_array([
      "db > Executed.",
      "db > (1, #{long_username}, #{long_email})",
      "Executed.",
      "db > "
    ])
  end

  it 'prints error message if strings are too long' do
    long_username = "a"*33
    long_email = "a"*256
    script = [
      "insert 1 #{long_username} #{long_email}",
      "select",
      ".exit"
    ]
    result = run_script(script)
    expect(result).to match_array([
      "db > String is too long",
      "db > Executed.",
      "db > "
    ])
  end

  it 'prints an error message if id is negative' do
    script = [
      "insert -1 foo bar@example.com",
      "select",
      ".exit"
    ]
    result = run_script(script)
    expect(result).to match_array([
      "db > ID must be positive",
      "db > Executed.",
      "db > "
    ])
  end

  it 'keeps data after closing connection' do
    result1 = run_script([
      "insert 1 user1 person1@example.com",
      ".exit"
    ])
    expect(result1).to match_array([
      "db > Executed.",
      "db > "
    ])

    result2 = run_script([
      "select",
      ".exit"
    ])
    expect(result2).to match_array([
      "db > (1, user1, person1@example.com)",
      "Executed.",
      "db > "
    ])
  end

  it 'prints constants' do
    script = [
      ".constants",
      ".exit"
    ]
    result = run_script(script)

    expect(result).to match_array([
      "db > Constants:",
      "ROW_SIZE: 293",
      "COMMON_NODE_HEADER_SIZE: 6",
      "LEAF_NODE_HEADER_SIZE: 14",
      "LEAF_NODE_CELL_SIZE: 297",
      "LEAF_NODE_SPACE_FOR_CELLS: 4082",
      "LEAF_NODE_MAX_CELLS: 13",
      "db > "
    ])
  end

  it 'allows printing structure of one-node btree' do
    script = [3, 1, 2].map do |i|
      "insert #{i} user#{i} person#{i}@gmail.com"
    end
    script << ".btree"
    script << ".exit"

    result = run_script(script)

    expect(result).to match_array([
      "db > Executed.",
      "db > Executed.",
      "db > Executed.",
      "db > Tree:",
      "- leaf (size 3)",
      "\t- 1",
      "\t- 2",
      "\t- 3",
      "db > "
    ])
  end

  it 'prints an error message if there is a duplicate id' do
    script = [
      "insert 1 user1 person1@example.com",
      "insert 1 user1 person1@example.com",
      "select",
      ".exit"
    ]
    result = run_script(script)
    expect(result).to match_array([
      "db > Executed.",
      "db > Error: Duplicate key.",
      "db > (1, user1, person1@example.com)",
      "Executed.",
      "db > "
    ])
  end

  it 'allows printing out the structure of a 3-leaf-node btree' do
    script = (1..14).map do |i|
      "insert #{i} user#{i} person#{i}@gmail.com"
    end
    script << ".btree"
    script << "insert 15 user15 person15@gmail.com"
    script << ".exit"
    result = run_script(script)

    expect(result[14...(result.length)]).to eq([
      "db > Tree:",
      "- internal (size 1)",
      "\t- leaf (size 7)",
      "\t\t- 1",
      "\t\t- 2",
      "\t\t- 3",
      "\t\t- 4",
      "\t\t- 5",
      "\t\t- 6",
      "\t\t- 7",
      "- key 7",
      "\t- leaf (size 7)",
      "\t\t- 8",
      "\t\t- 9",
      "\t\t- 10",
      "\t\t- 11",
      "\t\t- 12",
      "\t\t- 13",
      "\t\t- 14",
      "db > Executed.",
      "db > "
    ])
  end

  it 'allows printing out the structure of a 4-leaf-node btree' do
    script = [
      "insert 18 user18 person18@example.com",
      "insert 7 user7 person7@example.com",
      "insert 10 user10 person10@example.com",
      "insert 29 user29 person29@example.com",
      "insert 23 user23 person23@example.com",
      "insert 4 user4 person4@example.com",
      "insert 14 user14 person14@example.com",
      "insert 30 user30 person30@example.com",
      "insert 15 user15 person15@example.com",
      "insert 26 user26 person26@example.com",
      "insert 22 user22 person22@example.com",
      "insert 19 user19 person19@example.com",
      "insert 2 user2 person2@example.com",
      "insert 1 user1 person1@example.com",
      "insert 21 user21 person21@example.com",
      "insert 11 user11 person11@example.com",
      "insert 6 user6 person6@example.com",
      "insert 20 user20 person20@example.com",
      "insert 5 user5 person5@example.com",
      "insert 8 user8 person8@example.com",
      "insert 9 user9 person9@example.com",
      "insert 3 user3 person3@example.com",
      "insert 12 user12 person12@example.com",
      "insert 27 user27 person27@example.com",
      "insert 17 user17 person17@example.com",
      "insert 16 user16 person16@example.com",
      "insert 13 user13 person13@example.com",
      "insert 24 user24 person24@example.com",
      "insert 25 user25 person25@example.com",
      "insert 28 user28 person28@example.com",
      ".btree",
      ".exit"
    ]
    result = run_script(script)
    expect(result[30...(result.length)]).to eq([
      "db > Tree:",
      "- internal (size 3)",
      "\t- leaf (size 7)",
      "\t\t- 1",
      "\t\t- 2",
      "\t\t- 3",
      "\t\t- 4",
      "\t\t- 5",
      "\t\t- 6",
      "\t\t- 7",
      "- key 7",
      "\t- leaf (size 8)",
      "\t\t- 8",
      "\t\t- 9",
      "\t\t- 10",
      "\t\t- 11",
      "\t\t- 12",
      "\t\t- 13",
      "\t\t- 14",
      "\t\t- 15",
      "- key 15",
      "\t- leaf (size 7)",
      "\t\t- 16",
      "\t\t- 17",
      "\t\t- 18",
      "\t\t- 19",
      "\t\t- 20",
      "\t\t- 21",
      "\t\t- 22",
      "- key 22",
      "\t- leaf (size 8)",
      "\t\t- 23",
      "\t\t- 24",
      "\t\t- 25",
      "\t\t- 26",
      "\t\t- 27",
      "\t\t- 28",
      "\t\t- 29",
      "\t\t- 30",
      "db > "
    ])
  end
end
