package com.vcvnc.vpn.utils;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import com.vcvnc.vpn.service.VpnService;

public class VpnServiceHelper {
    static Context context;
    public static final int START_VPN_SERVICE_REQUEST_CODE = 1;
    private static VpnService sVpnService;

    public static void onVpnServiceCreated(VpnService vpnService) {
        sVpnService = vpnService;
        if(context==null){
            context=vpnService.getApplicationContext();
        }
    }

    public static void onVpnServiceDestroy() {
        sVpnService = null;
    }


    public static Context getContext() {
        return context;
    }

    public static boolean vpnRunningStatus() {
        if (sVpnService != null) {
            return sVpnService.vpnRunningStatus();
        }
        return false;
    }

    public static void changeVpnRunningStatus(Context context, boolean isStart) {
        ProxyConfig.errorMsg = "";
        if (context == null) {
            return;
        }
        if (isStart) {
            Intent intent = VpnService.prepare(context);
            if (intent == null) {
                startVpnService(context);
            } else {
                if (context instanceof Activity) {
                    ((Activity) context).startActivityForResult(intent, START_VPN_SERVICE_REQUEST_CODE);
                }
            }
        } else if (sVpnService != null) {
            boolean stopStatus = false;
            sVpnService.setVpnRunningStatus(stopStatus);
        }
    }

    public static void startVpnService(Context context) {
        if (context == null) {
            return;
        }
        context.startService(new Intent(context, VpnService.class));
    }
}
