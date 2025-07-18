package com.vcvnc.xiguavpn;
/*
 * 程序主函数
 */
public class VpnMain {
	public static void main(String args[]) {
		//处理参数
		if (args.length > 0) {
			Config.PORT = Integer.parseInt(args[0]);
		}
		//创建服务器
		new VpnServer();
	}
}
