ALL : vpn.out

clean:
	rm -f vpn.out

vpn.out : main.cpp VpnServer.h Client.h \
  Proxy.h ProxyContainer.h TcpProxy.h UdpProxy.h \
  CommonMethods.h IPHeader.h TCPHeader.h \
  UDPHeader.h Task.h Loop.h Socket.h \
  ServerSocket.h RingQueueBuffer.h
	g++ -g -rdynamic main.cpp VpnServer.h Client.h \
  Proxy.h ProxyContainer.h TcpProxy.h UdpProxy.h \
  Config.h CommonMethods.h IPHeader.h TCPHeader.h \
  UDPHeader.h Task.h Loop.h Socket.h \
  ServerSocket.h RingQueueBuffer.h -o vpn.out -I./