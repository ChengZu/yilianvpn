package com.vcvnc.vpn.utils;

import android.content.Context;
import java.net.InetSocketAddress;
import java.util.ArrayList;
import java.util.List;


public class ProxyConfig {
	public static final ProxyConfig Instance = new ProxyConfig();
	public static String serverIp = "192.168.0.101";
	public static int serverPort = 80;
	public static int userName = 12345678;
	public static int userPwd = 87654321;
	public static String DNS_FIRST = "8.8.8.8";
	public static String DNS_SECOND = "8.8.4.4";
	public static int MUTE = 1500;
	public static String errorMsg = "";
	private List<VpnStatusListener> mVpnStatusListeners=new ArrayList<>();

	private ProxyConfig() {

	}


	public void registerVpnStatusListener(VpnStatusListener vpnStatusListener) {
		mVpnStatusListeners.add(vpnStatusListener);
	}
	public void unregisterVpnStatusListener(VpnStatusListener vpnStatusListener) {
		mVpnStatusListeners.remove(vpnStatusListener);
	}
	public void cleanVpnStatusListener() {
		mVpnStatusListeners.clear();
	}

	public void onVpnStart(Context context) {
		VpnStatusListener[] vpnStatusListeners = new VpnStatusListener[mVpnStatusListeners.size()];
		mVpnStatusListeners.toArray(vpnStatusListeners);
		for(VpnStatusListener listener :vpnStatusListeners){
			listener.onVpnStart(context);
		}
	}

	public void onVpnEnd(Context context) {
		VpnStatusListener[] vpnStatusListeners = new VpnStatusListener[mVpnStatusListeners.size()];
		mVpnStatusListeners.toArray(vpnStatusListeners);
		for(VpnStatusListener listener :vpnStatusListeners){
			listener.onVpnEnd(context);
		}
	}

	public static InetSocketAddress getVpnAddress() {
		return new InetSocketAddress(serverIp, serverPort);
	}
	public IPAddress getDefaultLocalIP() {
		return new IPAddress("10.8.0.2", 32);
	}

	public interface VpnStatusListener {
		void onVpnStart(Context context);

		void onVpnEnd(Context context);
	}

	public static class IPAddress {
		public final String Address;
		public final int PrefixLength;

		public IPAddress(String address, int prefixLength) {
			Address = address;
			PrefixLength = prefixLength;
		}

		public IPAddress(String ipAddressString) {
			String[] arrStrings = ipAddressString.split("/");
			String address = arrStrings[0];
			int prefixLength = 32;
			if (arrStrings.length > 1) {
				prefixLength = Integer.parseInt(arrStrings[1]);
			}

			this.Address = address;
			this.PrefixLength = prefixLength;
		}

		@Override
		public boolean equals(Object o) {
			if (o == null || !(o instanceof IPAddress)) {
				return false;
			} else {
				return this.toString().equals(o.toString());
			}
		}

		@Override
		public String toString() {
			return String.format("%s:%d", Address, PrefixLength);
		}
	}
}
