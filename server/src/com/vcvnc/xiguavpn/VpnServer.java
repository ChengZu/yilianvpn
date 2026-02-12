package com.vcvnc.xiguavpn;

/*
 * 服务器套接字
 * 接收新客户端，移除已关闭客户端
 */
import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class VpnServer implements Runnable {
	// 服务器套接字
	private ServerSocket server = null;
	// 客户端保存数组
	public List<Client> clients = Collections.synchronizedList(new ArrayList<Client>());
	// 套接字运行状态
	private boolean closed = false;
	// 接收新客户端线程
	private Thread thread;

	public VpnServer() {
		try {
			// 初始化服务器套接字
			server = new ServerSocket(Config.PORT);
			// 创建新线程
			thread = new Thread(this);
			// 启动新线程
			thread.start();
		} catch (IOException e) {
			e.printStackTrace();
			;
		}
	}

	/*
	 * 关闭服务器套接字和客户端
	 */
	public void close() {
		closed = true;
		if (thread != null)
			thread.interrupt();
		try {
			server.close();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		// 关闭所有客户端
		for (Client client : clients) {
			client.close();
		}
	}

	@Override
	public void run() {
		boolean noError = true;
		System.out.printf("[%s]Server listen on: %s.\n", CommonMethods.formatTime(), server.getLocalSocketAddress());
		while (!closed && noError) {
			try {
				Socket socket = server.accept();
				// 移除已关闭客户端
				for (int i = 0; i < clients.size(); i++) {
					if (clients.get(i).isClose()) {
						clients.remove(i);
						i--;
					}
				}
				// 移除已过期客户端
				for (int i = 0; i < clients.size(); i++) {
					if (clients.get(i).isExpire()) {
						clients.get(i).close();
						clients.remove(i);
						i--;
					}
				}
				// 是否建立客户端
				if (clients.size() < Config.MAX_CLIENT_NUM) {
					Client client = new Client(socket);
					clients.add(client);
					System.out.printf("[%s]New client(%d) connect, total client num %d.\n", CommonMethods.formatTime(),
							client.id, clients.size());
				} else {
					socket.close();
					System.out.printf("[%s]Client connect max %d, close connect.", CommonMethods.formatTime(),
							Config.MAX_CLIENT_NUM);
				}
			} catch (IOException e) {
				noError = false;
				e.printStackTrace();
			}
		}
		close();
	}

}
