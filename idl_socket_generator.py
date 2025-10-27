#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
IDL到Socket通信代码生成器
支持Linux环境，解析IDL接口文件，生成C++ Socket通信框架

要求:
1. 可在Linux环境下运行
2. 解析IDL标准接口文件
3. 提示接口文件语法错误
4. 生成C++进程间通信代码框架
5. 使用Socket作为通信方式
"""

import os
import sys
import re
import json
import argparse
from typing import List, Dict, Optional, Tuple
from dataclasses import dataclass, field
from enum import Enum


class IDLTokenType(Enum):
    """IDL词法标记类型"""
    INTERFACE = "interface"
    STRUCT = "struct"
    ENUM = "enum"
    TYPEDEF = "typedef"
    IN = "in"
    OUT = "out"
    INOUT = "inout"
    VOID = "void"
    IDENTIFIER = "identifier"
    NUMBER = "number"
    STRING = "string"
    LBRACE = "{"
    RBRACE = "}"
    LPAREN = "("
    RPAREN = ")"
    LBRACKET = "["
    RBRACKET = "]"
    SEMICOLON = ";"
    COMMA = ","
    COMMENT = "comment"
    EOF = "eof"
    MODULE = "module"           # OMG IDL
    SEQUENCE = "sequence"       # OMG IDL
    BOOLEAN = "boolean"         # OMG IDL
    CALLBACK = "callback"       # 回调方法关键字
    LESS = "<"                  # 用于 sequence<>
    GREATER = ">"               # 用于 sequence<>
    COLON = ":"                 # 用于作用域


@dataclass
class IDLToken:
    """IDL词法标记"""
    type: IDLTokenType
    value: str
    line: int
    column: int


@dataclass
class SyntaxError:
    """语法错误信息"""
    message: str
    line: int
    column: int
    severity: str = "error"  # error, warning


@dataclass
class IDLParameter:
    """IDL方法参数"""
    name: str
    type_name: str
    direction: str  # "in", "out", "inout"
    is_array: bool = False
    array_size: Optional[int] = None
    line: int = 0


@dataclass
class IDLMethod:
    """IDL方法定义"""
    name: str
    return_type: str
    parameters: List[IDLParameter]
    is_callback: bool = False  # 标识是否为回调方法
    line: int = 0


@dataclass
class IDLStruct:
    """IDL结构体定义"""
    name: str
    fields: List[Tuple[str, str]]  # (type, name)
    line: int = 0


@dataclass
class IDLEnum:
    """IDL枚举定义"""
    name: str
    values: List[str]
    line: int = 0


@dataclass
class IDLTypedef:
    """IDL类型定义"""
    name: str
    base_type: str  # 例如 "sequence<string>"
    line: int = 0


@dataclass
class IDLModule:
    """IDL模块定义"""
    name: str
    interfaces: List['IDLInterface'] = field(default_factory=list)
    structs: List[IDLStruct] = field(default_factory=list)
    enums: List[IDLEnum] = field(default_factory=list)
    typedefs: List[IDLTypedef] = field(default_factory=list)
    line: int = 0


@dataclass
class IDLInterface:
    """IDL接口定义"""
    name: str
    methods: List[IDLMethod]
    structs: List[IDLStruct] = field(default_factory=list)
    enums: List[IDLEnum] = field(default_factory=list)
    line: int = 0


class IDLLexer:
    """IDL词法分析器"""
    
    KEYWORDS = {
        'interface', 'struct', 'enum', 'typedef',
        'in', 'out', 'inout', 'void',
        'bool', 'char', 'short', 'int', 'long',
        'float', 'double', 'string', 'byte',
        'module', 'sequence', 'boolean',  # OMG IDL 关键字
        'callback'  # 回调方法关键字
    }
    
    def __init__(self, content: str):
        self.content = content
        self.pos = 0
        self.line = 1
        self.column = 1
        self.tokens = []
        self.errors = []
    
    def error(self, message: str):
        """记录词法错误"""
        self.errors.append(SyntaxError(message, self.line, self.column))
    
    def peek(self, offset: int = 0) -> Optional[str]:
        """查看字符但不移动位置"""
        pos = self.pos + offset
        if pos < len(self.content):
            return self.content[pos]
        return None
    
    def advance(self) -> Optional[str]:
        """读取并移动到下一个字符"""
        if self.pos < len(self.content):
            ch = self.content[self.pos]
            self.pos += 1
            if ch == '\n':
                self.line += 1
                self.column = 1
            else:
                self.column += 1
            return ch
        return None
    
    def skip_whitespace(self):
        """跳过空白字符"""
        while self.peek() and self.peek() in ' \t\n\r':
            self.advance()
    
    def skip_comment(self):
        """跳过注释"""
        if self.peek() == '/' and self.peek(1) == '/':
            # 单行注释
            while self.peek() and self.peek() != '\n':
                self.advance()
            self.advance()  # 跳过\n
            return True
        elif self.peek() == '/' and self.peek(1) == '*':
            # 多行注释
            self.advance()  # /
            self.advance()  # *
            while True:
                if self.peek() is None:
                    self.error("未闭合的多行注释")
                    return True
                if self.peek() == '*' and self.peek(1) == '/':
                    self.advance()  # *
                    self.advance()  # /
                    break
                self.advance()
            return True
        return False
    
    def read_identifier(self) -> str:
        """读取标识符"""
        start = self.pos
        while self.peek() and (self.peek().isalnum() or self.peek() == '_'):
            self.advance()
        return self.content[start:self.pos]
    
    def read_number(self) -> str:
        """读取数字"""
        start = self.pos
        while self.peek() and self.peek().isdigit():
            self.advance()
        return self.content[start:self.pos]
    
    def tokenize(self) -> List[IDLToken]:
        """词法分析，返回标记列表"""
        while self.pos < len(self.content):
            self.skip_whitespace()
            
            if self.pos >= len(self.content):
                break
            
            # 跳过注释
            if self.skip_comment():
                continue
            
            ch = self.peek()
            line, col = self.line, self.column
            
            # 单字符标记
            if ch == '{':
                self.tokens.append(IDLToken(IDLTokenType.LBRACE, ch, line, col))
                self.advance()
            elif ch == '}':
                self.tokens.append(IDLToken(IDLTokenType.RBRACE, ch, line, col))
                self.advance()
            elif ch == '(':
                self.tokens.append(IDLToken(IDLTokenType.LPAREN, ch, line, col))
                self.advance()
            elif ch == ')':
                self.tokens.append(IDLToken(IDLTokenType.RPAREN, ch, line, col))
                self.advance()
            elif ch == '[':
                self.tokens.append(IDLToken(IDLTokenType.LBRACKET, ch, line, col))
                self.advance()
            elif ch == ']':
                self.tokens.append(IDLToken(IDLTokenType.RBRACKET, ch, line, col))
                self.advance()
            elif ch == ';':
                self.tokens.append(IDLToken(IDLTokenType.SEMICOLON, ch, line, col))
                self.advance()
            elif ch == ',':
                self.tokens.append(IDLToken(IDLTokenType.COMMA, ch, line, col))
                self.advance()
            elif ch == '<':
                self.tokens.append(IDLToken(IDLTokenType.LESS, ch, line, col))
                self.advance()
            elif ch == '>':
                self.tokens.append(IDLToken(IDLTokenType.GREATER, ch, line, col))
                self.advance()
            elif ch == ':':
                self.tokens.append(IDLToken(IDLTokenType.COLON, ch, line, col))
                self.advance()
            elif ch.isalpha() or ch == '_':
                # 标识符或关键字
                ident = self.read_identifier()
                # OMG IDL 关键字映射
                keyword_map = {
                    'in': IDLTokenType.IN,
                    'out': IDLTokenType.OUT,
                    'inout': IDLTokenType.INOUT,
                    'void': IDLTokenType.VOID,
                    'interface': IDLTokenType.INTERFACE,
                    'struct': IDLTokenType.STRUCT,
                    'enum': IDLTokenType.ENUM,
                    'typedef': IDLTokenType.TYPEDEF,
                    'module': IDLTokenType.MODULE,
                    'sequence': IDLTokenType.SEQUENCE,
                    'boolean': IDLTokenType.BOOLEAN,
                    'callback': IDLTokenType.CALLBACK
                }
                if ident in keyword_map:
                    self.tokens.append(IDLToken(keyword_map[ident], ident, line, col))
                else:
                    self.tokens.append(IDLToken(IDLTokenType.IDENTIFIER, ident, line, col))
            elif ch.isdigit():
                # 数字
                num = self.read_number()
                self.tokens.append(IDLToken(IDLTokenType.NUMBER, num, line, col))
            else:
                self.error(f"无法识别的字符: '{ch}'")
                self.advance()
        
        self.tokens.append(IDLToken(IDLTokenType.EOF, "", self.line, self.column))
        return self.tokens


class IDLParser:
    """IDL语法分析器"""
    
    def __init__(self, tokens: List[IDLToken]):
        self.tokens = tokens
        self.pos = 0
        self.errors = []
        self.interfaces = []
        self.modules = []
        self.global_typedefs = []
    
    def error(self, message: str, token: Optional[IDLToken] = None):
        """记录语法错误"""
        if token:
            self.errors.append(SyntaxError(message, token.line, token.column))
        else:
            current = self.current()
            self.errors.append(SyntaxError(message, current.line, current.column))
    
    def current(self) -> IDLToken:
        """获取当前标记"""
        if self.pos < len(self.tokens):
            return self.tokens[self.pos]
        return self.tokens[-1]  # EOF
    
    def peek(self, offset: int = 0) -> IDLToken:
        """查看标记但不移动位置"""
        pos = self.pos + offset
        if pos < len(self.tokens):
            return self.tokens[pos]
        return self.tokens[-1]  # EOF
    
    def advance(self) -> IDLToken:
        """移动到下一个标记"""
        token = self.current()
        if self.pos < len(self.tokens) - 1:
            self.pos += 1
        return token
    
    def expect(self, token_type: IDLTokenType) -> Optional[IDLToken]:
        """期望特定类型的标记"""
        token = self.current()
        if token.type == token_type:
            return self.advance()
        self.error(f"期望 {token_type.value}，但得到 {token.value}", token)
        return None
    
    def parse(self) -> List[IDLInterface]:
        """解析IDL文件，支持 OMG IDL module"""
        while self.current().type != IDLTokenType.EOF:
            if self.current().type == IDLTokenType.MODULE:
                # 解析 OMG IDL module
                module = self.parse_module()
                if module:
                    self.modules.append(module)
                    # 将 module 中的 interface 添加到 interfaces 列表
                    self.interfaces.extend(module.interfaces)
            elif self.current().type == IDLTokenType.INTERFACE:
                # 直接定义的 interface（兼容旧格式）
                interface = self.parse_interface()
                if interface:
                    self.interfaces.append(interface)
            else:
                self.error(f"期望 'module' 或 'interface' 关键字，但得到 '{self.current().value}'")
                self.advance()
        
        return self.interfaces
    
    def parse_interface(self) -> Optional[IDLInterface]:
        """解析接口定义"""
        interface_token = self.expect(IDLTokenType.INTERFACE)
        if not interface_token:
            return None
        
        name_token = self.current()
        if name_token.type != IDLTokenType.IDENTIFIER:
            self.error(f"期望接口名称，但得到 '{name_token.value}'", name_token)
            return None
        self.advance()
        
        if not self.expect(IDLTokenType.LBRACE):
            return None
        
        interface = IDLInterface(
            name=name_token.value,
            methods=[],
            line=interface_token.line
        )
        
        # 解析接口内容
        while self.current().type != IDLTokenType.RBRACE and self.current().type != IDLTokenType.EOF:
            if self.current().type == IDLTokenType.STRUCT:
                struct = self.parse_struct()
                if struct:
                    interface.structs.append(struct)
            elif self.current().type == IDLTokenType.ENUM:
                enum = self.parse_enum()
                if enum:
                    interface.enums.append(enum)
            else:
                # 解析方法
                method = self.parse_method()
                if method:
                    interface.methods.append(method)
        
        if not self.expect(IDLTokenType.RBRACE):
            self.error("接口定义未正确闭合")
        
        # 可选的分号
        if self.current().type == IDLTokenType.SEMICOLON:
            self.advance()
        
        return interface
    
    def parse_struct(self) -> Optional[IDLStruct]:
        """解析结构体定义"""
        struct_token = self.expect(IDLTokenType.STRUCT)
        if not struct_token:
            return None
        
        name_token = self.current()
        if name_token.type != IDLTokenType.IDENTIFIER:
            self.error(f"期望结构体名称", name_token)
            return None
        self.advance()
        
        if not self.expect(IDLTokenType.LBRACE):
            return None
        
        fields = []
        while self.current().type != IDLTokenType.RBRACE and self.current().type != IDLTokenType.EOF:
            # 解析字段: type name;
            type_token = self.current()
            if type_token.type != IDLTokenType.IDENTIFIER:
                self.error("期望字段类型")
                break
            self.advance()
            
            field_name_token = self.current()
            if field_name_token.type != IDLTokenType.IDENTIFIER:
                self.error("期望字段名称")
                break
            self.advance()
            
            fields.append((type_token.value, field_name_token.value))
            
            if not self.expect(IDLTokenType.SEMICOLON):
                break
        
        if not self.expect(IDLTokenType.RBRACE):
            return None
        
        if self.current().type == IDLTokenType.SEMICOLON:
            self.advance()
        
        return IDLStruct(name=name_token.value, fields=fields, line=struct_token.line)
    
    def parse_enum(self) -> Optional[IDLEnum]:
        """解析枚举定义"""
        enum_token = self.expect(IDLTokenType.ENUM)
        if not enum_token:
            return None
        
        name_token = self.current()
        if name_token.type != IDLTokenType.IDENTIFIER:
            self.error("期望枚举名称", name_token)
            return None
        self.advance()
        
        if not self.expect(IDLTokenType.LBRACE):
            return None
        
        values = []
        while self.current().type != IDLTokenType.RBRACE and self.current().type != IDLTokenType.EOF:
            value_token = self.current()
            if value_token.type != IDLTokenType.IDENTIFIER:
                self.error("期望枚举值")
                break
            values.append(value_token.value)
            self.advance()
            
            if self.current().type == IDLTokenType.COMMA:
                self.advance()
        
        if not self.expect(IDLTokenType.RBRACE):
            return None
        
        if self.current().type == IDLTokenType.SEMICOLON:
            self.advance()
        
        return IDLEnum(name=name_token.value, values=values, line=enum_token.line)
    
    def parse_method(self) -> Optional[IDLMethod]:
        """解析方法定义"""
        line = self.current().line
        
        # 检查是否有 callback 关键字
        is_callback = False
        if self.current().type == IDLTokenType.CALLBACK:
            is_callback = True
            self.advance()
        
        # 解析返回类型（使用 parse_type_spec 支持复杂类型）
        return_type = self.parse_type_spec()
        if not return_type:
            return None
        
        method_name_token = self.current()
        if method_name_token.type != IDLTokenType.IDENTIFIER:
            self.error(f"期望方法名称，但得到 '{method_name_token.value}'", method_name_token)
            return None
        self.advance()
        
        if not self.expect(IDLTokenType.LPAREN):
            return None
        
        parameters = []
        while self.current().type != IDLTokenType.RPAREN and self.current().type != IDLTokenType.EOF:
            param = self.parse_parameter()
            if param:
                parameters.append(param)
            
            if self.current().type == IDLTokenType.COMMA:
                self.advance()
            elif self.current().type != IDLTokenType.RPAREN:
                self.error("期望 ',' 或 ')'")
                break
        
        if not self.expect(IDLTokenType.RPAREN):
            return None
        
        if not self.expect(IDLTokenType.SEMICOLON):
            return None
        
        return IDLMethod(
            name=method_name_token.value,
            return_type=return_type,
            parameters=parameters,
            is_callback=is_callback,
            line=line
        )
    
    def parse_parameter(self) -> Optional[IDLParameter]:
        """解析方法参数"""
        line = self.current().line
        direction = "in"  # 默认方向
        
        # 检查方向修饰符
        if self.current().type in [IDLTokenType.IN, IDLTokenType.OUT, IDLTokenType.INOUT]:
            direction = self.current().value
            self.advance()
        
        # 参数类型（可能是 sequence<type> 或普通类型）
        type_name = self.parse_type_spec()
        if not type_name:
            return None
        
        # 参数名称
        param_name_token = self.current()
        if param_name_token.type != IDLTokenType.IDENTIFIER:
            self.error("期望参数名称", param_name_token)
            return None
        self.advance()
        
        # 检查数组
        is_array = False
        array_size = None
        if self.current().type == IDLTokenType.LBRACKET:
            is_array = True
            self.advance()
            if self.current().type == IDLTokenType.NUMBER:
                array_size = int(self.current().value)
                self.advance()
            if not self.expect(IDLTokenType.RBRACKET):
                return None
        
        return IDLParameter(
            name=param_name_token.value,
            type_name=type_name,
            direction=direction,
            is_array=is_array,
            array_size=array_size,
            line=line
        )
    
    def parse_type_spec(self) -> Optional[str]:
        """解析类型规范（支持 sequence<type>）"""
        if self.current().type == IDLTokenType.SEQUENCE:
            # sequence<type>
            self.advance()
            if not self.expect(IDLTokenType.LESS):
                return None
            
            inner_type = self.current()
            if inner_type.type not in [IDLTokenType.IDENTIFIER, IDLTokenType.BOOLEAN]:
                self.error("期望类型名称")
                return None
            self.advance()
            
            if not self.expect(IDLTokenType.GREATER):
                return None
            
            return f"sequence<{inner_type.value}>"
        else:
            # 普通类型
            type_token = self.current()
            if type_token.type not in [IDLTokenType.IDENTIFIER, IDLTokenType.VOID, IDLTokenType.BOOLEAN]:
                self.error("期望类型名称", type_token)
                return None
            self.advance()
            return type_token.value
    
    def parse_module(self) -> Optional[IDLModule]:
        """解析模块定义"""
        module_token = self.expect(IDLTokenType.MODULE)
        if not module_token:
            return None
        
        name_token = self.current()
        if name_token.type != IDLTokenType.IDENTIFIER:
            self.error("期望模块名称", name_token)
            return None
        self.advance()
        
        if not self.expect(IDLTokenType.LBRACE):
            return None
        
        module = IDLModule(name=name_token.value, line=module_token.line)
        
        # 解析模块内容
        while self.current().type != IDLTokenType.RBRACE and self.current().type != IDLTokenType.EOF:
            if self.current().type == IDLTokenType.INTERFACE:
                interface = self.parse_interface()
                if interface:
                    module.interfaces.append(interface)
            elif self.current().type == IDLTokenType.STRUCT:
                struct = self.parse_struct()
                if struct:
                    module.structs.append(struct)
            elif self.current().type == IDLTokenType.ENUM:
                enum = self.parse_enum()
                if enum:
                    module.enums.append(enum)
            elif self.current().type == IDLTokenType.TYPEDEF:
                typedef = self.parse_typedef()
                if typedef:
                    module.typedefs.append(typedef)
            else:
                self.error(f"模块中遇到未知元素: {self.current().value}")
                self.advance()
        
        if not self.expect(IDLTokenType.RBRACE):
            return None
        
        # 可选的分号
        if self.current().type == IDLTokenType.SEMICOLON:
            self.advance()
        
        return module
    
    def parse_typedef(self) -> Optional[IDLTypedef]:
        """解析类型定义"""
        typedef_token = self.expect(IDLTokenType.TYPEDEF)
        if not typedef_token:
            return None
        
        # 解析基础类型（可能是 sequence<type>）
        base_type = self.parse_type_spec()
        if not base_type:
            return None
        
        # 类型别名
        name_token = self.current()
        if name_token.type != IDLTokenType.IDENTIFIER:
            self.error("期望类型别名", name_token)
            return None
        self.advance()
        
        if not self.expect(IDLTokenType.SEMICOLON):
            return None
        
        return IDLTypedef(name=name_token.value, base_type=base_type, line=typedef_token.line)
    
    def parse_type_spec(self) -> Optional[str]:
        """解析类型规范（支持 sequence<type>）"""
        if self.current().type == IDLTokenType.SEQUENCE:
            # sequence<type>
            self.advance()
            if not self.expect(IDLTokenType.LESS):
                return None
            
            inner_type = self.current()
            if inner_type.type not in [IDLTokenType.IDENTIFIER, IDLTokenType.BOOLEAN]:
                self.error("期望类型名称")
                return None
            self.advance()
            
            if not self.expect(IDLTokenType.GREATER):
                return None
            
            return f"sequence<{inner_type.value}>"
        else:
            # 普通类型
            type_token = self.current()
            if type_token.type not in [IDLTokenType.IDENTIFIER, IDLTokenType.VOID, IDLTokenType.BOOLEAN]:
                self.error("期望类型名称", type_token)
                return None
            self.advance()
            return type_token.value
    


class CppSocketCodeGenerator:
    """C++ Socket通信代码生成器"""
    
    CPP_TYPE_MAPPING = {
        'void': 'void',
        'bool': 'bool',
        'boolean': 'bool',  # OMG IDL
        'char': 'char',
        'byte': 'uint8_t',
        'octet': 'uint8_t',  # OMG IDL
        'short': 'int16_t',
        'unsigned short': 'uint16_t',
        'int': 'int32_t',
        'unsigned int': 'uint32_t',
        'long': 'int64_t',
        'unsigned long': 'uint64_t',
        'long long': 'int64_t',
        'unsigned long long': 'uint64_t',
        'float': 'float',
        'double': 'double',
        'long double': 'long double',
        'string': 'std::string',
        # C++原生类型
        'int8_t': 'int8_t',
        'uint8_t': 'uint8_t',
        'int16_t': 'int16_t',
        'uint16_t': 'uint16_t',
        'int32_t': 'int32_t',
        'uint32_t': 'uint32_t',
        'int64_t': 'int64_t',
        'uint64_t': 'uint64_t'
    }
    
    def __init__(self, interface: IDLInterface, module: Optional[IDLModule] = None, namespace: str = "ipc", 
                 all_interfaces: Optional[list] = None, observer_interfaces: Optional[list] = None):
        self.interface = interface
        self.module = module  # OMG IDL module
        self.namespace = namespace
        self.message_id = 1000
        # 所有接口名称列表（用于检测接口类型参数）
        self.all_interfaces = set(all_interfaces) if all_interfaces else set()
        # 关联的观察者接口列表（用于在客户端中添加回调方法）
        self.observer_interfaces = observer_interfaces or []
        # 构建类型别名字典
        self.typedefs = {}
        if module and module.typedefs:
            for typedef in module.typedefs:
                self.typedefs[typedef.name] = typedef.base_type
        # 检测是否是观察者接口（所有方法都是 void 返回类型，且只有 in 参数）
        self.is_observer_interface = self._is_observer_interface()
    
    def _is_observer_interface(self) -> bool:
        """判断接口是否是观察者接口（所有方法返回void且只有in参数）"""
        if not self.interface.methods:
            return False
        for method in self.interface.methods:
            # 如果有非void返回值，不是观察者
            if method.return_type != 'void':
                return False
            # 如果有out或inout参数，不是观察者
            for param in method.parameters:
                if param.direction in ['out', 'inout']:
                    return False
        return True
    
    def map_type(self, idl_type: str) -> str:
        """映射IDL类型到C++类型，支持 OMG IDL sequence<> 和 typedef"""
        # 处理 sequence<type>
        if idl_type.startswith('sequence<') and idl_type.endswith('>'):
            elem_type = idl_type[9:-1]  # 提取 sequence<type> 中的 type
            cpp_elem_type = self.map_type(elem_type)  # 递归映射元素类型
            return f"std::vector<{cpp_elem_type}>"
        
        # 检查是否是 typedef 别名
        if idl_type in self.typedefs:
            actual_type = self.typedefs[idl_type]
            # 递归映射实际类型
            return self.map_type(actual_type)
        
        # 检查是否是接口类型（即 self.all_interfaces 中的接口名）
        # 将接口引用映射为 int（作为接口句柄/ID）
        if idl_type in self.all_interfaces:
            return "int"
        
        # 标准类型映射
        return self.CPP_TYPE_MAPPING.get(idl_type, idl_type)
    
    def generate_header(self) -> str:
        """生成头文件"""
        interface_name = self.interface.name
        guard_name = f"{interface_name.upper()}_SOCKET_HPP"
        
        code = []
        code.append(f"#ifndef {guard_name}")
        code.append(f"#define {guard_name}")
        code.append("")
        code.append("#include <string>")
        code.append("#include <vector>")
        code.append("#include <cstdint>")
        code.append("#include <cstring>")
        code.append("#include <memory>")
        code.append("#include <functional>")
        code.append("#include <thread>")
        code.append("#include <mutex>")
        code.append("#include <chrono>")
        code.append("#include <condition_variable>")
        code.append("#include <queue>")
        code.append("#include <algorithm>")
        code.append("#include <iostream>")
        code.append("#include <sys/socket.h>")
        code.append("#include <netinet/in.h>")
        code.append("#include <arpa/inet.h>")
        code.append("#include <unistd.h>")
        code.append("#include <errno.h>")
        code.append("")
        code.append(f"namespace {self.namespace} {{")
        code.append("")
        
        # 添加序列化辅助类（使用条件编译避免重复定义）
        code.append("#ifndef IPC_BYTE_BUFFER_DEFINED")
        code.append("#define IPC_BYTE_BUFFER_DEFINED")
        code.append(self._generate_serialization_helpers())
        code.append("#endif // IPC_BYTE_BUFFER_DEFINED")
        code.append("")
        
        # 消息ID定义
        code.append("// Message IDs")
        for method in self.interface.methods:
            if method.is_callback:
                # Callback 方法只需要 REQ (服务器推送给客户端)
                code.append(f"const uint32_t MSG_{method.name.upper()}_REQ = {self.message_id};")
                self.message_id += 1
            else:
                # 普通 RPC 方法需要 REQ
                code.append(f"const uint32_t MSG_{method.name.upper()}_REQ = {self.message_id};")
                self.message_id += 1
                # 如果有返回值或输出参数，也需要 RESP
                if method.return_type != 'void' or any(p.direction in ['out', 'inout'] for p in method.parameters):
                    code.append(f"const uint32_t MSG_{method.name.upper()}_RESP = {self.message_id};")
                    self.message_id += 1
        
        # 为关联的观察者接口生成消息ID
        if self.observer_interfaces:
            code.append("// Observer Message IDs (from associated observer interfaces)")
            for observer_iface in self.observer_interfaces:
                for method in observer_iface.methods:
                    code.append(f"const uint32_t MSG_OBSERVER_{method.name.upper()}_REQ = {self.message_id};")
                    self.message_id += 1
        
        code.append("")
        
        # 生成模块级别的枚举和结构体定义（如果有 module）
        # 先生成枚举，因为结构体可能使用枚举类型
        # 使用条件编译避免重复定义（module 级别的类型可能被多个接口引用）
        if self.module:
            module_guard = f"IPC_{self.module.name.upper()}_TYPES_DEFINED"
            code.append(f"#ifndef {module_guard}")
            code.append(f"#define {module_guard}")
            for enum in self.module.enums:
                code.append(self._generate_enum(enum))
                code.append("")
            for struct in self.module.structs:
                code.append(self._generate_struct(struct))
                code.append("")
            code.append(f"#endif // {module_guard}")
            code.append("")
        
        # 生成接口内部的枚举和结构体定义
        for enum in self.interface.enums:
            code.append(self._generate_enum(enum))
            code.append("")
        
        for struct in self.interface.structs:
            code.append(self._generate_struct(struct))
            code.append("")
        
        # 生成消息结构
        code.append("// Message Structures")
        for method in self.interface.methods:
            code.extend(self._generate_message_structs(method))
            code.append("")
        
        # 为关联的观察者接口生成消息结构
        if self.observer_interfaces:
            code.append("// Observer Message Structures (from associated observer interfaces)")
            for observer_iface in self.observer_interfaces:
                for method in observer_iface.methods:
                    # 为观察者方法生成请求结构（服务端推送给客户端）
                    code.extend(self._generate_observer_message_struct(method))
                    code.append("")
        
        # 生成基类
        code.append(self._generate_base_classes())
        code.append("")
        
        # 生成客户端接口
        code.append(self._generate_client_interface())
        code.append("")
        
        # 观察者接口只生成客户端部分（回调处理器），不生成服务端
        # 非观察者接口才生成服务端（处理RPC请求）
        if not self.is_observer_interface:
            code.append(self._generate_server_interface())
            code.append("")
        
        code.append(f"}} // namespace {self.namespace}")
        code.append("")
        code.append(f"#endif // {guard_name}")
        
        return "\n".join(code)
    
    def _generate_struct(self, struct: IDLStruct) -> str:
        """生成C++结构体（带序列化方法）"""
        lines = [f"struct {struct.name} {{"]
        
        # 生成字段
        for field_type, field_name in struct.fields:
            cpp_type = self.map_type(field_type)
            lines.append(f"    {cpp_type} {field_name};")
        
        # 生成序列化方法
        lines.append("")
        lines.append("    void serialize(ByteBuffer& buffer) const {")
        for field_type, field_name in struct.fields:
            cpp_type = self.map_type(field_type)
            if cpp_type == 'std::string':
                lines.append(f"        buffer.writeString({field_name});")
            elif cpp_type == 'int32_t':
                lines.append(f"        buffer.writeInt32({field_name});")
            elif cpp_type == 'uint32_t':
                lines.append(f"        buffer.writeUint32({field_name});")
            elif cpp_type == 'int64_t':
                lines.append(f"        buffer.writeInt64({field_name});")
            elif cpp_type == 'uint64_t':
                lines.append(f"        buffer.writeUint64({field_name});")
            elif cpp_type == 'int16_t':
                lines.append(f"        buffer.writeInt16({field_name});")
            elif cpp_type == 'uint16_t':
                lines.append(f"        buffer.writeUint16({field_name});")
            elif cpp_type == 'int8_t':
                lines.append(f"        buffer.writeInt8({field_name});")
            elif cpp_type == 'uint8_t':
                lines.append(f"        buffer.writeUint8({field_name});")
            elif cpp_type == 'char':
                lines.append(f"        buffer.writeChar({field_name});")
            elif cpp_type == 'bool':
                lines.append(f"        buffer.writeBool({field_name});")
            elif cpp_type == 'double':
                lines.append(f"        buffer.writeDouble({field_name});")
            elif cpp_type == 'float':
                lines.append(f"        buffer.writeFloat({field_name});")
            elif cpp_type.startswith('std::vector<'):
                # 处理 vector 类型 - 需要提取元素类型并正确序列化
                elem_type = cpp_type[12:-1]  # 提取 std::vector<Type> 中的 Type
                lines.append(f"        buffer.writeUint32({field_name}.size());")
                lines.append(f"        for (const auto& item : {field_name}) {{")
                # 根据元素类型选择序列化方法
                if elem_type == 'std::string':
                    lines.append(f"            buffer.writeString(item);")
                elif elem_type in ['int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'int16_t', 'uint16_t', 
                                  'int8_t', 'uint8_t', 'char', 'bool', 'float', 'double']:
                    write_method = {
                        'int32_t': 'writeInt32', 'uint32_t': 'writeUint32',
                        'int64_t': 'writeInt64', 'uint64_t': 'writeUint64',
                        'int16_t': 'writeInt16', 'uint16_t': 'writeUint16',
                        'int8_t': 'writeInt8', 'uint8_t': 'writeUint8',
                        'char': 'writeChar', 'bool': 'writeBool',
                        'float': 'writeFloat', 'double': 'writeDouble'
                    }[elem_type]
                    lines.append(f"            buffer.{write_method}(item);")
                else:
                    # 自定义类型 - 检查是否是enum
                    is_enum = any(e.name == elem_type for e in (self.module.enums if self.module else []))
                    if is_enum:
                        lines.append(f"            buffer.writeInt32(static_cast<int32_t>(item));")
                    else:
                        # struct类型
                        lines.append(f"            item.serialize(buffer);")
                lines.append(f"        }}")
            else:
                # 假设是自定义类型（enum 或 struct）
                # 对于 enum，当作 int32_t 处理
                if field_type in [e.name for e in (self.module.enums if self.module else [])]:
                    lines.append(f"        buffer.writeInt32(static_cast<int32_t>({field_name}));")
                else:
                    # 假设是 struct，调用其 serialize 方法
                    lines.append(f"        {field_name}.serialize(buffer);")
        lines.append("    }")
        
        # 生成反序列化方法
        lines.append("")
        lines.append("    void deserialize(ByteReader& reader) {")
        for field_type, field_name in struct.fields:
            cpp_type = self.map_type(field_type)
            if cpp_type == 'std::string':
                lines.append(f"        {field_name} = reader.readString();")
            elif cpp_type == 'int32_t':
                lines.append(f"        {field_name} = reader.readInt32();")
            elif cpp_type == 'uint32_t':
                lines.append(f"        {field_name} = reader.readUint32();")
            elif cpp_type == 'int64_t':
                lines.append(f"        {field_name} = reader.readInt64();")
            elif cpp_type == 'uint64_t':
                lines.append(f"        {field_name} = reader.readUint64();")
            elif cpp_type == 'int16_t':
                lines.append(f"        {field_name} = reader.readInt16();")
            elif cpp_type == 'uint16_t':
                lines.append(f"        {field_name} = reader.readUint16();")
            elif cpp_type == 'int8_t':
                lines.append(f"        {field_name} = reader.readInt8();")
            elif cpp_type == 'uint8_t':
                lines.append(f"        {field_name} = reader.readUint8();")
            elif cpp_type == 'char':
                lines.append(f"        {field_name} = reader.readChar();")
            elif cpp_type == 'bool':
                lines.append(f"        {field_name} = reader.readBool();")
            elif cpp_type == 'double':
                lines.append(f"        {field_name} = reader.readDouble();")
            elif cpp_type == 'float':
                lines.append(f"        {field_name} = reader.readFloat();")
            elif cpp_type.startswith('std::vector<'):
                # 处理 vector 类型 - 需要提取元素类型并正确反序列化
                elem_type = cpp_type[12:-1]  # 提取 std::vector<Type> 中的 Type
                lines.append(f"        {{")
                lines.append(f"            uint32_t count = reader.readUint32();")
                lines.append(f"            {field_name}.resize(count);")
                lines.append(f"            for (uint32_t i = 0; i < count; i++) {{")
                # 根据元素类型选择反序列化方法
                if elem_type == 'std::string':
                    lines.append(f"                {field_name}[i] = reader.readString();")
                elif elem_type in ['int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'int16_t', 'uint16_t',
                                  'int8_t', 'uint8_t', 'char', 'bool', 'float', 'double']:
                    read_method = {
                        'int32_t': 'readInt32', 'uint32_t': 'readUint32',
                        'int64_t': 'readInt64', 'uint64_t': 'readUint64',
                        'int16_t': 'readInt16', 'uint16_t': 'readUint16',
                        'int8_t': 'readInt8', 'uint8_t': 'readUint8',
                        'char': 'readChar', 'bool': 'readBool',
                        'float': 'readFloat', 'double': 'readDouble'
                    }[elem_type]
                    lines.append(f"                {field_name}[i] = reader.{read_method}();")
                else:
                    # 自定义类型 - 检查是否是enum
                    is_enum = any(e.name == elem_type for e in (self.module.enums if self.module else []))
                    if is_enum:
                        lines.append(f"                {field_name}[i] = static_cast<{elem_type}>(reader.readInt32());")
                    else:
                        # struct类型
                        lines.append(f"                {field_name}[i].deserialize(reader);")
                lines.append(f"            }}")
                lines.append(f"        }}")
            else:
                # 假设是自定义类型（enum 或 struct）
                if field_type in [e.name for e in (self.module.enums if self.module else [])]:
                    lines.append(f"        {field_name} = static_cast<{cpp_type}>(reader.readInt32());")
                else:
                    lines.append(f"        {field_name}.deserialize(reader);")
        lines.append("    }")
        
        lines.append("};")
        return "\n".join(lines)
    
    def _generate_serialization_helpers(self) -> str:
        """生成序列化辅助类"""
        return """// Serialization helpers
