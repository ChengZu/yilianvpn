# 西瓜vpn
## 最新版本说明
网速没有损失
## 建议提供
TCP如何断开本人不详，欢迎大家提供建议

非阻塞Socket很多报错，欢迎大家提供建议

## 被墙建议
服务器端口找一个可以用的，默认80，不可以用就看ssh是否可以用，可以就用22端口

## vps服务商建议
https://www.vultr.com/?ref=7607586

# 如何运行服务器程序
## C++版
1.上传程序源码到服务器

2.编译程序
```  
 make
```  
3.运行程序
```  
 ./vpn.out 22
```

## java版
1.安装jdk
```  
apt install openjdk-11-jdk-headless
```  
2.运行程序
 
 将build/XiGuaVpn.jar 拷贝到服务器(用WinSCP), 执行下面命令运行
```  
 java -jar XiGuaVpn.jar 22
```  

# 如何安装客户端

1.将build/XiGuaVpn.apk 拷贝到android手机上安装

2.ip选项填服务器IP地址, port填服务器端口(22)

3.点击启动
