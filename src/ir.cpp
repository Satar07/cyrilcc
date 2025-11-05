#include "ir.hpp"
#include "type.hpp"
#include <memory>
#include <string>
#include <unordered_map>

std::unordered_map<std::string, std::unique_ptr<IRType>> IRType::struct_cache;
