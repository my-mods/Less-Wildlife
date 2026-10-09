-- File-backed settings shared by startup and the optional Mod Setting Menu.
local M={}
M.keys={'populationPercent','reduceBoars','reduceWolves','boarReplacementChance','wolfReplacementChance','boarBandits','boarGuards','boarBloodGuards','boarVidmo','boarKobolds','boarNoSpawn','wolfBandits','wolfGuards','wolfBloodGuards','wolfVidmo','wolfKobolds','wolfNoSpawn','banditGroupMin','banditGroupMax','guardGroupMin','guardGroupMax','bloodGuardGroupMin','bloodGuardGroupMax','vidmoGroupMin','vidmoGroupMax','koboldGroupMin','koboldGroupMax','logLevel'}
M.defaults={populationPercent=100,reduceBoars=1,reduceWolves=1,boarReplacementChance=0,wolfReplacementChance=0,boarBandits=1,boarGuards=1,boarBloodGuards=1,boarVidmo=1,boarKobolds=1,boarNoSpawn=0,wolfBandits=1,wolfGuards=1,wolfBloodGuards=1,wolfVidmo=1,wolfKobolds=1,wolfNoSpawn=0,banditGroupMin=3,banditGroupMax=6,guardGroupMin=2,guardGroupMax=4,bloodGuardGroupMin=2,bloodGuardGroupMax=4,vidmoGroupMin=1,vidmoGroupMax=2,koboldGroupMin=4,koboldGroupMax=10,logLevel=2}
local function value(key,raw)
 local n=tonumber(raw)
 if not n or n~=n or n%1~=0 then return end
 if key:match('GroupMin$') or key:match('GroupMax$') then
  return math.max(1,math.min(10,n))
 elseif key=='populationPercent' then
  if n>=1 and n<=200 then return math.max(10,n) end
 elseif key=='logLevel' then
  if n>=0 and n<=4 then return n end
 elseif key=='boarReplacementChance' or key=='wolfReplacementChance' then
  if n>=0 and n<=100 then return n end
 elseif n==0 or n==1 then return n end