class ByteBuffer {
private:
    std::vector<uint8_t> data_;

public:
    void writeUint32(uint32_t value) {
        data_.push_back((value >> 24) & 0xFF);
        data_.push_back((value >> 16) & 0xFF);
        data_.push_back((value >> 8) & 0xFF);
        data_.push_back(value & 0xFF);
    }

    void writeInt32(int32_t value) {
        writeUint32(static_cast<uint32_t>(value));
    }
    
    void writeUint64(uint64_t value) {
        writeUint32(static_cast<uint32_t>(value >> 32));
        writeUint32(static_cast<uint32_t>(value & 0xFFFFFFFF));
    }
    
    void writeInt64(int64_t value) {
        writeUint64(static_cast<uint64_t>(value));
    }
    
    void writeUint16(uint16_t value) {
        data_.push_back((value >> 8) & 0xFF);
        data_.push_back(value & 0xFF);
    }
    
    void writeInt16(int16_t value) {
        writeUint16(static_cast<uint16_t>(value));
    }
    
    void writeUint8(uint8_t value) {
        data_.push_back(value);
    }
    
    void writeInt8(int8_t value) {
        data_.push_back(static_cast<uint8_t>(value));
    }
    
    void writeChar(char value) {
        data_.push_back(static_cast<uint8_t>(value));
    }

