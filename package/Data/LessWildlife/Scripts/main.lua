-- Configurable wildlife population reducer. See LICENSE.txt and UPSTREAM.json.
local here=assert(debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])'))
local root=here..'../'
local BOAR='/Game/_Dawnwalker/Combat/Enemies/Boar/NPCDef_Boar_Base.NPCDef_Boar_Base_C'
local WOLF='/Game/_Dawnwalker/Combat/Enemies/Wolf/NPCDef_Wolf_Base.NPCDef_Wolf_Base_C'
local HOOK='/Script/Population.PopulationArea:OnBeginOverlapOuterBox'
local MAX_ENTRIES,FRAME_ENTRIES,CACHE_LIMIT=64,128,4096
local cache,slots,count,prune={}, {},0,1
local system,frameFn,frame,frameEntries,frameTime
local debugLogging,populationPercent=false,40
local reduceBoars,reduceWolves=true,true
local stats,lastSummary={},nil
local function log(s)print('[Less Wildlife] '..s..'\n')end
local function valid(o)return o~=nil and o:IsValid()==true end
local function method(o,name)
 local f=o[name]
 if type(f)=='function' then return f end
 if type(f)=='userdata' and f:type()=='UFunction' and f:IsValid() then return f end
end
local function percentage(value)
 if type(value)=='number' and value>=1 and value<=100 and value%1==0 then return value end
end
local function readSettings()
 local path=root..'settings.ini'
 local f,why,code=io.open(path,'r')
 if f then
  local data=f:read('*a'):gsub('^\239\187\191','');f:close()
  local section
  for line in data:gmatch('[^\r\n]+') do
   section=line:match('^%s*%[([^%]]+)%]') or section
   if section=='LessWildlife' then
    local value=line:match('^%s*debugLogging%s*=%s*([^;#]+)')
    if value then debugLogging=value:match('^%s*1%s*$')~=nil end
    value=line:match('^%s*populationPercent%s*=%s*([^;#]+)')
    if value then populationPercent=percentage(tonumber(value)) or populationPercent end
    value=line:match('^%s*reduceBoars%s*=%s*([^;#]+)')
    if tonumber(value)==0 or tonumber(value)==1 then reduceBoars=tonumber(value)==1 end
    value=line:match('^%s*reduceWolves%s*=%s*([^;#]+)')
    if tonumber(value)==0 or tonumber(value)==1 then reduceWolves=tonumber(value)==1 end
   end
  end
 elseif code==2 then
  local output=io.open(path,'w')
  if output then output:write('[LessWildlife]\npopulationPercent = 40\nreduceBoars = 1\nreduceWolves = 1\ndebugLogging = 0\n');output:close() end
 end
end
local function record(key,n)
 if debugLogging then stats[key]=(stats[key] or 0)+(n or 1) end
end
local function summary()
 if not debugLogging then return end
 local now=os.clock()
 if lastSummary and now-lastSummary<10 then return end
 lastSummary=now
 log(string.format('overlaps=%d cached=%d entries=%d boars=%d wolves=%d deferred=%d failures=%d maxMs=%.3f',
  stats.overlaps or 0,stats.cached or 0,stats.entries or 0,stats.boars or 0,stats.wolves or 0,
  stats.deferred or 0,stats.failures or 0,stats.maxMs or 0))
 stats={}
end
local function pathOf(value)
 -- Owned soft-reference path; no class load or reflected Kismet conversion.
 -- Keep these temporary wrappers within this single callback.
 local id=value:GetObjectID()
 local path=id:GetAssetPathName():ToString()
 if path~=BOAR and path~=WOLF then return end
 if id:GetSubPathString():ToString()~='' then return end
 return path
end
local function reduced(n,percent)
 if n<=1 then return n end
 return math.max(1,math.floor(n*percent/100+.5))
end
local function capacity()
 if count<CACHE_LIMIT then return true end
 -- Inspect a fixed number of expired wrappers; never walk the whole cache.
 for _=1,8 do
  local entry=slots[prune]
  if entry and (not valid(entry.object) or not valid(entry.world)) then
   if cache[entry.address]==entry then cache[entry.address]=nil end
   slots[prune]=nil;count=count-1
  end
  prune=prune%CACHE_LIMIT+1
 end
 return count<CACHE_LIMIT
end
local function remember(area,world,address,entry)
 if entry then return entry end
 -- Free slot search uses Lua-owned state only and stops at a known free slot.
 local slot=prune
 for _=1,CACHE_LIMIT do
  if not slots[slot] then break end
  slot=slot%CACHE_LIMIT+1
 end
 entry={object=area,world=world,address=address,edits={},slot=slot}
 cache[address]=entry;slots[slot]=entry;count=count+1;prune=slot%CACHE_LIMIT+1
 return entry
