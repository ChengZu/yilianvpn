# 西瓜vpn
## 最新版本说明
2026.02

1.修复内存管理bug(task多容器同步移除)

2.修复建立代理bug(Proxy端口号初始化错误)

3.修复用户验证bug

4.修复过期代理清除bug(清除时socket未关闭)

5.更新app功能


2025.08 修复tcpproxy初始化连接错误

# 如何运行服务器程序
## linux
1.上传程序源码到服务器

2.编译程序
```  
 make
```  
3.运行程序
```  
 ./vpn.out port=4430
```
## windows
Dev-C++ IDE 编译运行

# 如何安装客户端

1.将build/xiguavpn-release.apk 拷贝到android手机上安装

2.ip选项填服务器IP地址, port填服务器端口(4430), dns服务器地区dns

3.点击启动
