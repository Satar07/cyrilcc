Program:
  VarDeclarations:
    Type: int
    Define var: global_int
  VarDeclarations:
    Type: char
    Define var: global_char
  FunctionDefinition: main -> int
    Parameters:
    Body:
      VarDeclarations:
        Type: int
        Define var: local_int
      VarDeclarations:
        Type: char
        Define var: local_char
      Input:
        VarRef: local_int
      Assign:
        VarRef: local_char
        Char: 'c'
      If:
        Condition:
            VarRef: local_int
          ==
            Int: 1
        Then:
          Assign:
            VarRef: local_char
            Char: 'd'
      While:
        Condition:
            VarRef: local_int
          >
            Int: 1
        Body:
          Assign:
            VarRef: local_char
            Char: 'e'
      Assign:
        VarRef: local_int
          FuncCall: max
            Args:
              VarRef: local_int
              Int: 3
        +
          Int: 123
      Output:
        VarRef: local_char
      Assign:
        VarRef: global_int
        VarRef: local_int
      Assign:
        VarRef: global_char
        VarRef: local_char
  FunctionDefinition: max -> int
    Parameters:
      Param: a (int)
      Param: b (int)
    Body:
      If:
        Condition:
            VarRef: a
          <
            VarRef: b
        Then:
          Return:
            VarRef: b
        Else:
          Return:
            VarRef: a
--- IR Module ---

--- Globals ---
@global_int = global i32
@global_char = global i8

--- Functions ---
define i32 @main() {
%L0:
  ptr i32 %0 = alloca
  ptr i8 %1 = alloca
  i32 %2 = input_int
  store i32 %2, ptr i32 %0
  store i8 'c', ptr i8 %1
  i32 %3 = load ptr i32 %0
  test i32 %3, i32 1
  brz %L1
%L3:
  br %L2
%L1:
  store i8 'd', ptr i8 %1
  br %L2
%L2:
  br %L4
%L4:
  i32 %4 = load ptr i32 %0
  test i32 %4, i32 1
  brgt %L5
%L7:
  br %L6
%L5:
  store i8 'e', ptr i8 %1
  br %L4
%L6:
  i32 %5 = load ptr i32 %0
  i32 %6 = call i32 @max, i32 %5, i32 3
  i32 %7 = add i32 %6, i32 123
  store i32 %7, ptr i32 %0
  i8 %8 = load ptr i8 %1
  output_char i8 %8
  i32 %9 = load ptr i32 %0
  store i32 %9, ptr i32 @global_int
  i8 %10 = load ptr i8 %1
  store i8 %10, ptr i8 @global_char
  ret i32 0
}

define i32 @max(i32 %0, i32 %2) {
%L8:
  ptr i32 %1 = alloca
  store i32 %0, ptr i32 %1
  ptr i32 %3 = alloca
  store i32 %2, ptr i32 %3
  i32 %4 = load ptr i32 %1
  i32 %5 = load ptr i32 %3
  test i32 %4, i32 %5
  brlt %L9
%L12:
  br %L11
%L9:
  i32 %6 = load ptr i32 %3
  ret i32 %6
  br %L10
%L11:
  i32 %7 = load ptr i32 %1
  ret i32 %7
  br %L10
%L10:
  ret i32 0
}

--- End Module ---