end
function M.ranges(values)
 local result={}
 for _,key in ipairs({'bandit','guard','bloodGuard','vidmo','kobold'})do
  local a=value(key..'GroupMin',values[key..'GroupMin']) or M.defaults[key..'GroupMin']
  local b=value(key..'GroupMax',values[key..'GroupMax']) or M.defaults[key..'GroupMax']
  result[#result+1]=math.min(a,b);result[#result+1]=math.max(a,b)
 end
 return result
end
local function lines(data)
 local result,pos={},1
 while pos<=#data do
  local finish=data:find('\n',pos,true)
  local body=data:sub(pos,finish and finish-1 or #data)
  local ending=finish and '\n' or ''
  if ending~='' and body:sub(-1)=='\r' then body=body:sub(1,-2);ending='\r\n' end
  result[#result+1]={body=body,ending=ending};pos=finish and finish+1 or #data+1
 end
 return result
end
local function assignment(line)
 if line:match('^%s*[;#]') then return end
 local prefix,key,equal,raw,suffix=line:match('^(%s*)([^=]-)(%s*=%s*)([^;#]*)(.*)$')
 if not key then return end
 key=key:match('^%s*(.-)%s*$')
 if M.defaults[key]==nil then return end
 return key,prefix,equal,raw,suffix
end
local function header(line)
 local section=line:match('^%s*%[([^%]]+)%]%s*$')
 return section and section:match('^%s*(.-)%s*$')
end
function M.upgrade(original)
 local here=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])'))
 local data=dofile(here..'ModLogLevels.lua').normalizeIniHeaders(original or '')
 assert(#data<=1048576,'settings exceed 1 MiB')
 local list=lines(data);local settings={}
 for k,v in pairs(M.defaults)do settings[k]=v end
 local section
 for _,line in ipairs(list)do
  section=header(line.body) or section
  if section=='LessWildlife' then
   local key,_,_,raw=assignment(line.body)
   if key then settings[key]=value(key,raw) or settings[key] end
  end
 end
 -- Explicit new setting wins; preserve legacy line/comments as import-only.
 local explicit,legacy,legacyCount,invalidLegacy=nil,nil,0,false;section=nil
 for _,line in ipairs(list)do
  section=header(line.body) or section
  if section=='LessWildlife' then
   local n=line.body:match('^%s*logLevel%s*=%s*([^;#]*)')
   if n then assert(not explicit,'Duplicate logLevel');explicit=true;assert(value('logLevel',n),'Invalid logLevel') end
   local old=line.body:match('^%s*debugLogging%s*=%s*([^;#]*)')
   if old then
    legacyCount=legacyCount+1;legacy=tonumber(old)
    invalidLegacy=invalidLegacy or (legacy~=0 and legacy~=1)
   end
  end
 end
 if not explicit then
  assert(legacyCount<=1,'Duplicate legacy logging')
  assert(not invalidLegacy,'Invalid legacy logging')
  settings.logLevel=legacy==1 and 4 or 2
 end
 local seen,out={},{};section=nil
 local newline=data:find('\r\n',1,true) and '\r\n' or '\n'
 local sectionFound=false
 for _,line in ipairs(list)do
  local heading=header(line.body)
  if heading then section=heading;if heading=='LessWildlife' then sectionFound=true end end
  local body=line.body
  if section=='LessWildlife' then
   local key,prefix,equal,raw,suffix=assignment(body)
   if key then
    if seen[key] then body='; Less Wildlife duplicate: '..body
    else
     seen[key]=true
     local leading,_,trailing=raw:match('^(%s*)(.-)(%s*)$')
     body=prefix..key..equal..leading..tostring(settings[key])..trailing..suffix
    end
   end
  end
  out[#out+1]=body..line.ending
 end
 local missing={}
 for _,key in ipairs(M.keys)do if not seen[key]then missing[#missing+1]=key..' = '..settings[key]..newline end end
 if #missing>0 then
  -- Append a section only when necessary; existing comments and unrelated
  -- sections remain in place. The menu requires one assignment per key.
  if #out>0 and out[#out]:sub(-1)~='\n' then out[#out]=out[#out]..newline end
  if not sectionFound or section~='LessWildlife' then out[#out+1]='[LessWildlife]'..newline end
  for _,line in ipairs(missing)do out[#out+1]=line end
 end
 return table.concat(out),settings
end
M.fs={}
function M.fs.read(path)
 local file,why,code=io.open(path,'rb')
 if not file then if code==2 then return nil end;error(why or 'settings read failed') end
 local data,readError=file:read(1048577);local closed,err=file:close()
 if data==nil and readError==nil then data='' end
 assert(data and #data<=1048576 and closed,readError or err or 'settings read failed');return data
end
function M.fs.write(path,data)
 local file,why=io.open(path,'wb');assert(file,why)
 local wrote,err=file:write(data);local closed,closeError=file:close()
 assert(wrote and closed,err or closeError or 'settings write failed')
end
function M.fs.rename(a,b)local ok,why=os.rename(a,b);assert(ok,why)end
function M.fs.remove(path)local ok,why=os.remove(path);assert(ok,why)end
function M.replace(path,original,updated,fs)
 local temporary,backup=path..'.less-wildlife-upgrade.tmp',path..'.less-wildlife-upgrade.bak'
 assert(fs.read(temporary)==nil and fs.read(backup)==nil,'previous settings upgrade needs recovery')
 assert(fs.read(path)==original,'settings changed before upgrade')
 local wrote,why=pcall(fs.write,temporary,updated)
 if not wrote then pcall(fs.remove,temporary);error(why) end
 if fs.read(temporary)~=updated or fs.read(path)~=original then pcall(fs.remove,temporary);error('settings upgrade verification failed') end
 if original~=nil then
  local moved,err=pcall(fs.rename,path,backup)
  if not moved then pcall(fs.remove,temporary);error(err) end
 end
 local installed,err=pcall(fs.rename,temporary,path)
 if not installed then
  if original~=nil then
   local restored,restoreError=pcall(fs.rename,backup,path)
   if not restored then error(tostring(err)..'; original retained at '..backup..': '..tostring(restoreError)) end
  end
  pcall(fs.remove,temporary);error(err)
 end
 assert(fs.read(path)==updated,'settings readback failed; original backup retained')
 if original~=nil then fs.remove(backup) end
end
function M.load(path,fs)
 fs=fs or M.fs
 local settings={};for k,v in pairs(M.defaults)do settings[k]=v end
 local ok,why=pcall(function()
  local original=fs.read(path)
  local updated;updated,settings=M.upgrade(original)
  if updated~=original then M.replace(path,original,updated,fs) end
 end)
 if not ok then settings.boarReplacementChance=0;settings.wolfReplacementChance=0 end
 return settings,ok,why
end
return M