    void writeBool(bool value) {
        data_.push_back(value ? 1 : 0);
    }
    
    void writeDouble(double value) {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(double));
        writeUint32(static_cast<uint32_t>(bits >> 32));
        writeUint32(static_cast<uint32_t>(bits & 0xFFFFFFFF));
    }
    
    void writeFloat(float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(float));
        writeUint32(bits);
    }

    void writeString(const std::string& str) {
        writeUint32(str.size());
        data_.insert(data_.end(), str.begin(), str.end());
    }

    void writeStringVector(const std::vector<std::string>& vec) {
        writeUint32(vec.size());
        for (const auto& item : vec) {
            writeString(item);
        }
    }

    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }
    void clear() { data_.clear(); }
};

class ByteReader {
private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_;

public:
    ByteReader(const uint8_t* data, size_t size) : data_(data), size_(size), pos_(0) {}

    bool canRead(size_t bytes) const {
        return pos_ + bytes <= size_;
    }

    uint32_t readUint32() {
        if (!canRead(4)) throw std::runtime_error("Buffer underflow");
        uint32_t value = (static_cast<uint32_t>(data_[pos_]) << 24) | 
                         (static_cast<uint32_t>(data_[pos_+1]) << 16) |
                         (static_cast<uint32_t>(data_[pos_+2]) << 8) | 
                         static_cast<uint32_t>(data_[pos_+3]);
        pos_ += 4;
        return value;
    }

    int32_t readInt32() {
        return static_cast<int32_t>(readUint32());
    }
    
    uint64_t readUint64() {
        uint32_t high = readUint32();
        uint32_t low = readUint32();
        return (static_cast<uint64_t>(high) << 32) | low;
    }
    
    int64_t readInt64() {
        return static_cast<int64_t>(readUint64());
    }
    
    uint16_t readUint16() {
        if (!canRead(2)) throw std::runtime_error("Buffer underflow");
        uint16_t value = (static_cast<uint16_t>(data_[pos_]) << 8) | 
                         static_cast<uint16_t>(data_[pos_+1]);
        pos_ += 2;
        return value;
    }
    
    int16_t readInt16() {
        return static_cast<int16_t>(readUint16());
    }
    
    uint8_t readUint8() {
        if (!canRead(1)) throw std::runtime_error("Buffer underflow");
        return data_[pos_++];
    }
    
    int8_t readInt8() {
        return static_cast<int8_t>(readUint8());
    }
    
    char readChar() {
        if (!canRead(1)) throw std::runtime_error("Buffer underflow");
        return static_cast<char>(data_[pos_++]);
    }

    bool readBool() {
        if (!canRead(1)) throw std::runtime_error("Buffer underflow");
        return data_[pos_++] != 0;
    }
    
    double readDouble() {
        uint64_t bits = (static_cast<uint64_t>(readUint32()) << 32) | readUint32();
        double value;
        std::memcpy(&value, &bits, sizeof(double));
        return value;
    }
    
    float readFloat() {
        uint32_t bits = readUint32();
        float value;
        std::memcpy(&value, &bits, sizeof(float));
        return value;
    }

    std::string readString() {
        uint32_t len = readUint32();
        if (!canRead(len)) throw std::runtime_error("Buffer underflow");
        std::string str(reinterpret_cast<const char*>(data_ + pos_), len);
        pos_ += len;
        return str;
    }

    std::vector<std::string> readStringVector() {
        uint32_t count = readUint32();
        std::vector<std::string> vec;
        vec.reserve(count);
        for (uint32_t i = 0; i < count; i++) {
            vec.push_back(readString());
        }
        return vec;
    }

