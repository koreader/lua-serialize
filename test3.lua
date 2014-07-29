local s = require "serialize"

local var = {"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z"}
local function RandVar(bit)
	bit = bit or 1
	local t = {}
	for i=1,bit do
		table.insert(t,var[math.random(1,26)])
	end
	return table.concat(t)
end

local function CreateGuild(num)
	local GDB = {}
	for i=1,num do
		local o = {}									--create a Guild Obj
		o.Id = #GDB + 1
		o.Name = RandVar(16)
		o.Money = 1000
		o.Level = 1
		o.Notice = RandVar(64)
		o.LeaderName = RandVar(16)
		o.Building = {1,0,0,0,0}
		o.BuildCD = 0
		o.Force = 0
		o.Manorial = 0
		o.PositionNick ={}
		o.Tech ={}
		o.Fuli = {}
		o.Member={}
		o.College = {}
		o.College.Skills = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}
		o.CollegeCD = 0
		o.DailyInfo = {}
		o.MissionTimes = 0
		o.PetRoom = {}
		for i=1,768 do
			table.insert(o.Member,{guid=o.Id*i,name=RandVar(16), menpai=math.random(0,4), level=math.random(1,65), 
				position=math.random(1,6), contribution=math.random(0,1000), jointime=os.time(), offlinetime=0, juntuan=true, trader=false, online=true})
		end
		o.Apply={}
		o.History={}
		o.Guard = {}
		o.Guard.GuardVesion = 0
		o.CreateTime = os.time()
		o.Valid = true
		table.insert(GDB,o)							--add obj to GDB
	end
	return GDB
end

local o = {}
o.GDB = CreateGuild(512)
local pack = s.pack(o)
local seri,len = s.serialize(pack)
s.writeserifile("Guild.db",len,seri)

print("-----------------------------")

local seri,len = s.readserifile("Guild.db")
local t = s.deserialize(seri)

for k,v in pairs(t.GDB) do
	print(v.Id,v.Name)
end
