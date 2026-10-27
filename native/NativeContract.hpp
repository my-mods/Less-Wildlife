#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string>
#include "ContractData.hpp"

namespace LessWildlife::NativeContract {
inline std::array<uint32_t,static_cast<size_t>(Id::Count)> resolved{};
inline bool ready{};
inline unsigned char* image{};
inline size_t imageSize{};
inline bool range(const unsigned char* base,size_t size,uint32_t offset,size_t length,bool executable=true) {
    if(offset>=size||length>size-offset)return false;
    auto p=base+offset;auto end=p+length;
    while(p<end){
        MEMORY_BASIC_INFORMATION m{};
        if(!VirtualQuery(p,&m,sizeof(m))||!m.RegionSize||m.AllocationBase!=base||m.State!=MEM_COMMIT
            ||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
        const auto allowed=executable?(PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)
            :(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY);
        if(!(m.Protect&allowed))return false;
        auto next=static_cast<const unsigned char*>(m.BaseAddress)+m.RegionSize;
        if(next<=p)return false;p=std::min(next,end);
    }
    return true;
}
inline bool bytesMatch(const unsigned char* actual,const Probe& p) {
    for(size_t i=0;i<p.size;++i)if((actual[i]&p.mask[i])!=(p.bytes[i]&p.mask[i]))return false;
    return true;
}
inline bool matches(uint32_t at,const Spec& s) {
    if(!range(image,imageSize,at,16))return false;
    for(size_t i=0;i<s.count;++i){const auto& p=s.probes[i];
        if(p.offset>imageSize||at>imageSize-p.offset||!range(image,imageSize,at+p.offset,p.size)
            ||!bytesMatch(image+at+p.offset,p))return false;
    }
    return true;
}
// The recorded RVA is only a fast hint. Moved functions are found from their
// required field/argument relationships; zero or multiple candidates fail closed.
inline uint32_t locate(const Spec& spec,const IMAGE_NT_HEADERS64* nt) {
    if(matches(spec.hint,spec))return spec.hint;
    const Probe* anchor=&spec.probes[0];size_t best{};
    for(size_t i=0;i<spec.count;++i){size_t n{};for(size_t j=0;j<spec.probes[i].size;++j)n+=spec.probes[i].mask[j]!=0;
        if(n>best){best=n;anchor=&spec.probes[i];}}
    if(best<6)return 0;
    size_t first{};while(first<anchor->size&&!anchor->mask[first])++first;
    uint32_t found{};
    auto sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){const auto& s=sections[i];
        if(!(s.Characteristics&IMAGE_SCN_MEM_EXECUTE)||!s.Misc.VirtualSize)continue;
        if(!range(image,imageSize,s.VirtualAddress,s.Misc.VirtualSize))continue;
        auto begin=image+s.VirtualAddress;auto end=begin+s.Misc.VirtualSize;
        const unsigned char* cursor=begin;
        while(cursor<end){
            auto hit=static_cast<const unsigned char*>(std::memchr(cursor,anchor->bytes[first],static_cast<size_t>(end-cursor)));
            if(!hit)break;cursor=hit+1;
            const auto pos=static_cast<size_t>(hit-image);
            if(pos<first+anchor->offset)continue;
            const auto anchorAt=pos-first;
            if(anchorAt<s.VirtualAddress||anchor->size>size_t(end-image)-anchorAt||!bytesMatch(image+anchorAt,*anchor))continue;
            const auto candidate=anchorAt-anchor->offset;
            if(candidate>UINT32_MAX||!matches(static_cast<uint32_t>(candidate),spec))continue;
            if(found&&found!=candidate)return 0;found=static_cast<uint32_t>(candidate);
        }
    }
    return found;
}
inline void* address(Id id) {return ready?image+resolved[static_cast<size_t>(id)]:nullptr;}
inline bool relative(Id from,uint32_t offset,uint32_t size,uint32_t displacement,Id to) {
    const auto at=resolved[static_cast<size_t>(from)]+offset;
    if(!range(image,imageSize,at,size)||displacement+4>size)return false;
    if(size==5 && image[at]!=0xe8 && image[at]!=0xe9)return false;
    if(size==6 && (image[at]!=0x0f || (image[at+1]&0xf0)!=0x80))return false;
    int32_t delta{};std::memcpy(&delta,image+at+displacement,4);
    return int64_t(at)+size+delta==resolved[static_cast<size_t>(to)];
}
inline void* montageTypeAddress();
inline bool validate(std::wstring& error) {
    if(ready)return true;
    image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if(!image){error=L"game image unavailable";return false;}
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>1048576){error=L"invalid image header";return false;}
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(image+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64
        ||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt->FileHeader.NumberOfSections>96){error=L"invalid x64 image";return false;}
    imageSize=nt->OptionalHeader.SizeOfImage;
    const auto sectionOffset=static_cast<size_t>(reinterpret_cast<const unsigned char*>(IMAGE_FIRST_SECTION(nt))-image);
    if(sectionOffset>=imageSize||size_t(nt->FileHeader.NumberOfSections)*sizeof(IMAGE_SECTION_HEADER)>imageSize-sectionOffset){error=L"invalid image sections";return false;}
    for(size_t i=0;i<std::size(specs);++i){
        resolved[i]=locate(specs[i],nt);
        if(!resolved[i]){error=L"cannot verify "+std::wstring(specs[i].name)+L" argument/field contract (missing or ambiguous target)";return false;}
    }
    struct Link {Id from;uint32_t offset,size,disp;Id to;};
    const Link links[]={
        {Id::Attempt,0x2369654-0x23695b4,5,1,Id::Core},
        {Id::Eligibility,0x5b60b92-0x5b60b34,5,1,Id::MemberWalk},
        {Id::Phase,0x1334926-0x13347f0,5,1,Id::Activity},
        {Id::Core,0x142da2a-0x142d808,5,1,Id::Phase},
        {Id::Complete,0x5b70021-0x5b6ff98,5,1,Id::Registry},
        {Id::FactsExists,0x1ac830f-0x1ac82ec,5,1,Id::Hash},
        {Id::FactsWrite,0x1883b33-0x1883b1c,5,1,Id::Hash},
        {Id::Core,0x142d843-0x142d808,6,2,Id::Cleanup},
        {Id::Equipment,0x16e3ca5-0x16e3c08,6,2,Id::SeedFlag}
    };
    for(const auto& link:links)if(!relative(link.from,link.offset,link.size,link.disp,link.to)){
        error=L"cannot verify "+std::wstring(specs[static_cast<size_t>(link.from)].name)+L" call/branch to "+specs[static_cast<size_t>(link.to)].name;return false;
    }
    ready=true;
    if(!montageTypeAddress()){ready=false;error=L"invalid montage type storage";return false;}
    return true;
}
inline void* resolve(std::wstring& error){return validate(error)?address(Id::Builder):nullptr;}
inline void* appearanceInitializer(){return address(Id::Appearance);}
inline void* montageTypeAddress(){
    if(!ready)return nullptr;
    auto at=resolved[static_cast<size_t>(Id::SetMontage)]+(0x1a881e4-0x1a881d4);
    int32_t delta{};std::memcpy(&delta,image+at+3,4);
    auto target=int64_t(at)+7+delta;
    if(target<0||target>UINT32_MAX||!range(image,imageSize,static_cast<uint32_t>(target),8,false))return nullptr;
    return image+target;
}
}