end
local function apply(area)
 if not valid(area) then return end
 local address=area:GetAddress()
 local entry=cache[address]
 local percent=populationPercent
 local settingsKey=percent+(reduceBoars and 128 or 0)+(reduceWolves and 256 or 0)
 if entry and valid(entry.object) and valid(entry.world) then
  if entry.blocked or (entry.done and entry.settingsKey==settingsKey)
   or (entry.attemptKey==settingsKey and (entry.failures or 0)>=3) then record('cached');return end
 elseif entry then
  slots[entry.slot]=nil;cache[address]=nil;count=count-1;entry=nil
 end
 if not entry and not capacity() then record('deferred');return end
 if not valid(system) then error('frame clock is unavailable') end
 local nowFrame=frameFn(system)
 if nowFrame~=frame then frame=nowFrame;frameEntries=0;frameTime=0 end
 -- The pre-hook must finish before the game spawns the herd. Do not schedule
 -- late population writes. Oversized/burst areas keep current values until another
 -- overlap rather than doing unbounded work or applying half an area.
 local items=area.Entries
 local size=#items
 if size>MAX_ENTRIES or frameEntries+size>FRAME_ENTRIES or frameTime>=.002 then
  record('deferred');return
 end
 frameEntries=frameEntries+size
 local world=area:GetWorld()
 if not valid(world) then record('deferred');return end
 entry=remember(area,world,address,entry)
 if entry.attemptKey~=settingsKey then
  entry.attemptKey=settingsKey;entry.failures=0;entry.done=false
 end
 local started=os.clock()
 local ok,reason=pcall(function()
  local pending={}
  for i=1,size do
   local item=items[i]
   local path=pathOf(item.PawnDefinition)
   local previous=entry.edits[i]
   record('entries')
   if previous and previous.path~=path then previous.released=true end
   if path then
    local quantity,maximum=item.Quantity,item.MaxQuantity
    assert(type(quantity)=='number' and quantity==quantity and quantity>=0 and quantity%1==0
     and type(maximum)=='number' and maximum==maximum and maximum>=0 and maximum%1==0,'invalid herd quantities')
    if previous then
     -- Yield ownership if the game or another mod changed this entry.
     if quantity~=previous.quantity or maximum~=previous.maximum then previous.released=true end
    else
     previous={path=path,originalQ=quantity,originalM=maximum,quantity=quantity,maximum=maximum}
     entry.edits[i]=previous
    end
    if not previous.released then
     -- Recompute from the original population, including after Apply or retry.
     local enabled=(path==BOAR and reduceBoars) or (path==WOLF and reduceWolves)
     local q=enabled and reduced(previous.originalQ,percent) or previous.originalQ
     local m=enabled and math.max(q,reduced(previous.originalM,percent)) or previous.originalM
     if q~=quantity or m~=maximum then pending[#pending+1]={index=i,path=path,q=q,m=m,oldQ=quantity,oldM=maximum} end
    end
   end
  end
  -- Planning succeeded before the first mutation. No borrowed entry survives
  -- this callback; retained edits are only owned numbers and strings.
  for _,change in ipairs(pending) do
   local item=items[change.index]
   local wrote=pcall(function()
    item.Quantity=change.q;item.MaxQuantity=change.m
    assert(item.Quantity==change.q and item.MaxQuantity==change.m,'herd write readback failed')
   end)
   if not wrote then
    local rolled=pcall(function()item.Quantity=change.oldQ;item.MaxQuantity=change.oldM end)
    if not rolled or item.Quantity~=change.oldQ or item.MaxQuantity~=change.oldM then entry.blocked=true end
    error('herd write failed; restoration attempted')
   end
   local saved=entry.edits[change.index];saved.quantity=change.q;saved.maximum=change.m
   record(change.path==BOAR and 'boars' or 'wolves')
  end
  entry.settingsKey=settingsKey;entry.done=true
 end)
 local elapsed=os.clock()-started;frameTime=frameTime+elapsed
 if debugLogging then stats.maxMs=math.max(stats.maxMs or 0,elapsed*1000) end
 if not ok then
  entry.failures=entry.failures+1;record('failures')
  if debugLogging and entry.failures==1 then log('Area left alone after failure: '..tostring(reason)) end
 end
end
readSettings()
-- A missing optional menu does not affect population reduction.
pcall(function()
 local api=dofile(here..'dmm_api.lua')
 api.subscribe('Local_LessWildlife',function(values)
  populationPercent=percentage(values.populationPercent) or populationPercent
  if values.reduceBoars==0 or values.reduceBoars==1 then reduceBoars=values.reduceBoars==1 end
  if values.reduceWolves==0 or values.reduceWolves==1 then reduceWolves=values.reduceWolves==1 end
  if values.debugLogging~=nil then debugLogging=values.debugLogging==1 end
  stats={};lastSummary=nil
 end)
end)
local hookReady,initializing,tries=false,false,0
local initialize
initialize=function()
 initializing=false
 if hookReady then return end
 tries=tries+1
 local ready,reason=pcall(function()
  system=StaticFindObject('/Script/Engine.Default__KismetSystemLibrary')
  assert(valid(system),'KismetSystemLibrary unavailable')
  frameFn=assert(method(system,'GetFrameCount'),'GetFrameCount unavailable')
  local before,after=RegisterHook(HOOK,function(context)
   record('overlaps')
   local ok=pcall(function()apply(context:get())end)
   if not ok then record('failures') end
   summary()
  end)
  assert(type(before)=='number' and before>0 and type(after)=='number' and after>0,'population hook unavailable')
  hookReady=true
 end)
 if not ready then
  if tries<8 and type(ExecuteInGameThreadWithDelay)=='function' then
   initializing=true;ExecuteInGameThreadWithDelay(250,initialize)
  else log('Population reduction unavailable: '..tostring(reason)) end
 end
end
if type(RegisterLoadMapPostHook)=='function' then
 pcall(RegisterLoadMapPostHook,function()
  if not hookReady and not initializing then tries=0;initializing=true;ExecuteInGameThread(initialize) end
 end)
end
initializing=true;ExecuteInGameThread(initialize)
