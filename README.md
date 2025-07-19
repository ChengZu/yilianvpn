# 西瓜vpn
TCP如何断开本人不详，，欢迎大家提供建议
非阻塞Socket很多报错，欢迎大家提供建议
# 如何配置服务器（Ubuntu）
## C++版
1.上传程序源码到服务器

2.编译程序
```  
 make
```  
3.运行程序
```  
 ./vpn.out 80
```

## java版
1.安装jdk
```  
apt install openjdk-11-jdk-headless
```  
2.运行程序
 
 将build/XiGuaVpn.jar 拷贝到服务器(用WinSCP), 执行下面命令运行
```  
 java -jar XiGuaVpn.jar 80
```  

# 如何安装客户端

1.将build/XiGuaVpn.apk 拷贝到android手机上安装

2.ip选项填服务器IP地址, port填服务器端口

3.点击启动
