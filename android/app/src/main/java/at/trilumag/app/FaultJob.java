package at.trilumag.app;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobService;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.ConnectivityManager;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.Network;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.Inet4Address;
import java.net.URL;

/**
 * Sieht etwa alle 15 Minuten im Hintergrund nach, ob alle bekannten Panelwände erreichbar sind und ihre Panels
 * antworten, und meldet sich bei einer Störung. Nur im eigenen WLAN: ist das Handy unterwegs, wird nichts gemeldet.
 */
public class FaultJob extends JobService {
    static final int JOB_ID = 4711;
    static final String CH = "stoerung";

    static void schedule(Context c, boolean on) {
        JobScheduler js = (JobScheduler) c.getSystemService(Context.JOB_SCHEDULER_SERVICE);
        if (!on) { js.cancel(JOB_ID); return; }
        JobInfo ji = new JobInfo.Builder(JOB_ID, new ComponentName(c, FaultJob.class))
                .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                .setPeriodic(15 * 60 * 1000L)
                .setPersisted(true)
                .build();
        js.schedule(ji);
    }

    @Override public boolean onStartJob(JobParameters p) {
        new Thread(() -> { try { check(this); } catch (Exception ignored) { } jobFinished(p, false); }).start();
        return true;
    }
    @Override public boolean onStopJob(JobParameters p) { return true; }

    /** Erste drei Teile der IPv4-Adresse des Handys im WLAN (z. B. "192.168.1."), sonst null */
    static String homePrefix(Context c) {
        ConnectivityManager cm = (ConnectivityManager) c.getSystemService(Context.CONNECTIVITY_SERVICE);
        Network n = cm.getActiveNetwork(); if (n == null) return null;
        LinkProperties lp = cm.getLinkProperties(n); if (lp == null) return null;
        for (LinkAddress la : lp.getLinkAddresses()) if (la.getAddress() instanceof Inet4Address) {
            String ip = la.getAddress().getHostAddress(); return ip.substring(0, ip.lastIndexOf('.') + 1);
        }
        return null;
    }

    static String get(String url) {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL(url).openConnection(); c.setConnectTimeout(3000); c.setReadTimeout(3000);
            if (c.getResponseCode() != 200) return null;
            try (InputStream in = c.getInputStream()) { ByteArrayOutputStream o = new ByteArrayOutputStream(); byte[] b = new byte[2048]; int k; while ((k = in.read(b)) > 0) o.write(b, 0, k); return o.toString("UTF-8"); }
        } catch (Exception e) { return null; } finally { if (c != null) c.disconnect(); }
    }

    static void check(Context ctx) throws Exception {
        SharedPreferences prefs = ctx.getSharedPreferences("walls", MODE_PRIVATE);
        if (!prefs.getBoolean("faultOn", false)) return;
        String pre = homePrefix(ctx); if (pre == null) return;
        JSONArray a = new JSONArray(prefs.getString("known", "[]"));
        SharedPreferences.Editor ed = prefs.edit();
        for (int i = 0; i < a.length(); i++) {
            JSONObject w = a.getJSONObject(i); String ip = w.getString("ip"), name = w.optString("name", ip);
            if (!ip.startsWith(pre)) continue;                    // andere Netz: Handy ist nicht zu Hause
            String r = get("http://" + ip + "/api/health");
            String st;
            if (r == null) st = "off";
            else { JSONObject j = new JSONObject(r); name = j.optString("n", name); st = j.optBoolean("bad") ? "bad" : "ok"; }
            String key = "fault_" + ip, last = prefs.getString(key, "ok");
            // erst beim zweiten Mal hintereinander "nicht erreichbar" melden (kurz aus oder neu gestartet ist keine Störung)
            if (st.equals("off") && !last.startsWith("off")) { ed.putString(key, "off1"); continue; }
            if (st.equals("off") && last.equals("off1")) { notify(ctx, ip, ctx.getString(R.string.fault_off, name)); ed.putString(key, "off"); continue; }
            if (st.equals("bad") && !last.equals("bad")) notify(ctx, ip, ctx.getString(R.string.fault_panel, name));
            if (st.equals("ok") && (last.equals("off") || last.equals("bad"))) cancel(ctx, ip);
            if (!st.equals("off")) ed.putString(key, st);
        }
        ed.apply();
    }

    static void notify(Context c, String ip, String text) {
        NotificationManager nm = (NotificationManager) c.getSystemService(Context.NOTIFICATION_SERVICE);
        nm.createNotificationChannel(new NotificationChannel(CH, c.getString(R.string.fault_channel), NotificationManager.IMPORTANCE_DEFAULT));
        PendingIntent pi = PendingIntent.getActivity(c, 0, new Intent(c, MainActivity.class), PendingIntent.FLAG_IMMUTABLE);
        Notification n = new Notification.Builder(c, CH).setSmallIcon(R.drawable.ic_stat).setContentTitle(c.getString(R.string.app_name))
                .setContentText(text).setStyle(new Notification.BigTextStyle().bigText(text)).setContentIntent(pi).setAutoCancel(true).build();
        nm.notify(ip.hashCode(), n);
    }
    static void cancel(Context c, String ip) { ((NotificationManager) c.getSystemService(Context.NOTIFICATION_SERVICE)).cancel(ip.hashCode()); }
}