    size_t position() const { return pos_; }
};"""
    
    def _generate_enum(self, enum: IDLEnum) -> str:
        """生成C++枚举"""
        lines = [f"enum class {enum.name} {{"]
        for i, value in enumerate(enum.values):
            comma = "," if i < len(enum.values) - 1 else ""
            lines.append(f"    {value}{comma}")
        lines.append("};")
        return "\n".join(lines)
    
    def _generate_message_structs(self, method: IDLMethod) -> List[str]:
        """生成消息结构体（带序列化）"""
        lines = []
        
        # 请求消息
        lines.append(f"struct {method.name}Request {{")
        lines.append(f"    uint32_t msg_id = MSG_{method.name.upper()}_REQ;")
        
        # 收集字段信息
        req_fields = []
        for param in method.parameters:
            if param.direction in ['in', 'inout']:
                cpp_type = self.map_type(param.type_name)
                if param.is_array and not param.array_size:
                    # 动态数组：使用vector
                    if cpp_type == 'std::string':
                        lines.append(f"    std::vector<std::string> {param.name};")
                        req_fields.append(('vector<string>', param.name))
                    else:
                        lines.append(f"    std::vector<{cpp_type}> {param.name};")
                        req_fields.append(('vector', param.name, cpp_type))
                elif param.is_array and param.array_size:
                    # 固定数组暂不改变
                    lines.append(f"    {cpp_type} {param.name}[{param.array_size}];")
                    req_fields.append(('array', param.name, cpp_type, param.array_size))
                elif cpp_type.startswith('std::vector<'):
                    # 已经是vector类型（通过typedef或sequence映射）
                    lines.append(f"    {cpp_type} {param.name};")
                    # 提取vector中的元素类型
                    elem_type = cpp_type[12:-1]  # 去掉 "std::vector<" 和 ">"
                    if elem_type == 'std::string':
                        req_fields.append(('vector<string>', param.name))
                    else:
                        req_fields.append(('vector', param.name, elem_type))
                else:
                    lines.append(f"    {cpp_type} {param.name};")
                    req_fields.append((cpp_type, param.name))
        
        # 生成serialize方法
        lines.append("")
        lines.append("    void serialize(ByteBuffer& buffer) const {")
        lines.append("        buffer.writeUint32(msg_id);")
        for field_info in req_fields:
            if field_info[0] == 'vector<string>':
                lines.append(f"        buffer.writeStringVector({field_info[1]});")
            elif field_info[0] == 'vector':
                lines.append(f"        buffer.writeUint32({field_info[1]}.size());")
                lines.append(f"        for (const auto& item : {field_info[1]}) {{")
                if field_info[2] in ['int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'int16_t', 'uint16_t',
                                     'int8_t', 'uint8_t', 'char', 'bool', 'float', 'double']:
                    write_method = {
                        'int32_t': 'writeInt32', 'uint32_t': 'writeUint32',
                        'int64_t': 'writeInt64', 'uint64_t': 'writeUint64',
                        'int16_t': 'writeInt16', 'uint16_t': 'writeUint16',
                        'int8_t': 'writeInt8', 'uint8_t': 'writeUint8',
                        'char': 'writeChar', 'bool': 'writeBool',
                        'float': 'writeFloat', 'double': 'writeDouble'
                    }[field_info[2]]
                    lines.append(f"            buffer.{write_method}(item);")
                elif field_info[2] == 'std::string':
                    lines.append(f"            buffer.writeString(item);")
                else:
                    # 自定义类型 - 检查是否是enum或struct
                    is_enum = any(e.name == field_info[2] for e in (self.module.enums if self.module else []))
                    is_struct = any(s.name == field_info[2] for s in (self.module.structs if self.module else []))
                    if is_enum:
                        lines.append(f"            buffer.writeInt32(static_cast<int32_t>(item));")
                    elif is_struct:
                        lines.append(f"            item.serialize(buffer);")
                    else:
                        # 未知类型，假设是struct
                        lines.append(f"            item.serialize(buffer);")
                lines.append(f"        }}")
            elif field_info[0] == 'std::string':
                lines.append(f"        buffer.writeString({field_info[1]});")
            elif field_info[0] in ['int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'int16_t', 'uint16_t',
                                    'int8_t', 'uint8_t', 'char', 'bool', 'float', 'double']:
                write_method = {
                    'int32_t': 'writeInt32', 'uint32_t': 'writeUint32',
                    'int64_t': 'writeInt64', 'uint64_t': 'writeUint64',
                    'int16_t': 'writeInt16', 'uint16_t': 'writeUint16',
                    'int8_t': 'writeInt8', 'uint8_t': 'writeUint8',
                    'char': 'writeChar', 'bool': 'writeBool',
                    'float': 'writeFloat', 'double': 'writeDouble'
                }[field_info[0]]
                lines.append(f"        buffer.{write_method}({field_info[1]});")
            # 固定数组需要特殊处理
            elif field_info[0] == 'array':
                lines.append(f"        // TODO: serialize fixed array {field_info[1]}")
            else:
                # 其他类型 - 检查是否为struct或enum
                # 对于struct，调用serialize；对于enum，转换为int32_t
                is_struct = any(s.name == field_info[0] for s in self.module.structs)
                if is_struct:
                    lines.append(f"        {field_info[1]}.serialize(buffer);")
                else:
                    # Enum类型 - 转换为int32_t
                    lines.append(f"        buffer.writeInt32(static_cast<int32_t>({field_info[1]}));")
        lines.append("    }")
        
        # 生成deserialize方法
        lines.append("")
        lines.append("    void deserialize(ByteReader& reader) {")
        lines.append("        msg_id = reader.readUint32();")
        for field_info in req_fields:
            if field_info[0] == 'vector<string>':
                lines.append(f"        {field_info[1]} = reader.readStringVector();")
            elif field_info[0] == 'vector':
                lines.append(f"        {{")
                lines.append(f"            uint32_t count = reader.readUint32();")
                lines.append(f"            {field_info[1]}.resize(count);")
                lines.append(f"            for (uint32_t i = 0; i < count; i++) {{")
                if field_info[2] in ['int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'int16_t', 'uint16_t',
                                     'int8_t', 'uint8_t', 'char', 'bool', 'float', 'double']:
                    read_method = {
                        'int32_t': 'readInt32', 'uint32_t': 'readUint32',
                        'int64_t': 'readInt64', 'uint64_t': 'readUint64',
                        'int16_t': 'readInt16', 'uint16_t': 'readUint16',
                        'int8_t': 'readInt8', 'uint8_t': 'readUint8',
                        'char': 'readChar', 'bool': 'readBool',
                        'float': 'readFloat', 'double': 'readDouble'
                    }[field_info[2]]
                    lines.append(f"                {field_info[1]}[i] = reader.{read_method}();")
                elif field_info[2] == 'std::string':
                    lines.append(f"                {field_info[1]}[i] = reader.readString();")
                else:
                    # 自定义类型 - 检查是否是enum或struct
                    is_enum = any(e.name == field_info[2] for e in (self.module.enums if self.module else []))
                    is_struct = any(s.name == field_info[2] for s in (self.module.structs if self.module else []))
                    if is_enum:
                        lines.append(f"                {field_info[1]}[i] = static_cast<{field_info[2]}>(reader.readInt32());")
                    elif is_struct:
                        lines.append(f"                {field_info[1]}[i].deserialize(reader);")
                    else:
                        # 未知类型，假设是struct
                        lines.append(f"                {field_info[1]}[i].deserialize(reader);")
                lines.append(f"            }}")
                lines.append(f"        }}")
            elif field_info[0] == 'std::string':
                lines.append(f"        {field_info[1]} = reader.readString();")
            elif field_info[0] in ['int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'int16_t', 'uint16_t',
                                    'int8_t', 'uint8_t', 'char', 'bool', 'float', 'double']:
                read_method = {
                    'int32_t': 'readInt32', 'uint32_t': 'readUint32',
                    'int64_t': 'readInt64', 'uint64_t': 'readUint64',
                    'int16_t': 'readInt16', 'uint16_t': 'readUint16',
                    'int8_t': 'readInt8', 'uint8_t': 'readUint8',
                    'char': 'readChar', 'bool': 'readBool',
                    'float': 'readFloat', 'double': 'readDouble'
                }[field_info[0]]
                lines.append(f"        {field_info[1]} = reader.{read_method}();")
            elif field_info[0] == 'array':
                lines.append(f"        // TODO: deserialize fixed array {field_info[1]}")
            else:
                # 其他类型 - 检查是否为struct或enum
                # 对于struct，调用deserialize；对于enum，从int32_t转换
                is_struct = any(s.name == field_info[0] for s in self.module.structs)
                if is_struct:
                    lines.append(f"        {field_info[1]}.deserialize(reader);")
                else:
                    # Enum类型 - 从int32_t读取并转换
                    lines.append(f"        {field_info[1]} = static_cast<{field_info[0]}>(reader.readInt32());")
        lines.append("    }")
        
        lines.append("};")
        lines.append("")
        
        # 响应消息
        has_response = method.return_type != 'void' or any(p.direction in ['out', 'inout'] for p in method.parameters)
        if has_response:
            lines.append(f"struct {method.name}Response {{")
            lines.append(f"    uint32_t msg_id = MSG_{method.name.upper()}_RESP;")
            
            # 检查是否有名为'status'的输出参数
            has_status_param = any(p.name == 'status' and p.direction in ['out', 'inout'] for p in method.parameters)
            
            resp_fields = []
            
            # 如果没有名为'status'的输出参数，才添加标准的status字段
            if not has_status_param:
                lines.append(f"    int32_t status = 0;")
                resp_fields.append(('int32_t', 'status'))
            
            if method.return_type != 'void':
                cpp_type = self.map_type(method.return_type)
                lines.append(f"    {cpp_type} return_value;")
                # Check if return type is a vector
                if cpp_type.startswith('std::vector<'):
                    # Extract element type from std::vector<Type>
                    elem_type = cpp_type[12:-1]  # Remove "std::vector<" and ">"
                    if elem_type == 'std::string':
                        resp_fields.append(('vector<string>', 'return_value'))
                    else:
                        resp_fields.append(('vector', 'return_value', elem_type))
                else:
                    resp_fields.append((cpp_type, 'return_value'))
            
            for param in method.parameters:
                if param.direction in ['out', 'inout']:
                    cpp_type = self.map_type(param.type_name)
                    if param.is_array and not param.array_size:
                        # 动态数组：使用vector
                        if cpp_type == 'std::string':
                            lines.append(f"    std::vector<std::string> {param.name};")
                            resp_fields.append(('vector<string>', param.name))
                        else:
                            lines.append(f"    std::vector<{cpp_type}> {param.name};")
                            resp_fields.append(('vector', param.name, cpp_type))
                    elif param.is_array and param.array_size:
                        # 固定数组
                        lines.append(f"    {cpp_type} {param.name}[{param.array_size}];")
                        resp_fields.append(('array', param.name, cpp_type, param.array_size))
                    elif cpp_type.startswith('std::vector<'):
                        # Already a vector type (from typedef or sequence)
                        lines.append(f"    {cpp_type} {param.name};")
                        # Extract element type from std::vector<Type>
                        elem_type = cpp_type[12:-1]  # Remove "std::vector<" and ">"
                        if elem_type == 'std::string':
                            resp_fields.append(('vector<string>', param.name))
                        else:
                            resp_fields.append(('vector', param.name, elem_type))
                    else:
                        lines.append(f"    {cpp_type} {param.name};")
                        resp_fields.append((cpp_type, param.name))
            
            # 如果有名为'status'的输出参数，在最后添加响应状态字段
            if has_status_param:
                lines.append(f"    int32_t response_status = 0;")
                resp_fields.append(('int32_t', 'response_status'))
            
            # 生成serialize方法
            lines.append("")
            lines.append("    void serialize(ByteBuffer& buffer) const {")
            lines.append("        buffer.writeUint32(msg_id);")
            for field_info in resp_fields:
                if field_info[0] == 'vector<string>':
                    lines.append(f"        buffer.writeStringVector({field_info[1]});")
                elif field_info[0] == 'vector':
                    lines.append(f"        buffer.writeUint32({field_info[1]}.size());")
                    lines.append(f"        for (const auto& item : {field_info[1]}) {{")
                    if field_info[2] in ['int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'int16_t', 'uint16_t',
                                         'int8_t', 'uint8_t', 'char', 'bool', 'float', 'double']:
                        write_method = {
                            'int32_t': 'writeInt32', 'uint32_t': 'writeUint32',
                            'int64_t': 'writeInt64', 'uint64_t': 'writeUint64',
                            'int16_t': 'writeInt16', 'uint16_t': 'writeUint16',
                            'int8_t': 'writeInt8', 'uint8_t': 'writeUint8',
                            'char': 'writeChar', 'bool': 'writeBool',
                            'float': 'writeFloat', 'double': 'writeDouble'
                        }[field_info[2]]
                        lines.append(f"            buffer.{write_method}(item);")
                    elif field_info[2] == 'std::string':
                        lines.append(f"            buffer.writeString(item);")
                    else:
                        # Custom types - check if it's a struct or enum
                        is_enum = any(e.name == field_info[2] for e in (self.module.enums if self.module else []))
                        is_struct = any(s.name == field_info[2] for s in (self.module.structs if self.module else []))
                        if is_enum:
                            lines.append(f"            buffer.writeInt32(static_cast<int32_t>(item));")
                        elif is_struct:
                            lines.append(f"            item.serialize(buffer);")
                        else:
                            # 未知类型，假设是struct
                            lines.append(f"            item.serialize(buffer);")
                    lines.append(f"        }}")
                elif field_info[0] == 'std::string':
                    lines.append(f"        buffer.writeString({field_info[1]});")
                elif field_info[0] in ['int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'int16_t', 'uint16_t',
                                        'int8_t', 'uint8_t', 'char', 'bool', 'float', 'double']:
                    write_method = {
                        'int32_t': 'writeInt32', 'uint32_t': 'writeUint32',
                        'int64_t': 'writeInt64', 'uint64_t': 'writeUint64',
                        'int16_t': 'writeInt16', 'uint16_t': 'writeUint16',
                        'int8_t': 'writeInt8', 'uint8_t': 'writeUint8',
                        'char': 'writeChar', 'bool': 'writeBool',
                        'float': 'writeFloat', 'double': 'writeDouble'
                    }[field_info[0]]
                    lines.append(f"        buffer.{write_method}({field_info[1]});")
                elif field_info[0] == 'array':
                    lines.append(f"        // TODO: serialize fixed array {field_info[1]}")
                else:
                    # 其他类型 - 检查是否为struct或enum
                    # 对于struct，调用serialize；对于enum，转换为int32_t
                    is_struct = any(s.name == field_info[0] for s in self.module.structs)
                    if is_struct:
                        lines.append(f"        {field_info[1]}.serialize(buffer);")
                    else:
                        # Enum类型 - 转换为int32_t
                        lines.append(f"        buffer.writeInt32(static_cast<int32_t>({field_info[1]}));")
            lines.append("    }")
            
            # 生成deserialize方法
            lines.append("")
            lines.append("    void deserialize(ByteReader& reader) {")
            lines.append("        msg_id = reader.readUint32();")
            for field_info in resp_fields:
                if field_info[0] == 'vector<string>':
                    lines.append(f"        {field_info[1]} = reader.readStringVector();")
                elif field_info[0] == 'vector':
                    lines.append(f"        {{")
                    lines.append(f"            uint32_t count = reader.readUint32();")
                    lines.append(f"            {field_info[1]}.resize(count);")
                    lines.append(f"            for (uint32_t i = 0; i < count; i++) {{")
                    if field_info[2] in ['int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'int16_t', 'uint16_t',
                                         'int8_t', 'uint8_t', 'char', 'bool', 'float', 'double']:
                        read_method = {
                            'int32_t': 'readInt32', 'uint32_t': 'readUint32',
                            'int64_t': 'readInt64', 'uint64_t': 'readUint64',
                            'int16_t': 'readInt16', 'uint16_t': 'readUint16',
                            'int8_t': 'readInt8', 'uint8_t': 'readUint8',
                            'char': 'readChar', 'bool': 'readBool',
                            'float': 'readFloat', 'double': 'readDouble'
                        }[field_info[2]]
                        lines.append(f"                {field_info[1]}[i] = reader.{read_method}();")
                    elif field_info[2] == 'std::string':
                        lines.append(f"                {field_info[1]}[i] = reader.readString();")
                    else:
                        # Custom types - check if it's a struct or enum
                        is_enum = any(e.name == field_info[2] for e in (self.module.enums if self.module else []))
                        is_struct = any(s.name == field_info[2] for s in (self.module.structs if self.module else []))
                        if is_enum:
                            lines.append(f"                {field_info[1]}[i] = static_cast<{field_info[2]}>(reader.readInt32());")
                        elif is_struct:
                            lines.append(f"                {field_info[1]}[i].deserialize(reader);")
                        else:
                            # 未知类型，假设是struct
                            lines.append(f"                {field_info[1]}[i].deserialize(reader);")
                    lines.append(f"            }}")
                    lines.append(f"        }}")
                elif field_info[0] == 'std::string':
                    lines.append(f"        {field_info[1]} = reader.readString();")
                elif field_info[0] in ['int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'int16_t', 'uint16_t',
                                        'int8_t', 'uint8_t', 'char', 'bool', 'float', 'double']:
                    read_method = {
                        'int32_t': 'readInt32', 'uint32_t': 'readUint32',
                        'int64_t': 'readInt64', 'uint64_t': 'readUint64',
                        'int16_t': 'readInt16', 'uint16_t': 'readUint16',
                        'int8_t': 'readInt8', 'uint8_t': 'readUint8',
                        'char': 'readChar', 'bool': 'readBool',
                        'float': 'readFloat', 'double': 'readDouble'
                    }[field_info[0]]
                    lines.append(f"        {field_info[1]} = reader.{read_method}();")
                elif field_info[0] == 'array':
                    lines.append(f"        // TODO: deserialize fixed array {field_info[1]}")
                else:
                    # 其他类型 - 检查是否为struct或enum
                    # 对于struct，调用deserialize；对于enum，从int32_t转换
                    is_struct = any(s.name == field_info[0] for s in self.module.structs)
                    if is_struct:
                        lines.append(f"        {field_info[1]}.deserialize(reader);")
                    else:
                        # Enum类型 - 从int32_t读取并转换
                        lines.append(f"        {field_info[1]} = static_cast<{field_info[0]}>(reader.readInt32());")
            lines.append("    }")
            
            lines.append("};")
        
        return lines
    
    def _generate_observer_message_struct(self, method: IDLMethod) -> List[str]:
        """为观察者方法生成请求消息结构（服务端推送给客户端）"""
        lines = []
        
        lines.append(f"struct observer_{method.name}Request {{")
        lines.append(f"    uint32_t msg_id = MSG_OBSERVER_{method.name.upper()}_REQ;")
        
        # 收集字段信息
        req_fields = []
        for param in method.parameters:
            cpp_type = self.map_type(param.type_name)
            lines.append(f"    {cpp_type} {param.name};")
            req_fields.append((cpp_type, param.name, param.type_name))
        
        # 生成serialize方法
        lines.append("")
        lines.append("    void serialize(ByteBuffer& buffer) const {")
        lines.append("        buffer.writeUint32(msg_id);")
        for cpp_type, field_name, idl_type in req_fields:
            if cpp_type == 'std::string':
                lines.append(f"        buffer.writeString({field_name});")
            elif cpp_type in ['int32_t', 'int64_t']:
                lines.append(f"        buffer.writeInt32({field_name});")
            elif cpp_type == 'uint32_t':
                lines.append(f"        buffer.writeUint32({field_name});")
            elif cpp_type == 'bool':
                lines.append(f"        buffer.writeBool({field_name});")
            elif cpp_type == 'std::vector<std::string>':
                lines.append(f"        buffer.writeStringVector({field_name});")
            else:
                # 其他复杂类型暂时忽略序列化（需要自定义）
                lines.append(f"        // TODO: serialize {cpp_type} {field_name}")
        lines.append("    }")
        
        # 生成deserialize方法
        lines.append("")
        lines.append("    void deserialize(ByteReader& reader) {")
        lines.append("        msg_id = reader.readUint32();")
        for cpp_type, field_name, idl_type in req_fields:
            if cpp_type == 'std::string':
                lines.append(f"        {field_name} = reader.readString();")
            elif cpp_type in ['int32_t', 'int64_t']:
                lines.append(f"        {field_name} = reader.readInt32();")
            elif cpp_type == 'uint32_t':
                lines.append(f"        {field_name} = reader.readUint32();")
            elif cpp_type == 'bool':
                lines.append(f"        {field_name} = reader.readBool();")
            elif cpp_type == 'std::vector<std::string>':
                lines.append(f"        {field_name} = reader.readStringVector();")
            else:
                # 其他复杂类型暂时忽略反序列化
                lines.append(f"        // TODO: deserialize {cpp_type} {field_name}")
        lines.append("    }")
        
        lines.append("};")
        
        return lines
    
    def _generate_base_classes(self) -> str:
        """生成基础类（使用条件编译避免重复定义）"""
        return """#ifndef IPC_SOCKET_BASE_DEFINED
