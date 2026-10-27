#pragma once
#include "NativeContract.hpp"
namespace LessWildlife::RespawnContract {
inline bool validate(std::wstring& error){return NativeContract::validate(error);}
inline void* address(size_t index){
    constexpr NativeContract::Id ids[]={NativeContract::Id::Queue,NativeContract::Id::Eligibility,NativeContract::Id::Attempt,NativeContract::Id::Clock};
    return index<std::size(ids)?NativeContract::address(ids[index]):nullptr;
}
}
