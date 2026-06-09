# cpp-containers-practice

从零实现的 C++ 标准库风格容器，用于深入理解 STL 底层机制与 C++ 现代特性。

> **声明：** 本项目为个人 C++ 练手项目，容器实现中的部分修改优化以及全部测试样例由大语言模型辅助生成。

## 容器

| 容器 | 底层结构 | 对应 STL |
|------|----------|----------|
| `MyVector<T>` | 动态数组（倍增扩容） | `std::vector` |
| `MyList<T>` | 双向循环链表（哨兵节点） | `std::list` |
| `MyStack<T>` | 适配器 → `MyVector` | `std::stack` |
| `MyQueue<T>` | 适配器 → `MyList` | `std::queue` |

## 技术要点

- **C++20** — 使用 `std::construct_at` / `std::destroy_at` 替代 C++17 中废弃的 `std::allocator` 成员函数，确保与现代标准的兼容性
- **RAII + Rule of Five** — 所有容器完整实现拷贝/移动构造、拷贝/移动赋值及析构函数，保证资源安全
- **异常安全** — 扩容路径在分配新内存 → 移动/拷贝元素失败时，正确回滚已分配内存
- **`if constexpr` 编译期分支** — 扩容时根据 `std::is_nothrow_move_constructible_v<T>` 选择移动或拷贝路径，在保证安全的前提下最大化性能
- **哨兵节点设计** — `MyList` 使用循环链表 + 哨兵头节点，消除空链表边界判断，`pop_front` / `pop_back` 均为 O(1)
- **类型擦除基类** — `my_list_node_base` 分离指针字段与数据字段，减少模板实例化导致的代码膨胀
- **适配器模式** — `MyStack` / `MyQueue` 通过组合复用已有容器，不重复实现数据结构

## 构建

```bash
# 前置: CMake >= 3.30, 支持 C++20 的编译器
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

支持 GCC 11+ / Clang 16+ / MSVC 2022+，可直接用 CLion 打开。

## 测试

```bash
./build/experiment02
```

共 39 项测试，覆盖：

- 默认 / 拷贝 / 移动构造与赋值
- 自赋值安全
- 元素增删（含 rvalue 重载）
- 扩容正确性（200 元素）
- 对象生命周期（构造/析构计数校验）
- 空容器边界行为
- `const` 正确性

## 结构

```
.
├── CMakeLists.txt
├── main.cpp          # 容器实现 + 测试
└── README.md
```