#define IPC_SOCKET_BASE_DEFINED
// Socket Base Class
class SocketBase {
protected:
    int sockfd_;
    struct sockaddr_in addr_;
    bool connected_;
    
public:
    SocketBase() : sockfd_(-1), connected_(false) {}
    
    virtual ~SocketBase() {
        if (sockfd_ >= 0) {
            close(sockfd_);
        }
    }
    
    bool isConnected() const { return connected_; }
    
    ssize_t sendData(const void* data, size_t size) {
        return send(sockfd_, data, size, 0);
    }
    
    ssize_t receiveData(void* buffer, size_t size) {
        return recv(sockfd_, buffer, size, 0);
    }
    
    static ssize_t sendDataToSocket(int fd, const void* data, size_t size) {
        return send(fd, data, size, 0);
    }
    
    static ssize_t receiveDataFromSocket(int fd, void* buffer, size_t size) {
        return recv(fd, buffer, size, 0);
    }
};
#endif // IPC_SOCKET_BASE_DEFINED"""
    
    def _generate_client_interface(self) -> str:
        """生成客户端接口类"""
        # 如果是观察者接口，生成观察者客户端（接收服务器推送）
        if self.is_observer_interface:
            return self._generate_observer_client_interface()
        # 否则生成正常的RPC客户端
        return self._generate_rpc_client_interface()
    
    def _generate_observer_client_interface(self) -> str:
        """生成观察者客户端接口（用于接收服务器推送的通知）"""
        interface_name = self.interface.name
        lines = []
        
        lines.append(f"// Observer Client Interface for {interface_name}")
        lines.append(f"// This client receives notifications from server")
        lines.append(f"class {interface_name}Client : public SocketBase {{")
        lines.append("private:")
        lines.append("    std::thread listener_thread_;")
        lines.append("    bool listening_;")
        lines.append("")
        lines.append("public:")
        lines.append(f"    {interface_name}Client() : listening_(false) {{}}")
        lines.append("")
        lines.append(f"    ~{interface_name}Client() {{")
        lines.append("        stopListening();")
        lines.append("    }")
        lines.append("")
        lines.append("    // Connect to server")
        lines.append("    bool connect(const std::string& host, uint16_t port) {")
        lines.append("        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);")
        lines.append("        if (sockfd_ < 0) return false;")
        lines.append("")
        lines.append("        addr_.sin_family = AF_INET;")
        lines.append("        addr_.sin_port = htons(port);")
        lines.append("        inet_pton(AF_INET, host.c_str(), &addr_.sin_addr);")
        lines.append("")
        lines.append("        if (::connect(sockfd_, (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {")
        lines.append("            close(sockfd_);")
        lines.append("            sockfd_ = -1;")
        lines.append("            return false;")
        lines.append("        }")
        lines.append("        connected_ = true;")
        lines.append("        return true;")
        lines.append("    }")
        lines.append("")
        lines.append("    // Start listening for server notifications")
        lines.append("    void startListening() {")
        lines.append("        if (listening_ || !connected_) return;")
        lines.append("        listening_ = true;")
        lines.append("        listener_thread_ = std::thread([this]() { listenLoop(); });")
        lines.append("    }")
        lines.append("")
        lines.append("    void stopListening() {")
        lines.append("        listening_ = false;")
        lines.append("        if (listener_thread_.joinable()) listener_thread_.join();")
        lines.append("    }")
        lines.append("")
        lines.append("private:")
        lines.append("    void listenLoop() {")
        lines.append("        while (listening_ && connected_) {")
        lines.append("            struct timeval tv;")
        lines.append("            tv.tv_sec = 1;")
        lines.append("            tv.tv_usec = 0;")
        lines.append("            setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));")
        lines.append("")
        lines.append("            uint32_t msg_size;")
        lines.append("            ssize_t received = recv(sockfd_, &msg_size, sizeof(msg_size), 0);")
        lines.append("            if (received <= 0) {")
        lines.append("                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;")
        lines.append("                break;")
        lines.append("            }")
        lines.append("")
        lines.append("            uint8_t recv_buffer[65536];")
        lines.append("            ssize_t recv_size = recv(sockfd_, recv_buffer, msg_size, 0);")
        lines.append("            if (recv_size < 0 || recv_size != msg_size) break;")
        lines.append("")
        lines.append("            if (msg_size < 4) continue;")
        lines.append("            uint32_t msg_id = (static_cast<uint32_t>(recv_buffer[0]) << 24) |")
        lines.append("                              (static_cast<uint32_t>(recv_buffer[1]) << 16) |")
        lines.append("                              (static_cast<uint32_t>(recv_buffer[2]) << 8) |")
        lines.append("                              static_cast<uint32_t>(recv_buffer[3]);")
        lines.append("")
        lines.append("            handleNotification(msg_id, recv_buffer, recv_size);")
        lines.append("        }")
        lines.append("    }")
        lines.append("")
        lines.append("    void handleNotification(uint32_t msg_id, const uint8_t* data, size_t size) {")
        lines.append("        ByteReader reader(data, size);")
        lines.append("        switch (msg_id) {")
        
        # 为每个观察者方法生成通知处理
        for method in self.interface.methods:
            lines.append(f"            case MSG_{method.name.upper()}_REQ: {{")
            lines.append(f"                {method.name}Request request;")
            lines.append(f"                request.deserialize(reader);")
            
            # 调用虚函数
            call_params = []
            for param in method.parameters:
                if param.direction == 'in':
                    call_params.append(f"request.{param.name}")
            
            lines.append(f"                {method.name}({', '.join(call_params)});")
            lines.append(f"                break;")
            lines.append(f"            }}")
        
        lines.append("            default:")
        lines.append("                std::cout << \"[Observer] Unknown notification: \" << msg_id << std::endl;")
        lines.append("                break;")
        lines.append("        }")
        lines.append("    }")
        lines.append("")
        lines.append("protected:")
        lines.append("    // Override these methods to handle notifications")
        
        for method in self.interface.methods:
            params = []
            for param in method.parameters:
                if param.direction == 'in':
                    cpp_type = self.map_type(param.type_name)
                    if param.is_array and not param.array_size:
                        params.append(f"const std::vector<{cpp_type}>& {param.name}")
                    elif param.is_array and param.array_size:
                        params.append(f"const {cpp_type} {param.name}[{param.array_size}]")
                    else:
                        if cpp_type == 'std::string':
                            params.append(f"const {cpp_type}& {param.name}")
                        else:
                            params.append(f"{cpp_type} {param.name}")
            
            lines.append(f"    virtual void {method.name}({', '.join(params)}) {{")
            lines.append(f"        // Override to handle {method.name} notification")
            lines.append(f"        std::cout << \"[Observer] Received {method.name} notification\" << std::endl;")
            lines.append("    }")
            lines.append("")
        
        lines.append("};")
        
        return "\n".join(lines)
    
    def _generate_rpc_client_interface(self) -> str:
        """生成正常的RPC客户端接口"""
        interface_name = self.interface.name
        lines = []
        
        lines.append(f"// Client Interface for {interface_name}")
        lines.append(f"class {interface_name}Client : public SocketBase {{")
        lines.append("private:")
        lines.append("    std::thread listener_thread_;")
        lines.append("    bool listening_;")
        lines.append("    std::mutex send_mutex_;")
        lines.append("")
        lines.append("    // Message queue for RPC responses")
        lines.append("    struct QueuedMessage {")
        lines.append("        uint32_t msg_id;")
        lines.append("        std::vector<uint8_t> data;")
        lines.append("    };")
        lines.append("    std::queue<QueuedMessage> rpc_response_queue_;")
        lines.append("    std::mutex queue_mutex_;")
        lines.append("    std::condition_variable queue_cv_;")
        lines.append("")
        lines.append("public:")
        lines.append(f"    {interface_name}Client() : listening_(false) {{}}")
        lines.append("")
        lines.append(f"    ~{interface_name}Client() {{")
        lines.append("        stopListening();")
        lines.append("    }")
        lines.append("")
        lines.append("    // Connect to server")
        lines.append("    bool connect(const std::string& host, uint16_t port) {")
        lines.append("        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);")
        lines.append("        if (sockfd_ < 0) {")
        lines.append("            return false;")
        lines.append("        }")
        lines.append("")
        lines.append("        addr_.sin_family = AF_INET;")
        lines.append("        addr_.sin_port = htons(port);")
        lines.append("        inet_pton(AF_INET, host.c_str(), &addr_.sin_addr);")
        lines.append("")
        lines.append("        if (::connect(sockfd_, (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {")
        lines.append("            close(sockfd_);")
        lines.append("            sockfd_ = -1;")
        lines.append("            return false;")
        lines.append("        }")
        lines.append("")
        lines.append("        connected_ = true;")
        lines.append("        ")
        lines.append("        // Auto-start listener thread for message reception")
        lines.append("        startListening();")
        lines.append("        ")
        lines.append("        return true;")
        lines.append("    }")
        lines.append("")
        lines.append("    // Start async listening for broadcast messages")
        lines.append("    void startListening() {")
        lines.append("        if (listening_ || !connected_) return;")
        lines.append("        listening_ = true;")
        lines.append("        listener_thread_ = std::thread([this]() {")
        lines.append("            listenLoop();")
        lines.append("        });")
        lines.append("    }")
        lines.append("")
        lines.append("    // Stop async listening")
        lines.append("    void stopListening() {")
        lines.append("        listening_ = false;")
        lines.append("        if (listener_thread_.joinable()) {")
        lines.append("            listener_thread_.join();")
        lines.append("        }")
        lines.append("    }")
        lines.append("")
        lines.append("private:")
        lines.append("    void listenLoop() {")
        lines.append("        while (listening_ && connected_) {")
        lines.append("            // Set recv timeout to allow periodic checking of listening_ flag")
        lines.append("            struct timeval tv;")
        lines.append("            tv.tv_sec = 1;")
        lines.append("            tv.tv_usec = 0;")
        lines.append("            setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));")
        lines.append("")
        lines.append("            // Receive message size")
        lines.append("            uint32_t msg_size;")
        lines.append("            ssize_t received = recv(sockfd_, &msg_size, sizeof(msg_size), 0);")
        lines.append("")
        lines.append("            if (received <= 0) {")
        lines.append("                if (errno == EAGAIN || errno == EWOULDBLOCK) {")
        lines.append("                    continue; // Timeout, continue listening")
        lines.append("                }")
        lines.append("                break; // Connection closed or error")
        lines.append("            }")
        lines.append("")
        lines.append("            // Receive message data")
        lines.append("            uint8_t recv_buffer[65536];")
        lines.append("            ssize_t recv_size = recv(sockfd_, recv_buffer, msg_size, 0);")
        lines.append("            if (recv_size < 0 || recv_size != msg_size) {")
        lines.append("                break;")
        lines.append("            }")
        lines.append("")
        lines.append("            // Parse message ID")
        lines.append("            if (msg_size < 4) continue;")
        lines.append("            uint32_t msg_id = (static_cast<uint32_t>(recv_buffer[0]) << 24) |")
        lines.append("                              (static_cast<uint32_t>(recv_buffer[1]) << 16) |")
        lines.append("                              (static_cast<uint32_t>(recv_buffer[2]) << 8) |")
        lines.append("                              static_cast<uint32_t>(recv_buffer[3]);")
        lines.append("")
        lines.append("            // Check if this is a callback message (REQ) or RPC response (RESP)")
        lines.append("            bool is_callback = isCallbackMessage(msg_id);")
        lines.append("")
        lines.append("            if (is_callback) {")
        lines.append("                // Handle callback directly")
        lines.append("                handleBroadcastMessage(msg_id, recv_buffer, recv_size);")
        lines.append("            } else {")
        lines.append("                // Queue RPC response for RPC method to retrieve")
        lines.append("                QueuedMessage msg;")
        lines.append("                msg.msg_id = msg_id;")
        lines.append("                msg.data.assign(recv_buffer, recv_buffer + recv_size);")
        lines.append("                {")
        lines.append("                    std::lock_guard<std::mutex> lock(queue_mutex_);")
        lines.append("                    rpc_response_queue_.push(msg);")
        lines.append("                }")
        lines.append("                queue_cv_.notify_one();")
        lines.append("            }")
        lines.append("        }")
        lines.append("    }")
        lines.append("")
        lines.append("    bool isCallbackMessage(uint32_t msg_id) {")
        lines.append("        // Check if message ID corresponds to a callback (REQ message)")
        lines.append("        switch (msg_id) {")
        
        # Add observer callback message IDs
        if self.observer_interfaces:
            for observer_iface in self.observer_interfaces:
                for method in observer_iface.methods:
                    msg_const = f"MSG_OBSERVER_{method.name.upper()}_REQ"
                    lines.append(f"            case {msg_const}:")
        
        # Add callback method message IDs
        for method in self.interface.methods:
            if method.is_callback:
                msg_const = f"MSG_{method.name.upper()}_REQ"
                lines.append(f"            case {msg_const}:")
        
        lines.append("                return true;")
        lines.append("            default:")
        lines.append("                return false;")
        lines.append("        }")
        lines.append("    }")
        lines.append("")
        lines.append("    void handleBroadcastMessage(uint32_t msg_id, const uint8_t* data, size_t size) {")
        lines.append("        ByteReader reader(data, size);")
        lines.append("        ")
        lines.append("        switch (msg_id) {")
        
        # 为关联的观察者接口添加消息处理
        if self.observer_interfaces:
            for observer_iface in self.observer_interfaces:
                for method in observer_iface.methods:
                    msg_const = f"MSG_OBSERVER_{method.name.upper()}_REQ"
                    req_struct = f"observer_{method.name}Request"
                    lines.append(f"            case {msg_const}: {{")
                    lines.append(f"                {req_struct} request;")
                    lines.append(f"                request.deserialize(reader);")
                    # 调用观察者回调方法
                    params = [f"request.{param.name}" for param in method.parameters]
                    params_str = ", ".join(params)
                    lines.append(f"                {method.name}({params_str});")
                    lines.append(f"                break;")
                    lines.append(f"            }}")
        
        # 为 callback 方法添加消息处理
        for method in self.interface.methods:
            if method.is_callback:
                msg_const = f"MSG_{method.name.upper()}_REQ"
                req_struct = f"{method.name}Request"
                lines.append(f"            case {msg_const}: {{")
                lines.append(f"                {req_struct} request;")
                lines.append(f"                request.deserialize(reader);")
                # 调用回调方法
                params = [f"request.{param.name}" for param in method.parameters]
                params_str = ", ".join(params)
                lines.append(f"                {method.name}({params_str});")
                lines.append(f"                break;")
                lines.append(f"            }}")
        
        lines.append("            default:")
        lines.append("                std::cout << \"[Client] Received unknown broadcast message: \" << msg_id << std::endl;")
        lines.append("                break;")
        lines.append("        }")
        lines.append("    }")
        lines.append("")
        lines.append("protected:")
        
        # 如果有关联的观察者接口，添加观察者回调方法
        if self.observer_interfaces:
            lines.append("    // Observer callbacks (from associated observer interfaces)")
            for observer_iface in self.observer_interfaces:
                for method in observer_iface.methods:
                    params = []
                    for param in method.parameters:
                        cpp_type = self.map_type(param.type_name)
                        params.append(f"{cpp_type} {param.name}")
                    
                    params_str = ", ".join(params)
                    lines.append(f"    virtual void {method.name}({params_str}) {{")
                    lines.append(f"        // Override to handle {method.name} notification from server")
                    lines.append(f"        std::cout << \"[Client] 📢 Observer: {method.name}\" << std::endl;")
                    lines.append("    }")
                    lines.append("")
        
        # 生成 callback 标记的回调方法
        callback_methods = [m for m in self.interface.methods if m.is_callback]
        if callback_methods:
            lines.append("    // Callback methods (marked with 'callback' keyword in IDL)")
            for method in callback_methods:
                params = []
                for param in method.parameters:
                    cpp_type = self.map_type(param.type_name)
                    params.append(f"{cpp_type} {param.name}")
                
                params_str = ", ".join(params)
                lines.append(f"    virtual void {method.name}({params_str}) {{")
                lines.append(f"        // Override to handle {method.name} callback from server")
                lines.append(f"        std::cout << \"[Client] 📢 Callback: {method.name}\" << std::endl;")
                lines.append("    }")
                lines.append("")
        
        lines.append("public:")
        lines.append("")
        
        # 生成客户端方法（不包含 callback 方法）
        for method in self.interface.methods:
            if not method.is_callback:  # 跳过 callback 方法
                lines.append(self._generate_client_method(method))
                lines.append("")
        
        lines.append("};")
        
        return "\n".join(lines)
    
    def _generate_client_method(self, method: IDLMethod) -> str:
        """生成客户端方法（使用序列化）"""
        lines = []
        
        # 方法签名
        cpp_return_type = self.map_type(method.return_type) if method.return_type != 'void' else 'bool'
        params = []
        
        for param in method.parameters:
            cpp_type = self.map_type(param.type_name)
            if param.direction == 'in':
                if param.is_array:
                    if param.array_size:
                        params.append(f"const {cpp_type} {param.name}[{param.array_size}]")
                    else:
                        params.append(f"const std::vector<{cpp_type}>& {param.name}")
                else:
                    if cpp_type == 'std::string':
                        params.append(f"const {cpp_type}& {param.name}")
                    else:
                        params.append(f"{cpp_type} {param.name}")
            else:  # out 或 inout
                if param.is_array:
                    if param.array_size:
                        params.append(f"{cpp_type} {param.name}[{param.array_size}]")
                    else:
                        params.append(f"std::vector<{cpp_type}>& {param.name}")
                else:
                    params.append(f"{cpp_type}& {param.name}")
        
        lines.append(f"    {cpp_return_type} {method.name}({', '.join(params)}) {{")
        lines.append("        if (!connected_) {")
        if method.return_type == 'void':
            lines.append("            return false;")
        else:
            lines.append(f"            return {cpp_return_type}();")
        lines.append("        }")
        lines.append("")
        lines.append(f"        // Prepare request")
        lines.append(f"        {method.name}Request request;")
        
        for param in method.parameters:
            if param.direction in ['in', 'inout']:
                cpp_type = self.map_type(param.type_name)
                if param.is_array and not param.array_size:
                    # 动态数组：直接赋值vector
                    lines.append(f"        request.{param.name} = {param.name};")
                elif param.is_array and param.array_size:
                    # 固定数组
                    lines.append(f"        memcpy(request.{param.name}, {param.name}, sizeof(request.{param.name}));")
                else:
                    # 标量
                    lines.append(f"        request.{param.name} = {param.name};")
        
        lines.append("")
        lines.append("        // Serialize and send request (thread-safe)")
        lines.append("        std::lock_guard<std::mutex> lock(send_mutex_);")
        lines.append("        ByteBuffer buffer;")
        lines.append("        request.serialize(buffer);")
        lines.append("        // Send message size first")
        lines.append("        uint32_t msg_size = buffer.size();")
        lines.append("        if (sendData(&msg_size, sizeof(msg_size)) < 0) {")
        if method.return_type == 'void':
            lines.append("            return false;")
        else:
            lines.append(f"            return {cpp_return_type}();")
        lines.append("        }")
        lines.append("        // Send message data")
        lines.append("        if (sendData(buffer.data(), buffer.size()) < 0) {")
        if method.return_type == 'void':
            lines.append("            return false;")
        else:
            lines.append(f"            return {cpp_return_type}();")
        lines.append("        }")
        lines.append("")
        
        has_response = method.return_type != 'void' or any(p.direction in ['out', 'inout'] for p in method.parameters)
        if has_response:
            lines.append("        // Wait for response from queue (listener thread will queue it)")
            lines.append(f"        uint32_t expected_msg_id = MSG_{method.name.upper()}_RESP;")
            lines.append("        QueuedMessage response_msg;")
            lines.append("        {")
            lines.append("            std::unique_lock<std::mutex> lock(queue_mutex_);")
            lines.append("            // Wait up to 5 seconds for response")
            lines.append("            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {")
            lines.append("                // Check if expected response is in queue")
            lines.append("                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;")
            lines.append("                while (!temp_queue.empty()) {")
            lines.append("                    if (temp_queue.front().msg_id == expected_msg_id) {")
            lines.append("                        return true;")
            lines.append("                    }")
            lines.append("                    temp_queue.pop();")
            lines.append("                }")
            lines.append("                return false;")
            lines.append("            })) {")
            if method.return_type == 'void':
                lines.append("                return false; // Timeout")
            else:
                lines.append(f"                return {cpp_return_type}(); // Timeout")
            lines.append("            }")
            lines.append("")
            lines.append("            // Find and remove the response message from queue")
            lines.append("            std::queue<QueuedMessage> temp_queue;")
            lines.append("            bool found = false;")
            lines.append("            while (!rpc_response_queue_.empty()) {")
            lines.append("                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {")
            lines.append("                    response_msg = rpc_response_queue_.front();")
            lines.append("                    found = true;")
            lines.append("                } else {")
            lines.append("                    temp_queue.push(rpc_response_queue_.front());")
            lines.append("                }")
            lines.append("                rpc_response_queue_.pop();")
            lines.append("            }")
            lines.append("            rpc_response_queue_ = temp_queue;")
            lines.append("        }")
            lines.append("")
            lines.append(f"        {method.name}Response response;")
            lines.append("        ByteReader reader(response_msg.data.data(), response_msg.data.size());")
            lines.append("        response.deserialize(reader);")
            lines.append("")
            
            # 处理输出参数
            for param in method.parameters:
                if param.direction in ['out', 'inout']:
                    cpp_type = self.map_type(param.type_name)
                    if param.is_array and not param.array_size:
                        # 动态数组：直接赋值
                        lines.append(f"        {param.name} = response.{param.name};")
                    elif param.is_array and param.array_size:
                        # 固定数组
                        lines.append(f"        memcpy({param.name}, response.{param.name}, sizeof(response.{param.name}));")
                    else:
                        # 标量
                        lines.append(f"        {param.name} = response.{param.name};")
            
            if method.return_type != 'void':
                lines.append("        return response.return_value;")
            else:
                # 检查是否有名为'status'的输出参数
                has_status_param = any(p.name == 'status' and p.direction in ['out', 'inout'] for p in method.parameters)
                status_field = "response_status" if has_status_param else "status"
                lines.append(f"        return response.{status_field} == 0;")
        else:
            lines.append("        return true;")
        
        lines.append("    }")
        
        return "\n".join(lines)
    
    def _generate_server_interface(self) -> str:
        """生成服务端接口类"""
        interface_name = self.interface.name
        lines = []
        
        lines.append(f"// Server Interface for {interface_name}")
        lines.append(f"class {interface_name}Server : public SocketBase {{")
        lines.append("private:")
        lines.append("    int listen_fd_;")
        lines.append("    bool running_;")
        lines.append("    std::vector<int> client_fds_;")
        lines.append("    mutable std::mutex clients_mutex_;")
        lines.append("    std::vector<std::thread> client_threads_;")
        lines.append("")
        lines.append("public:")
        lines.append(f"    {interface_name}Server() : listen_fd_(-1), running_(false) {{}}")
        lines.append("")
        lines.append("    ~" + interface_name + "Server() {")
        lines.append("        stop();")
        lines.append("    }")
        lines.append("")
        lines.append("    // Start server")
        lines.append("    bool start(uint16_t port) {")
        lines.append("        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);")
        lines.append("        if (listen_fd_ < 0) {")
        lines.append("            return false;")
        lines.append("        }")
        lines.append("")
        lines.append("        int opt = 1;")
        lines.append("        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));")
        lines.append("")
        lines.append("        addr_.sin_family = AF_INET;")
        lines.append("        addr_.sin_addr.s_addr = INADDR_ANY;")
        lines.append("        addr_.sin_port = htons(port);")
        lines.append("")
        lines.append("        if (bind(listen_fd_, (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {")
        lines.append("            close(listen_fd_);")
        lines.append("            listen_fd_ = -1;")
        lines.append("            return false;")
        lines.append("        }")
        lines.append("")
        lines.append("        if (listen(listen_fd_, 10) < 0) {")
        lines.append("            close(listen_fd_);")
        lines.append("            listen_fd_ = -1;")
        lines.append("            return false;")
        lines.append("        }")
        lines.append("")
        lines.append("        running_ = true;")
        lines.append("        return true;")
        lines.append("    }")
        lines.append("")
        lines.append("    void stop() {")
        lines.append("        running_ = false;")
        lines.append("        ")
        lines.append("        // Close all client connections")
        lines.append("        {")
        lines.append("            std::lock_guard<std::mutex> lock(clients_mutex_);")
        lines.append("            for (int fd : client_fds_) {")
        lines.append("                close(fd);")
        lines.append("            }")
        lines.append("            client_fds_.clear();")
        lines.append("        }")
        lines.append("        ")
        lines.append("        if (listen_fd_ >= 0) {")
        lines.append("            close(listen_fd_);")
        lines.append("            listen_fd_ = -1;")
        lines.append("        }")
        lines.append("        ")
        lines.append("        // Wait for all client threads to finish")
        lines.append("        for (auto& thread : client_threads_) {")
        lines.append("            if (thread.joinable()) {")
        lines.append("                thread.join();")
        lines.append("            }")
        lines.append("        }")
        lines.append("        client_threads_.clear();")
        lines.append("    }")
        lines.append("")
        lines.append("    // Main server loop")
        lines.append("    void run() {")
        lines.append("        while (running_) {")
        lines.append("            struct sockaddr_in client_addr;")
        lines.append("            socklen_t addr_len = sizeof(client_addr);")
        lines.append("            int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &addr_len);")
        lines.append("")
        lines.append("            if (client_fd < 0) {")
        lines.append("                if (running_) {")
        lines.append("                    continue;")
        lines.append("                } else {")
        lines.append("                    break;")
        lines.append("                }")
        lines.append("            }")
        lines.append("")
        lines.append("            // Add client to list")
        lines.append("            {")
        lines.append("                std::lock_guard<std::mutex> lock(clients_mutex_);")
        lines.append("                client_fds_.push_back(client_fd);")
        lines.append("            }")
        lines.append("")
        lines.append("            // Handle client in a new thread")
        lines.append("            client_threads_.emplace_back([this, client_fd]() {")
        lines.append("                handleClient(client_fd);")
        lines.append("                ")
        lines.append("                // Remove client from list")
        lines.append("                {")
        lines.append("                    std::lock_guard<std::mutex> lock(clients_mutex_);")
        lines.append("                    auto it = std::find(client_fds_.begin(), client_fds_.end(), client_fd);")
        lines.append("                    if (it != client_fds_.end()) {")
        lines.append("                        client_fds_.erase(it);")
        lines.append("                    }")
        lines.append("                }")
        lines.append("                ")
        lines.append("                close(client_fd);")
        lines.append("                onClientDisconnected(client_fd);")
        lines.append("            });")
        lines.append("        }")
        lines.append("    }")
        lines.append("")
        lines.append("    // Broadcast message to all connected clients (with serialization)")
        lines.append("    // exclude_fd: optionally exclude one client from broadcast (e.g., the sender)")
        lines.append("    template<typename T>")
        lines.append("    void broadcast(const T& message, int exclude_fd = -1) {")
        lines.append("        std::lock_guard<std::mutex> lock(clients_mutex_);")
        lines.append("        ")
        lines.append("        // Serialize message once")
        lines.append("        ByteBuffer buffer;")
        lines.append("        message.serialize(buffer);")
        lines.append("        uint32_t msg_size = buffer.size();")
        lines.append("        ")
        lines.append("        // Send to all clients except excluded one")
        lines.append("        for (int fd : client_fds_) {")
        lines.append("            if (fd == exclude_fd) continue; // Skip the excluded client")
        lines.append("            // Send message size")
        lines.append("            send(fd, &msg_size, sizeof(msg_size), 0);")
        lines.append("            // Send message data")
        lines.append("            send(fd, buffer.data(), buffer.size(), 0);")
        lines.append("        }")
        lines.append("    }")
        lines.append("")
        lines.append("    // Get number of connected clients")
        lines.append("    size_t getClientCount() {")
        lines.append("        std::lock_guard<std::mutex> lock(clients_mutex_);")
        lines.append("        return client_fds_.size();")
        lines.append("    }")
        lines.append("")
        lines.append("private:")
        lines.append("    void handleClient(int client_fd) {")
        lines.append("        onClientConnected(client_fd);")
        lines.append("        ")
        lines.append("        while (running_) {")
        lines.append("            // Receive message size")
        lines.append("            uint32_t msg_size;")
        lines.append("            ssize_t received = recv(client_fd, &msg_size, sizeof(msg_size), 0);")
        lines.append("            ")
        lines.append("            if (received <= 0) {")
        lines.append("                // Client disconnected or error")
        lines.append("                break;")
        lines.append("            }")
        lines.append("")
        lines.append("            // Peek message ID from the data (will be consumed by handler)")
        lines.append("            uint8_t peek_buffer[65536];")
        lines.append("            ssize_t peeked = recv(client_fd, peek_buffer, 4, MSG_PEEK);")
        lines.append("            if (peeked < 4) {")
        lines.append("                break;")
        lines.append("            }")
        lines.append("")
        lines.append("            uint32_t msg_id = (static_cast<uint32_t>(peek_buffer[0]) << 24) |")
        lines.append("                              (static_cast<uint32_t>(peek_buffer[1]) << 16) |")
        lines.append("                              (static_cast<uint32_t>(peek_buffer[2]) << 8) |")
        lines.append("                              static_cast<uint32_t>(peek_buffer[3]);")
        lines.append("")
        lines.append("            switch (msg_id) {")
        
        # 只为非 callback 方法生成服务端处理 (callback 方法是服务端推送给客户端的)
        for method in self.interface.methods:
            if not method.is_callback:
                lines.append(f"                case MSG_{method.name.upper()}_REQ:")
                lines.append(f"                    handle_{method.name}(client_fd, msg_size);")
                lines.append("                    break;")
        
        lines.append("                default:")
        lines.append("                    // Unknown message, consume the data")
        lines.append("                    {")
        lines.append("                        uint8_t discard[65536];")
        lines.append("                        recv(client_fd, discard, msg_size, 0);")
        lines.append("                    }")
        lines.append("                    break;")
        lines.append("            }")
        lines.append("        }")
        lines.append("    }")
        lines.append("")
        
        # 生成服务端处理方法 (只为非 callback 方法生成)
        for method in self.interface.methods:
            if not method.is_callback:
                lines.append(self._generate_server_handler(method))
                lines.append("")
        
        # 为 callback 方法生成推送方法
        callback_methods = [m for m in self.interface.methods if m.is_callback]
        if callback_methods:
            lines.append("public:")
            lines.append("    // Callback push methods (send callbacks to clients)")
            for method in callback_methods:
                lines.append(self._generate_callback_push_method(method))
                lines.append("")
        
        # 生成虚函数声明（用户需要实现）
        lines.append("protected:")
        lines.append("    // Virtual functions to be implemented by user")
        for method in self.interface.methods:
            if not method.is_callback:  # callback 方法不需要用户在服务端实现
                lines.append(self._generate_virtual_method(method))
        lines.append("")
        lines.append("    // Optional callbacks for client connection events")
        lines.append("    virtual void onClientConnected(int client_fd) {")
        lines.append("        // Override this to handle client connection")
        lines.append("    }")
        lines.append("")
        lines.append("    virtual void onClientDisconnected(int client_fd) {")
        lines.append("        // Override this to handle client disconnection")
        lines.append("    }")
        
        lines.append("};")
        
        return "\n".join(lines)
    
    def _generate_server_handler(self, method: IDLMethod) -> str:
        """生成服务端消息处理方法（使用序列化，msg_size已被读取）"""
        lines = []
        
        lines.append(f"    void handle_{method.name}(int client_fd, uint32_t msg_size) {{")
        lines.append("        // Receive request data (size already read by handleClient)")
        lines.append("        uint8_t recv_buffer[65536];")
        lines.append("        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);")
        lines.append("        if (recv_size < 0 || recv_size != msg_size) {")
        lines.append("            return;")
        lines.append("        }")
        lines.append("")
        lines.append(f"        {method.name}Request request;")
        lines.append("        ByteReader reader(recv_buffer, recv_size);")
        lines.append("        request.deserialize(reader);")
        lines.append("")
        
        has_response = method.return_type != 'void' or any(p.direction in ['out', 'inout'] for p in method.parameters)
        
        if has_response:
            lines.append(f"        {method.name}Response response;")
            
            # 准备调用参数
            call_params = []
            
            for param in method.parameters:
                cpp_type = self.map_type(param.type_name)
                
                if param.direction == 'in':
                    # in参数：从request读取
                    call_params.append(f"request.{param.name}")
                elif param.direction == 'out':
                    # out参数：写入response
                    call_params.append(f"response.{param.name}")
                elif param.direction == 'inout':
                    # inout参数：从request复制到response，然后传递response的引用
                    lines.append(f"        response.{param.name} = request.{param.name};")
                    call_params.append(f"response.{param.name}")
            
            # 调用用户实现的方法
            if method.return_type != 'void':
                lines.append(f"        response.return_value = on{method.name}({', '.join(call_params)});")
            else:
                lines.append(f"        on{method.name}({', '.join(call_params)});")
            
            lines.append("")
            lines.append("        // Serialize and send response")
            lines.append("        ByteBuffer buffer;")
            lines.append("        response.serialize(buffer);")
            lines.append("        // Send response size first")
            lines.append("        uint32_t resp_size = buffer.size();")
            lines.append("        send(client_fd, &resp_size, sizeof(resp_size), 0);")
            lines.append("        // Send response data")
            lines.append("        send(client_fd, buffer.data(), buffer.size(), 0);")
        else:
            # 无响应的方法
            call_params = []
            
            for param in method.parameters:
                if param.direction == 'in':
                    call_params.append(f"request.{param.name}")
                
            lines.append(f"        on{method.name}({', '.join(call_params)});")
        
        lines.append("    }")
        
        return "\n".join(lines)
    
    def _generate_callback_push_method(self, method: IDLMethod) -> str:
        """生成 callback 方法的推送函数（服务端用于向客户端推送回调）"""
        lines = []
        
        # 生成推送方法签名
        params = []
        for param in method.parameters:
            cpp_type = self.map_type(param.type_name)
            if param.is_array:
                if param.array_size:
                    params.append(f"const {cpp_type} {param.name}[{param.array_size}]")
                else:
                    params.append(f"const std::vector<{cpp_type}>& {param.name}")
            else:
                if cpp_type == 'std::string':
                    params.append(f"const {cpp_type}& {param.name}")
                else:
                    params.append(f"{cpp_type} {param.name}")
        
        # 添加可选的 exclude_fd 参数用于排除特定客户端
        params.append("int exclude_fd = -1")
        
        lines.append(f"    void push_{method.name}({', '.join(params)}) {{")
        lines.append(f"        // Prepare callback request")
        lines.append(f"        {method.name}Request request;")
        
        # 填充请求参数
        for param in method.parameters:
            cpp_type = self.map_type(param.type_name)
            if param.is_array and not param.array_size:
                lines.append(f"        request.{param.name} = {param.name};")
            elif param.is_array and param.array_size:
                lines.append(f"        memcpy(request.{param.name}, {param.name}, sizeof(request.{param.name}));")
            else:
                lines.append(f"        request.{param.name} = {param.name};")
        
        lines.append("")
        lines.append(f"        // Broadcast callback to all clients")
        lines.append(f"        broadcast(request, exclude_fd);")
        lines.append("    }")
        
        return "\n".join(lines)
    
    def _generate_virtual_method(self, method: IDLMethod) -> str:
        """生成虚函数声明"""
        cpp_return_type = self.map_type(method.return_type)
        params = []
        
        for param in method.parameters:
            cpp_type = self.map_type(param.type_name)
            if param.direction == 'in':
                if param.is_array:
                    if param.array_size:
                        params.append(f"const {cpp_type} {param.name}[{param.array_size}]")
                    else:
                        params.append(f"const std::vector<{cpp_type}>& {param.name}")
                else:
                    if cpp_type == 'std::string':
                        params.append(f"const {cpp_type}& {param.name}")
                    else:
                        params.append(f"{cpp_type} {param.name}")
            else:  # out 或 inout
                if param.is_array:
                    if param.array_size:
                        params.append(f"{cpp_type} {param.name}[{param.array_size}]")
                    else:
                        params.append(f"std::vector<{cpp_type}>& {param.name}")
                else:
                    params.append(f"{cpp_type}& {param.name}")
        
        return f"    virtual {cpp_return_type} on{method.name}({', '.join(params)}) = 0;"
    
    def generate_client_example(self) -> str:
        """生成客户端使用示例"""
        interface_name = self.interface.name
        lines = []
        
        lines.append(f"// Example client usage for {interface_name}")
        lines.append(f'#include "{interface_name.lower()}_socket.hpp"')
        lines.append("#include <iostream>")
        lines.append("")
        lines.append("int main() {")
        lines.append(f"    {self.namespace}::{interface_name}Client client;")
        lines.append("")
        lines.append('    if (!client.connect("127.0.0.1", 8888)) {')
        lines.append('        std::cerr << "Failed to connect to server" << std::endl;')
        lines.append("        return 1;")
        lines.append("    }")
        lines.append("")
        
        if self.interface.methods:
            method = self.interface.methods[0]
            lines.append(f"    // Call {method.name}")
            
            # 示例参数
            call_params = []
            for param in method.parameters:
                if param.direction == 'in':
                    if param.type_name == 'int':
                        call_params.append("42")
                    elif param.type_name == 'string':
                        call_params.append('"example"')
                    else:
                        call_params.append("/* value */")
                elif param.direction in ['out', 'inout']:
                    cpp_type = self.map_type(param.type_name)
                    lines.append(f"    {cpp_type} {param.name};")
                    call_params.append(param.name)
            
            if method.return_type != 'void':
                lines.append(f"    auto result = client.{method.name}({', '.join(call_params)});")
                lines.append('    std::cout << "Result: " << result << std::endl;')
            else:
                lines.append(f"    client.{method.name}({', '.join(call_params)});")
        
        lines.append("")
        lines.append("    return 0;")
        lines.append("}")
        
        return "\n".join(lines)
    
    def generate_server_example(self) -> str:
        """生成服务端使用示例"""
        interface_name = self.interface.name
        lines = []
        
        lines.append(f"// Example server implementation for {interface_name}")
        lines.append(f'#include "{interface_name.lower()}_socket.hpp"')
        lines.append("#include <iostream>")
        lines.append("")
        lines.append(f"using namespace {self.namespace};")
        lines.append("")
        lines.append(f"class My{interface_name}Server : public {self.namespace}::{interface_name}Server {{")
        lines.append("protected:")
        
        for method in self.interface.methods:
            # 只生成非 callback 方法的实现
            if method.is_callback:
                continue
                
            cpp_return_type = self.map_type(method.return_type)
            params = []
            
            for param in method.parameters:
                cpp_type = self.map_type(param.type_name)
                
                if param.direction == 'in':
                    if param.is_array:
                        if param.array_size:
                            params.append(f"const {cpp_type} {param.name}[{param.array_size}]")
                        else:
                            params.append(f"const {cpp_type}& {param.name}")
                    else:
                        if param.type_name == 'string':
                            params.append(f"const {cpp_type}& {param.name}")
                        else:
                            params.append(f"{cpp_type} {param.name}")
                else:
                    if param.is_array:
                        if param.array_size:
                            params.append(f"{cpp_type} {param.name}[{param.array_size}]")
                        else:
                            params.append(f"{cpp_type}& {param.name}")
                    else:
                        params.append(f"{cpp_type}& {param.name}")
            
            lines.append(f"    {cpp_return_type} on{method.name}({', '.join(params)}) override {{")
            lines.append(f"        // TODO: Implement {method.name}")
            lines.append('        std::cout << "' + method.name + ' called" << std::endl;')
            
            if method.return_type != 'void':
                if method.return_type == 'int':
                    lines.append("        return 0;")
                elif method.return_type == 'bool':
                    lines.append("        return true;")
                elif method.return_type == 'string':
                    lines.append('        return "result";')
                else:
                    lines.append(f"        return {cpp_return_type}();")
            
            lines.append("    }")
            lines.append("")
        
        lines.append("};")
        lines.append("")
        lines.append("int main() {")
        lines.append(f"    My{interface_name}Server server;")
        lines.append("")
        lines.append("    if (!server.start(8888)) {")
        lines.append('        std::cerr << "Failed to start server" << std::endl;')
        lines.append("        return 1;")
        lines.append("    }")
        lines.append("")
        lines.append('    std::cout << "Server started on port 8888" << std::endl;')
        lines.append("    server.run();")
        lines.append("")
        lines.append("    return 0;")
        lines.append("}")
        
        return "\n".join(lines)


