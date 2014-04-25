```lua
	serialize = require "serialize"

	-- pack serialize lua objects into a lightuserdata (use malloc internal) 
	-- It supports types : nil , number , boolean, lightuserdata , string , table (without recursion)
	bin = serialize.pack(...) 

	-- You can append some objects end of the binary block packed before
	serialize.append(bin, ...)

	-- unpack extract ... from bin, and free the memory. 
  -- If a packed bin is not serialized or dumped, unpack should be called on that bin to free memory. 
	-- You can only unpack binary block once.
	serialize.unpack(bin)

	-- You can use serialize.serialize(bin) to serialize them to one block
	-- You can send the block to the other process.
	local block, length = serialize.serialize(bin)

  -- You can use serialize.deserialize(block) to deserialize block to variables
	serialize.deserialize(block)

  -- You can dump variables to a binary file.
  -- It supports types as serialize.pack
  serialize.dump(..., "dump.bin")

  -- Then the dumped file can be loaded to variables.
  serialize.load("dump.bin")
```
