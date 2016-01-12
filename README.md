# 一个基于ASIO(c++11)的网络服务器

## 目录介绍
1. src：包含所以源文件及执行cmake所需要的CMakeLists.txt。
2. vs2013：windows环境vs2013工程文件。
3. bin：包含程序运行时配置文件及日志配置文件。
4. build：在linux系统编译时，建议在此目录编译程序。

## 程序介绍
1. module目录包含全部公用模块，center包含中心平台服务器模式源码，proxy包含远程服务器模式源码。
2. src/CMakeLists.txt为编译入口。

## 程序编译
1. 在项目顶级目录进入到build目录：`cd build`
2. 使用cmake生成平台相关makefile文件：`cmake ../src`
3. 编译文件：`make`,将在build/bin下生成ics-center(中心平台服务器)、ics-proxy（远程代理服务器）可执行文件
4. 安装文件: `make install`，将安装到../src/CMakeLists.txt指定的路径下

## 程序执行
1. 将ics-center拷贝到bin目录下，修改该目录下的config.xml配置文件，修改log4cplus.properties日志配置文件
2. 执行：`ics-center config.xml`

