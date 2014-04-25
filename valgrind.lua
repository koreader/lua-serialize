
local serial = require("serialize")

local addressbook = {
    name = "Alice",
    id = 12345,
    phone = {
		{ number = "1301234567" },
		{ number = "87654321", type = "WORK" },
	}
}

for i=1, 1000 do
    local p = serial.pack(addressbook)
    local t = serial.unpack(p)
end

for i=1, 100 do
    local p = serial.pack(addressbook)
    local s = serial.serialize(p)
    local r = serial.deserialize(s)
end

for i=1, 10 do
    serial.dump(addressbook, "/tmp/dump.bin")
    serial.load("/tmp/dump.bin")
end

