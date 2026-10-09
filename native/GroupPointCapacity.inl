// The stock point generator runs once, with temporary capacity-only counts.
namespace GroupPointCapacity {
using Generate = void(*)(UObject*,int32_t,void*,void*);
Generate original{};void* target{};
void generate(UObject* area,int32_t ordinal,void* context,void* row) {
    HookTimer timing(7);
    CountEdit capacity;
    std::shared_ptr<EncounterRuntime::AreaHandle> handle;
    unsigned sourceIndex=64;
    try {
        if(active && configurationReady && !failed && GetCurrentThreadId()==gameThread && area && area->IsA(areaType)) {
            auto entries=read<Array>(area,0x360);
            require(arrayValid(entries,64) && ordinal>=0 && ordinal<entries.count,"point capacity entry mapping");
            auto table=read<UDataTable*>(area,0x370);
            require(table && table->GetRowStruct()==rowType,"point capacity table");
            unsigned position=0;
            for(auto& pair:table->GetRowMap()) {
                if(position++!=static_cast<unsigned>(ordinal))continue;
                require(pair.Value==row,"point capacity row identity");
                auto key=EncounterRuntime::identify(read<std::array<uint32_t,4>>(area,0x1e0),pair.Key);
                if(!key)break;
                handle=EncounterRuntime::findArea(*key);
                if(!handle || !handle->object.valid() || handle->object.object!=area || !handle->counts[key->row].known) {handle.reset();break;}
                require(read<Array>(area,0x350).count==0,"point capacity quest exclusion");
                auto sources=read<Array>(area,0x340);
                require(arrayValid(sources,64) && key->row<sources.count && entryReason(sources.data+key->row*0x100)==SkipReason::None,"point capacity scripted exclusion");
                auto entry=entries.data+ordinal*0x100;
                const auto& counts=handle->counts[key->row];
                auto n=std::max({groupLimit,counts.original[0],counts.original[2]});
                auto actions=read<Array>(area,0x398),spawns=read<Array>(area,0x388),montages=read<Array>(entry,0xb0);
                require(arrayValid(actions,4096) && arrayValid(spawns,4096) && arrayValid(montages,64),"point capacity arrays");
                const auto multiplier=read<float>(entry,0xc0);
                require(std::isfinite(multiplier) && multiplier>=0 && multiplier<=128,"roaming point multiplier");
                const auto actionCount=montages.count ? uint64_t(n)*montages.count+1 : static_cast<uint64_t>(n*multiplier+0.5f);
                require(uint64_t(spawns.count)+n<=4096 && uint64_t(actions.count)+actionCount<=4096,"group point capacity budget");
                capacity=CountEdit(phaseQuantity(row),entry+0x68,entry+0x6c,{n,n,n});
                require(capacity.before==counts.owned,"point capacity quantities changed externally");
                sourceIndex=key->row;capacity.apply();require(capacity.readbackMatches(),"point capacity write");
                break;
            }
        }
    } catch(const std::exception& error) {
        capacity.restore();capacity={};handle.reset();
        if(logLevel>=2 && warningCount.fetch_add(1)<8){std::string s(error.what());message(L"Group point capacity unavailable: "+std::wstring(s.begin(),s.end()),2);}
    }
    struct Restore {CountEdit& value;~Restore(){value.restore();}} restore{capacity};
    original(area,ordinal,context,row);
    capacity.restore();
    if(handle && sourceIndex<64 && handle->object.valid())handle->counts[sourceIndex].capacityReady=true;
}
void stop(){if(target){MH_DisableHook(target);MH_RemoveHook(target);target=nullptr;}}
void start(){
    auto candidate=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr))+0x16896e4;
    require(MH_CreateHook(candidate,reinterpret_cast<void*>(&generate),reinterpret_cast<void**>(&original))==MH_OK,"group point capacity hook creation");
    target=candidate;require(MH_EnableHook(target)==MH_OK,"group point capacity hook activation");
}
}
