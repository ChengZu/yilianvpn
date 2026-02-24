# 西瓜vpn
## 最新版本说明
2026.02 
1.修复内存管理BUG(task多容器同步移除)

2.Proxy端口号初始化错误

3.用户验证错误

2025.08 修复tcpproxy初始连接化错误

# 如何运行服务器程序
## linux
1.上传程序源码到服务器

2.编译程序
```  
 make
```  
3.运行程序
```  
 ./vpn.out 4430
```
## windows
Dev-C++ IDE 编译运行

# 如何安装客户端

1.将build/XiGuaVpn.apk 拷贝到android手机上安装

2.ip选项填服务器IP地址, port填服务器端口(4430), dns服务器地区dns

3.点击启动
