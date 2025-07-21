package com.vcvnc.app;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import com.vcvnc.vpn.utils.ProxyConfig;
import com.vcvnc.vpn.utils.VpnServiceHelper;
import com.vcvnc.xiguavpn.R;

public class HomeFragment extends Fragment {
    private Handler handler = new Handler();;
    private TextView infoMsg;
    private TextView errorMsg;
    private Button startRunBtn;
    private Button stopRunBtn;
    private static boolean isRunning = false;
    private static boolean onStart = false;
    private static boolean onStop = false;
    View root;
    public static final String myPref ="preferenceName";
    ProxyConfig.VpnStatusListener vpnStatusListener = new ProxyConfig.VpnStatusListener() {
        @Override
        public void onVpnStart(Context context) {
            handler.post(new Runnable() {
                             public void run() {
                                 isRunning = true;
                                 onStart = false;
                                 onStop = false;
                                 updateUI();
                                 System.out.println("vpn start");
                             }
                         }
            );
        }

        @Override
        public void onVpnEnd(Context context) {
            handler.post(new Runnable() {
                             public void run() {
                                 isRunning = false;
                                 onStart = false;
                                 onStop = false;
                                 updateUI();
                                 System.out.println("vpn stop");
                             }
                         }
            );
        }
    };

    public View onCreateView(@NonNull LayoutInflater inflater,
                             ViewGroup container, Bundle savedInstanceState) {
        root = inflater.inflate(R.layout.fragment_home, container, false);
        infoMsg = root.findViewById(R.id.info_msg);
        errorMsg = root.findViewById(R.id.error_msg);

        if(!getPreferenceValue("ip").equals("0")){
            ProxyConfig.serverIp= getPreferenceValue("ip");
            ProxyConfig.serverPort = Integer.parseInt(getPreferenceValue("port"));
            ProxyConfig.userName= Integer.parseInt(getPreferenceValue("username"));
            ProxyConfig.userPwd = Integer.parseInt(getPreferenceValue("user-password"));
            ProxyConfig.DNS_FIRST = getPreferenceValue("dns1");
            ProxyConfig.DNS_SECOND = getPreferenceValue("dns2");
        }


        startRunBtn = root.findViewById(R.id.start_run);
        startRunBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if(!isRunning && !onStart){
                    if(getPreferenceValue("new-install").equals("0")){
                        writeToPreference("new-install", "no");
                        startVPN();
                        updateUI();
                    }else{
                        onStart = true;
                        startVPN();
                        updateUI();
                    }
                }
            }
        });
        stopRunBtn = root.findViewById(R.id.stop_run);
        stopRunBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if(isRunning && !onStop){
                    onStop = true;
                    closeVpn();
                    updateUI();
                }
            }
        });
        ProxyConfig.Instance.cleanVpnStatusListener();
        ProxyConfig.Instance.registerVpnStatusListener(vpnStatusListener);
        updateUI();
        return root;
    }

    public String getPreferenceValue(String key)
    {
        SharedPreferences sp = root.getContext().getSharedPreferences(myPref,0);
        String str = sp.getString(key,"0");
        return str;
    }

    public void writeToPreference(String key, String value)
    {
        SharedPreferences.Editor editor = root.getContext().getSharedPreferences(myPref,0).edit();
        editor.putString(key, value);
        editor.commit();
    }

    private void updateUI() {
        if(onStart){
            startRunBtn.setVisibility(View.VISIBLE);
            stopRunBtn.setVisibility(View.INVISIBLE);
            infoMsg.setText(getString(R.string.connecting));
        }else if(onStop){
            startRunBtn.setVisibility(View.INVISIBLE);
            stopRunBtn.setVisibility(View.VISIBLE);
            infoMsg.setText(getString(R.string.disconnecting));
        }else if (VpnServiceHelper.vpnRunningStatus()){
            startRunBtn.setVisibility(View.INVISIBLE);
            stopRunBtn.setVisibility(View.VISIBLE);
            infoMsg.setText(getString(R.string.connected) + ProxyConfig.serverIp + ":" +ProxyConfig.serverPort);
        }else if(!VpnServiceHelper.vpnRunningStatus()){
            startRunBtn.setVisibility(View.VISIBLE);
            stopRunBtn.setVisibility(View.INVISIBLE);
            infoMsg.setText(getString(R.string.not_connect) + ProxyConfig.serverIp + ":" +ProxyConfig.serverPort);
        }
        String msg = "";
        if(ProxyConfig.errorMsg.length() > 0){
            msg = getString(R.string.error_msg) + ProxyConfig.errorMsg;
        }
        errorMsg.setText(msg);
    }

    private void startVPN() {
        if (!VpnServiceHelper.vpnRunningStatus()) {
            VpnServiceHelper.changeVpnRunningStatus(this.getContext(), true);
        }
    }

    private void closeVpn() {
        if (VpnServiceHelper.vpnRunningStatus()) {
            VpnServiceHelper.changeVpnRunningStatus(this.getContext(), false);
        }
    }

}
