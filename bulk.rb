db_executable = './a.out test.db'
raw_output = nil
commands = (1..13).map do |i|
  "insert #{i} user#{i} person#{i}@gmail.com"
end
commands << "select"
commands << "insert 14 user14 person14"
commands << "select"


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

puts raw_output.split("\n")
