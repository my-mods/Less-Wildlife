#pragma once
#include "NativeContract.hpp"
namespace LessWildlife::LifecycleContract {
inline bool validate(std::wstring& error){return NativeContract::validate(error);}
}
