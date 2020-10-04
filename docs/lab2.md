## Lab2 内存管理

[toc]

### 2.1 实验内容简介

#### 2.1.1 实验目标

　　在本实验中，需要实现教学操作系统内核的内存管理系统。内存管理系统分为两部分：

- 内核的物理内存分配器，用于分配和回收物理内存
- 内核的页表，用于将虚拟地址映射到物理地址

#### 2.1.2 Git

　　在本次实验以及后续实验中，你需要根据文档指示自行实现教学操作系统的内核，我们仅会提供部分代码用于引导。每次发布新的 lab，均会开启一个新的分支，以下获取新 lab 信息的流程在后续实验中会经常出现（并非强制性的）。首先确保你当前所在分支为完成 lab1 实验的分支，假设为 `dev`。

```sh
git pull # 通过 git pull 获取 github 仓库中的更新
git checkout -b dev-lab2 # 从 dev 切换到新的分支 dev-lab2
git rebase lab2 # 将 lab2 中的更新作用于 dev-lab2
work in dev-lab2 branch...
git add files you've modified
git commit -m "balabala"
git checkout dev # 在生成新的 commit 后，切换回 dev
git rebase dev-lab2 # 将 dev-lab2 中的修改作用于 dev，此时 dev 分支为完成 lab0-2 实验的分支
```

### 2.2 物理内存管理





### 2.x 参考文献

