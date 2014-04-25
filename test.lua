local serial = require("serialize")

describe("serialize module", function()
    t = { hello={3, 4}, false, 0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, "hello" }
    describe("lower API", function()
        it("should pack up table", function()
            p = serial.pack(t)
            assert.truthy(p)
        end)
        it("should append data to pack", function()
            p = serial.append(p, 42, 4.2, -1, 1000, "hello", true, false, nil, "123")
            assert.truthy(p)
        end)
        it("should unpack pack to data", function()
            t = serial.unpack(p)
            assert.truthy(t)
        end)
        it("should serialize pack to block", function()
            b, len = serial.serialize(serial.pack(t))
            assert.truthy(b)
            assert.are.same(53, len)
        end)
        it("should deserialize block to table", function()
            s = serial.deserialize(b)
            assert.are.same(t, s)
        end)
        it("should serialize/deserialize multiple variables", function()
            local b, len = serial.serialize(serial.pack(t, 15, nil, true, "lua"))
            local a, b, c, d, e = serial.deserialize(b)
            assert.are.same(t, a)
            assert.are.same(15, b)
            assert.are.same(nil, c)
            assert.are.same(true, d)
            assert.are.same("lua", e)
        end)
    end)
    describe("higher API", function()
        it("should dump variable to binary file", function()
            len = serial.dump(t, "/tmp/dump.bin")
            assert.are.same(53, len)
        end)
        it("should load variable from dump file", function()
            s = serial.load("/tmp/dump.bin")
            assert.are.same(t, s)
        end)
        it("should dump/load simple data types", function()
            for _,v in pairs({25, 0.8534, false, nil, "hello", {1, 2, 3}}) do
                serial.dump(v, "/tmp/dump.bin")
                assert.are.same(v, serial.load("/tmp/dump.bin"))
            end
        end)
        it("should dump/load multiple variables", function()
            local len = serial.dump(t, 15, nil, true, "lua", "/tmp/dump.bin")
            local a, b, c, d, e = serial.load("/tmp/dump.bin")
            assert.are.same(t, a)
            assert.are.same(15, b)
            assert.are.same(nil, c)
            assert.are.same(true, d)
            assert.are.same("lua", e)
        end)
    end)
end)

describe("benchmark", function()
    local addressbook = {
	    name = "Alice",
	    id = 12345,
	    phone = {
		    { number = "1301234567" },
		    { number = "87654321", type = "WORK" },
	    }
    }
    it("should unpack to free memory", function()
        for i=1, 1000 do
            local u = serial.pack(addressbook)
            local t = serial.unpack(u)
        end
    end)
    it("need not to unpack after serialization", function()
        for i=1, 100 do
            local p = serial.pack(addressbook)
            local s = serial.serialize(p)
            local r = serial.deserialize(s)
        end
    end)
    it("need not to pack when dumping", function()
        for i=1, 10 do
            serial.dump(addressbook, "/tmp/dump.bin")
            serial.load("/tmp/dump.bin")
        end
    end)
end)

