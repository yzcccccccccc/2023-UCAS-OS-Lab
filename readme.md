# Project6-FileSystem
笔者实现了S和A Core的任务要求。
## 支持命令
支持绝对路径和相对路径的操作。  
`mkfs [-f]`：初始化文件系统，参数`-f`指示是否强制初始化  
`statfs`：查看文件系统状态  
`mkdir [dirname]`：创建目录  
`rmdir [dirname]`：删除目录  
`touch [filename]`：创建空文件  
`cat [filename]`：在shell中打印文件内容  
`rm [filename]`：删除文件
`ln [filename] [filename]`：建立硬链接  
`ls [-l] [dirname]`：打印目录中的文件情况，默认为当前目录。参数`-l`指示是否需要打印详细信息  
`cd [dirname]`：进入一个目录  
`pwd`：查看当前路径  
## 测试文件
`rwfile.c`：测试文件的打开、读写和关闭功能。  
`lf_test.c`：大文件测试。测试文件数据块的动态分配。  
## 运行流程
**QEMU**
```bash
# 运行：
make run-net  
# 调试：
make debug-net
``` 
需要在`Makefile`中在镜像后面padding一定的空白块。  

**板卡**
```bash
make minicom
# 之后使用loadbootm启动双核
```
