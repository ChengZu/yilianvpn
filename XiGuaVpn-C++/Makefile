ALL : vpn.out

clean:
	rm -f vpn.out

vpn.out : Main.cpp VpnServer.h Client.h \
  TcpTunnel.h UdpTunnel.h TcpProxy.h UdpProxy.h \
  Config.h CommonMethods.h IPHeader.h TCPHeader.h \
  UDPHeader.h Task.h Loop.h Socket.h \
  ServerSocket.h DatagramSocket.h
	g++ -g -rdynamic Main.cpp VpnServer.h Client.h \
  TcpTunnel.h UdpTunnel.h TcpProxy.h UdpProxy.h \
  Config.h CommonMethods.h IPHeader.h TCPHeader.h \
  UDPHeader.h Task.h Loop.h Socket.h \
  ServerSocket.h DatagramSocket.h -o vpn.out -I./