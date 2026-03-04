ALL : vpn.out

clean:
	rm -f vpn.out

vpn.out : Main.cpp VpnServer.h Client.h \
  Proxy.h TcpProxy.h UdpProxy.h \
  Config.h CommonMethods.h IPHeader.h TCPHeader.h \
  UDPHeader.h Task.h Socket.h \
  ServerSocket.h
	g++ -g -rdynamic Main.cpp VpnServer.h Client.h \
  Proxy.h TcpProxy.h UdpProxy.h \
  Config.h CommonMethods.h IPHeader.h TCPHeader.h \
  UDPHeader.h Task.h Socket.h \
  ServerSocket.h -o vpn.out -I./