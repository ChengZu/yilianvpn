package com.vcvnc.app;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;


import com.vcvnc.vpn.utils.ProxyConfig;
import com.vcvnc.vpn.utils.VpnServiceHelper;
import com.vcvnc.xiguavpn.R;

public class OptionsFragment extends Fragment {
    private EditText ip;
    private EditText port;
    private EditText userName;
    private EditText userPwd;
    private EditText dns1;
    private EditText dns2;
    private Button saveBtn;
    private View root;
    public static final String myPref ="preferenceName";



    public View onCreateView(@NonNull LayoutInflater inflater,
                             ViewGroup container, Bundle savedInstanceState) {
        root = inflater.inflate(R.layout.fragment_options, container, false);

        ip = root.findViewById(R.id.options_ip_value);
        port = root.findViewById(R.id.options_port_value);
        userName = root.findViewById(R.id.options_user_name_value);
        userPwd = root.findViewById(R.id.options_user_password_value);
        dns1 = root.findViewById(R.id.options_dns1_value);
        dns2 = root.findViewById(R.id.options_dns2_value);
        saveBtn = root.findViewById(R.id.options_save_btn);

        //获取配置
        if(!getPreferenceValue("ip").equals("0")){
            ProxyConfig.serverIp= getPreferenceValue("ip");
            ProxyConfig.serverPort = Integer.parseInt(getPreferenceValue("port"));
            ProxyConfig.userName= Integer.parseInt(getPreferenceValue("username"));
            ProxyConfig.userPwd = Integer.parseInt(getPreferenceValue("user-password"));
            ProxyConfig.DNS_FIRST = getPreferenceValue("dns1");
            ProxyConfig.DNS_SECOND = getPreferenceValue("dns2");
        }
        //设置UI信息
        setUIIP(ProxyConfig.serverIp);
        setUIPort(ProxyConfig.serverPort);
        setUIUserName(ProxyConfig.userName);
        setUIUserPwd(ProxyConfig.userPwd);
        setUIDns1(ProxyConfig.DNS_FIRST);
        setUIDns2(ProxyConfig.DNS_SECOND);

        saveBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                closeVpn();
                ProxyConfig.serverIp= getUIIP();
                ProxyConfig.serverPort = getUIPort();
                ProxyConfig.userName= getUIUserName();
                ProxyConfig.userPwd = getUIUserPwd();
                ProxyConfig.DNS_FIRST = getUIDns1();
                ProxyConfig.DNS_SECOND = getUIDns2();


                writeToPreference("ip", ProxyConfig.serverIp);
                writeToPreference("port", ProxyConfig.serverPort + "");
                writeToPreference("username", ProxyConfig.userName + "");
                writeToPreference("user-password", ProxyConfig.userPwd + "");
                writeToPreference("dns1", ProxyConfig.DNS_FIRST);
                writeToPreference("dns2", ProxyConfig.DNS_SECOND);

                showAlertDialog(getString(R.string.apply_success));
            }
        });

        return root;
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

    private void showAlertDialog(String msg) {
        AlertDialog.Builder builder = new AlertDialog.Builder(root.getContext()).setIcon(R.mipmap.ic_launcher).setTitle(getString(R.string.result))
                .setMessage(msg).setPositiveButton(getString(R.string.ok), new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {
                        //Toast.makeText(root.getContext(), "成功！", Toast.LENGTH_SHORT).show();
                    }
                });
        builder.create().show();
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

    public String getUIIP() {
        return ip.getText().toString();
    }

    public void setUIIP(String value) {
        this.ip.setText(value);
    }

    public int getUIPort() {
        return Integer.parseInt(port.getText().toString());
    }

    public void setUIPort(int value) {
        this.port.setText(value + "");
    }

    public int getUIUserName() {
        return Integer.parseInt(userName.getText().toString());
    }

    public void setUIUserName(int value) {
        this.userName.setText(value + "");
    }

    public int getUIUserPwd() {
        return Integer.parseInt(userPwd.getText().toString());
    }

    public void setUIUserPwd(int value) {
        this.userPwd.setText(value + "");
    }

    public String getUIDns1() {
        return this.dns1.getText().toString();
    }

    public void setUIDns1(String value) {
        this.dns1.setText(value);
    }

    public String getUIDns2() {
        return this.dns2.getText().toString();
    }

    public void setUIDns2(String value) {
        this.dns2.setText(value);
    }
}
