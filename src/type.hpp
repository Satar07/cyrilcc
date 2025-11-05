#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/_types/_ucontext.h>
#include <unordered_map>
#include <utility>
#include <vector>

// --- 枚举定义 ---
enum class PrimitiveType {
    VOID,
    I1,  // Bool
    I8,  // Char
    I32, // Int
    I64
};

enum class TypeKind { PRIMITIVE, POINTER, ARRAY, STRUCT, FUNCTION };

class IRType;
struct StructField {
    std::string name;
    IRType *type;
    int index;
};

// --- IRType 类定义 ---
class IRType {
  public:
    TypeKind kind;

  private:
    PrimitiveType prim_type = PrimitiveType::VOID;
    IRType *base_type = nullptr; // 用于 Pointer/Array
    size_t array_size = 0;       // 用于 Array
    std::string struct_name;
    std::vector<StructField> struct_fields;

    // --- 静态缓存 ---
    static std::unordered_map<std::string, std::unique_ptr<IRType>> struct_cache;

    // --- 私有构造函数 ---
    explicit IRType(PrimitiveType pt) : kind(TypeKind::PRIMITIVE), prim_type(pt) {}
    explicit IRType(TypeKind k, IRType *base) : kind(k), base_type(base) {}
    IRType(IRType *base, size_t size) : kind(TypeKind::ARRAY), base_type(base), array_size(size) {}
    IRType(std::string name, std::vector<StructField> fields)
        : kind(TypeKind::STRUCT), struct_name(std::move(name)), struct_fields(std::move(fields)) {}

  public:
    IRType(const IRType &) = delete;
    IRType &operator=(const IRType &) = delete;

    // --- 辅助查询 ---
    bool is_void() const {
        return kind == TypeKind::PRIMITIVE && prim_type == PrimitiveType::VOID;
    }
    bool is_int() const {
        return kind == TypeKind::PRIMITIVE && prim_type == PrimitiveType::I32;
    }
    bool is_char() const {
        return kind == TypeKind::PRIMITIVE && prim_type == PrimitiveType::I8;
    }
    bool is_bool() const {
        return kind == TypeKind::PRIMITIVE && prim_type == PrimitiveType::I1;
    }
    bool is_primitive() const {
        return kind == TypeKind::PRIMITIVE;
    }
    bool is_pointer() const {
        return kind == TypeKind::POINTER;
    }
    bool is_array() const {
        return kind == TypeKind::ARRAY;
    }
    bool is_struct() const {
        return kind == TypeKind::STRUCT;
    }

    PrimitiveType get_primitive_type() const {
        return prim_type;
    }
    IRType *get_pointee_type() const {
        return base_type;
    } // 适用于 Pointer
    IRType *get_array_element_type() const {
        return base_type;
    } // 适用于 Array
    int get_array_size() const {
        return static_cast<int>(array_size);
    }
    const std::string &get_struct_name() const {
        return struct_name;
    }

    const StructField *get_field(const std::string &name) const {
        if (kind != TypeKind::STRUCT)
            throw std::runtime_error("get_field called on non-struct type");
        for (const auto &f : struct_fields) {
            if (f.name == name) return &f;
        }
        throw std::runtime_error("Field not found in struct: " + name);
    }

    const std::vector<StructField> &get_fields() const {
        if (kind != TypeKind::STRUCT)
            throw std::runtime_error("get_fields called on non-struct type");
        return struct_fields;
    }

    int get_field_index(const std::string &name) {
        if (kind != TypeKind::STRUCT)
            throw std::runtime_error("get_field_index called on non-struct type");
        for (size_t i = 0; i < struct_fields.size(); ++i) {
            if (struct_fields.at(i).name == name) {
                return static_cast<int>(i);
            }
        }
        throw std::runtime_error("Field not found in struct: " + name);
    }

    int size() {
        // 我直接对齐
        if (is_char() or is_int() or is_pointer()) return 4;
        if (is_array()) {
            return get_array_size() * get_array_element_type()->size();
        }
        if (is_struct()) {
            int total_size = 0;
            for (const auto &field : get_fields()) {
                total_size += field.type->size();
            }
            return total_size;
        }
        throw std::runtime_error("Cannot get size of unknown type: " + to_string());
    }

    int get_field_offset(int field_index) {
        if (!is_struct()) {
            throw std::runtime_error("Attempt to get field offset from non-struct");
        }
        int current_offset = 0;
        const auto &fields = get_fields();
        if (field_index >= (int)fields.size()) {
            throw std::runtime_error("Field index out of bounds");
        }
        for (int i = 0; i < field_index; ++i) {
            current_offset += fields[i].type->size();
        }
        return current_offset;
    }

    // --- 调试输出 ---
    std::string to_string() const {
        switch (kind) {
            case TypeKind::PRIMITIVE:
                switch (prim_type) {
                    case PrimitiveType::VOID: return "void";
                    case PrimitiveType::I1: return "i1";
                    case PrimitiveType::I8: return "i8";
                    case PrimitiveType::I32: return "i32";
                    case PrimitiveType::I64: return "i64";
                }
                return "unknown_primitive";
            case TypeKind::POINTER: return base_type->to_string() + "*";
            case TypeKind::ARRAY:
                return "[" + std::to_string(array_size) + " x " + base_type->to_string() + "]";
            case TypeKind::STRUCT: return "struct " + struct_name;
            default: return "unknown_type";
        }
    }

    // --- 静态工厂方法 ---
    static IRType *get_void() {
        static IRType t(PrimitiveType::VOID);
        return &t;
    }
    static IRType *get_i1() {
        static IRType t(PrimitiveType::I1);
        return &t;
    }
    static IRType *get_i8() {
        static IRType t(PrimitiveType::I8);
        return &t;
    }
    static IRType *get_i32() {
        static IRType t(PrimitiveType::I32);
        return &t;
    }

    // 持久化的，随便存指针
    static IRType *get_pointer(IRType *base) {
        static std::map<IRType *, std::unique_ptr<IRType>> cache;
        if (auto it = cache.find(base); it != cache.end()) return it->second.get();
        IRType *t = new IRType(TypeKind::POINTER, base);
        cache[base] = std::unique_ptr<IRType>(t);
        return t;
    }

    static IRType *get_char_ptr() {
        return get_pointer(get_i8());
    }

    static IRType *get_array(IRType *base, size_t size) {
        static std::map<std::pair<IRType *, size_t>, std::unique_ptr<IRType>> cache;
        if (auto it = cache.find({ base, size }); it != cache.end()) return it->second.get();
        IRType *t = new IRType(base, size);
        cache[{ base, size }] = std::unique_ptr<IRType>(t);
        return t;
    }

    static IRType *register_struct(std::string name, std::vector<StructField> fields) {
        auto it = struct_cache.find(name);
        if (it != struct_cache.end()) {
            return it->second.get();
        }
        // 为字段设置索引
        for (size_t i = 0; i < fields.size(); ++i) {
            fields[i].index = i;
        }
        auto *t = new IRType(name, std::move(fields));
        struct_cache[name] = std::unique_ptr<IRType>(t);
        return t;
    }

    static IRType *get_struct(const std::string &name) {
        auto it = struct_cache.find(name);
        if (it != struct_cache.end()) return it->second.get();
        throw std::runtime_error("Struct type not found: " + name);
    }
};