def print_errors(errors: List[SyntaxError], filename: str):
    """打印语法错误"""
    if not errors:
        return
    
    print(f"\n在文件 '{filename}' 中发现 {len(errors)} 个错误:")
    print("=" * 70)
    
    for error in errors:
        severity_marker = "❌" if error.severity == "error" else "⚠️ "
        print(f"{severity_marker} 第 {error.line} 行, 第 {error.column} 列: {error.message}")
    
    print("=" * 70)


def main():
    parser = argparse.ArgumentParser(
        description="IDL到Socket通信代码生成器 - 支持Linux环境的C++ Socket IPC代码生成"
    )
    parser.add_argument("idl_file", help="IDL接口定义文件路径")
    parser.add_argument("-o", "--output", default="generated", help="输出目录 (默认: generated)")
    parser.add_argument("-n", "--namespace", default="ipc", help="C++命名空间 (默认: ipc)")
    parser.add_argument("--no-examples", action="store_true", help="不生成示例代码")
    parser.add_argument("--check-only", action="store_true", help="仅检查语法，不生成代码")
    
    args = parser.parse_args()
    
    # 读取IDL文件
    try:
        with open(args.idl_file, 'r', encoding='utf-8') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"❌ 错误: 找不到文件 '{args.idl_file}'")
        return 1
    except Exception as e:
        print(f"❌ 错误: 读取文件失败 - {e}")
        return 1
    
    print(f"📖 正在解析 IDL 文件: {args.idl_file}")
    
    # 词法分析
    lexer = IDLLexer(content)
    tokens = lexer.tokenize()
    
    if lexer.errors:
        print_errors(lexer.errors, args.idl_file)
        return 1
    
    print(f"✅ 词法分析完成，共 {len(tokens)} 个标记")
    
    # 语法分析
    parser_obj = IDLParser(tokens)
    interfaces = parser_obj.parse()
    
    if parser_obj.errors:
        print_errors(parser_obj.errors, args.idl_file)
        return 1
    
    if not interfaces:
        print("❌ 错误: 没有找到接口定义")
        return 1
    
    print(f"✅ 语法分析完成，找到 {len(interfaces)} 个接口")
    
    for interface in interfaces:
        print(f"   - 接口 '{interface.name}': {len(interface.methods)} 个方法")
    
    if args.check_only:
        print("\n✅ 语法检查通过！")
        return 0
    
    # 生成代码
    print(f"\n🔧 正在生成 C++ Socket 通信代码...")
    
    os.makedirs(args.output, exist_ok=True)
    
    # 找到对应的 module（如果有）
    module_map = {}
    for module in parser_obj.modules:
        for iface in module.interfaces:
            module_map[iface.name] = module
    
    # 收集所有接口名称（用于类型映射）
    all_interface_names = [iface.name for iface in interfaces]
    
    # 识别观察者接口和服务接口的关系
    observer_interfaces = {}  # 服务接口名 -> [观察者接口列表]
    interface_map = {iface.name: iface for iface in interfaces}
    
    for interface in interfaces:
        # 检查是否是观察者接口（所有方法返回void且只有in参数）
        is_observer = all(
            method.return_type == 'void' and
            all(p.direction == 'in' for p in method.parameters)
            for method in interface.methods
        )
        
        if is_observer:
            # 这是观察者接口，找到引用它的服务接口
            for other_iface in interfaces:
                if other_iface.name == interface.name:
                    continue
                # 检查other_iface的方法参数是否引用了interface
                found = False
                for method in other_iface.methods:
                    for param in method.parameters:
                        if param.type_name == interface.name:
                            # other_iface 引用了 interface（观察者）
                            if other_iface.name not in observer_interfaces:
                                observer_interfaces[other_iface.name] = []
                            # 检查是否已经添加过（避免重复）
                            if interface not in observer_interfaces[other_iface.name]:
                                observer_interfaces[other_iface.name].append(interface)
                            found = True
                            break
                    if found:
                        break
    
    for interface in interfaces:
        # 跳过纯观察者接口（它们会被合并到服务接口中）
        is_pure_observer = all(
            method.return_type == 'void' and
            all(p.direction == 'in' for p in method.parameters)
            for method in interface.methods
        )
        
        # 检查是否被其他接口引用
        is_referenced = any(interface.name in obs_list_name 
                           for obs_list in observer_interfaces.values()
                           for obs_list_name in [obs.name for obs in obs_list])
        
        if is_pure_observer and is_referenced:
            print(f"   ⏭️  跳过: {interface.name} (观察者接口，将合并到服务接口)")
            continue
        
        interface_name = interface.name.lower()
        
        # 获取对应的 module
        module = module_map.get(interface.name)
        
        # 获取关联的观察者接口
        related_observers = observer_interfaces.get(interface.name, [])
        
        # 生成代码，传递所有接口名称和关联的观察者
        generator = CppSocketCodeGenerator(interface, module, args.namespace, all_interface_names, related_observers)
        
        # 生成头文件
        header_code = generator.generate_header()
        header_file = os.path.join(args.output, f"{interface_name}_socket.hpp")
        with open(header_file, 'w', encoding='utf-8') as f:
            f.write(header_code)
        print(f"   ✅ 生成: {header_file}")
        
        # 生成示例代码
        if not args.no_examples:
            client_example = generator.generate_client_example()
            client_file = os.path.join(args.output, f"{interface_name}_client_example.cpp")
            with open(client_file, 'w', encoding='utf-8') as f:
                f.write(client_example)
            print(f"   ✅ 生成: {client_file}")
            
            server_example = generator.generate_server_example()
            server_file = os.path.join(args.output, f"{interface_name}_server_example.cpp")
            with open(server_file, 'w', encoding='utf-8') as f:
                f.write(server_example)
            print(f"   ✅ 生成: {server_file}")
    
    # 生成Makefile
    makefile_content = generate_makefile(interfaces, args.namespace)
    makefile_path = os.path.join(args.output, "Makefile")
    with open(makefile_path, 'w', encoding='utf-8') as f:
        f.write(makefile_content)
    print(f"   ✅ 生成: {makefile_path}")
    
    print("\n🎉 代码生成完成！")
    print(f"\n编译方法:")
    print(f"  cd {args.output}")
    print(f"  make")
    
    return 0


