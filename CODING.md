# 内核代码风格指南

此文档描述了我们的内核代码风格

## 缩进

- 使用四个空格来完成缩进，而非制表符 `\t`

## 括号和空格

- 总体风格类似 K&R 风格，只有函数定义的花括号需要换行（因为函数不能嵌套定义），如下：

  ```
  main()
  {
      struct a {
          ...
      };

      for (int i = 0; i < n; i ++) {
          ...
      }

      while (...) {
          ...
      }

      do {
          ...
      } while (...);

      if (...) {
          ...
      } else if (...) {
          ...
      } else
          ...
  }
  ```

- 将 `switch` 和它的 `case` 标签对齐，如下:

  ```
  switch (a) {
  case 1:
  case 2:
  default:
      break;
  }
  ```

- 在二元和三元运算符周围加上空格

  如 `a ? b : c` 而非 `a ? b: c`

- 一元运算符如 `& * ~ ! __attribute__` 之后不带空格

  如 `~p` 而非 `~ p`

- 前（后）自增自减运算符之后（之前）不加空格

  如 `++a` 而非 `++ a`

- 函数调用时，函数名和括号之间没有空格

  如 `printf("hello")` 而非 `printf ("hello")`

- 指针中存在空格

  如 `(uint16_t *)` 而非 `(uint16_t*)`

## 注释

- 注释为 C 风格 注释 `/* ... */`

- 多行注释的风格如下：

  ```
  /*
   * A column of asterisks on the left side,
   * with beginning and ending almost-blank lines.
   */
  ```

## 命名约定

- 宏由全大写和下划线组成，除了：

  `assert`, `panic`, `static_assert`, `offsetof`, `container_of`

- 函数和变量名均为小写，单词之间用下划线连接

- 在函数定义时，返回值和函数名各占一行

  这样就可用 `grep -n '^foo' */*.c` 找到 `foo` 的函数定义

- 无参函数的定义和声明均为 `f()` 而非 `f(void)`
