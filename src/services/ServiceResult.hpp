#pragma once
#include <string>
#include <utility>
namespace geobr { template<class T> struct ServiceResult final { bool success=false; T data{}; std::string error; static ServiceResult ok(T v){return{true,std::move(v),{}};} static ServiceResult fail(std::string e){return{false,{},std::move(e)};} }; }