def generate_makefile(interfaces: List[IDLInterface], namespace: str) -> str:
    """生成Makefile"""
    lines = []
    lines.append("# Makefile for IDL Socket Generated Code")
    lines.append("")
    lines.append("CXX = g++")
    lines.append("CXXFLAGS = -std=c++11 -Wall -O2 -pthread")
    lines.append("LDFLAGS = -pthread")
    lines.append("")
    lines.append("all: " + " ".join([f"{iface.name.lower()}_client {iface.name.lower()}_server" for iface in interfaces]))
    lines.append("")
    
    for interface in interfaces:
        name_lower = interface.name.lower()
        lines.append(f"{name_lower}_client: {name_lower}_client_example.cpp {name_lower}_socket.hpp")
        lines.append(f"\t$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)")
        lines.append("")
        lines.append(f"{name_lower}_server: {name_lower}_server_example.cpp {name_lower}_socket.hpp")
        lines.append(f"\t$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)")
        lines.append("")
    
    lines.append("clean:")
    lines.append("\trm -f " + " ".join([f"{iface.name.lower()}_client {iface.name.lower()}_server" for iface in interfaces]))
    lines.append("")
    lines.append(".PHONY: all clean")
    
    return "\n".join(lines)


if __name__ == "__main__":
    sys.exit(main())
