package at.trilumag.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Startseite wie bei der WLED-App: alle Trilumag im WLAN als Liste, mit Ein/Aus und Helligkeit.
 * Antippen öffnet die Steuerung der Wand (die Web-App aus der Wand selbst).
 */
public class MainActivity extends Activity {
    static final String REPO = "mariofritzer/Trilumag";

    /** Eine Wand in der Liste */
    static class Wall {
        String ip, name, ver = "";
        boolean manual, reachable = true, on, busy;
        int bri = 128, color = Color.GRAY;
        View row; TextView title, sub; View dot; Switch sw; SeekBar bar;
    }

    final Map<String, Wall> walls = new LinkedHashMap<>();
    final Handler ui = new Handler(Looper.getMainLooper());
    final ExecutorService pool = Executors.newFixedThreadPool(4);
    LinearLayout list; TextView hint; LinearLayout updateBox; TextView updateText; Button updateBtn;
    NsdManager nsd; NsdManager.DiscoveryListener disc; WifiManager.MulticastLock lock;
    final ArrayDeque<NsdServiceInfo> resolveQueue = new ArrayDeque<>(); boolean resolving;
    SharedPreferences prefs;
    boolean visible;
    LinearLayout root; Updater updater;

    int dp(float v) { return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics()); }

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        prefs = getSharedPreferences("walls", MODE_PRIVATE);
        root = new LinearLayout(this); root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(getColor(R.color.bg));

        // Kopfzeile: Titel, Neu suchen, Hinzufügen
        LinearLayout head = new LinearLayout(this); head.setGravity(Gravity.CENTER_VERTICAL);
        head.setPadding(dp(18), dp(16), dp(8), dp(8));
        TextView t = new TextView(this); t.setText(R.string.app_name); t.setTextColor(getColor(R.color.fg));
        t.setTextSize(24); t.setTypeface(Typeface.DEFAULT_BOLD);
        head.addView(t, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        Button refresh = flatButton("↻"); refresh.setContentDescription(getString(R.string.refresh));
        refresh.setOnClickListener(v -> restartDiscovery());
        Button add = flatButton("＋"); add.setOnClickListener(v -> askAdd());
        head.addView(refresh); head.addView(add);
        root.addView(head);

        // Hinweis auf eine neue App-Version
        updateBox = new LinearLayout(this); updateBox.setGravity(Gravity.CENTER_VERTICAL);
        updateBox.setPadding(dp(14), dp(10), dp(10), dp(10)); updateBox.setVisibility(View.GONE);
        GradientDrawable ub = new GradientDrawable(); ub.setColor(Color.parseColor("#2B2410")); ub.setCornerRadius(dp(12)); ub.setStroke(dp(1), Color.parseColor("#5A4A1A"));
        updateBox.setBackground(ub);
        updateText = new TextView(this); updateText.setTextColor(Color.parseColor("#F0D890")); updateText.setTextSize(14);
        updateBox.addView(updateText, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        updateBtn = new Button(this); updateBtn.setText(R.string.update_btn); updateBtn.setAllCaps(false);
        updateBox.addView(updateBtn);
        LinearLayout.LayoutParams ulp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        ulp.setMargins(dp(14), 0, dp(14), dp(8));
        root.addView(updateBox, ulp);

        ScrollView sv = new ScrollView(this);
        LinearLayout inner = new LinearLayout(this); inner.setOrientation(LinearLayout.VERTICAL); inner.setPadding(dp(14), 0, dp(14), dp(24));
        list = new LinearLayout(this); list.setOrientation(LinearLayout.VERTICAL);
        inner.addView(list);
        hint = new TextView(this); hint.setTextColor(getColor(R.color.muted)); hint.setTextSize(14); hint.setPadding(dp(6), dp(16), dp(6), dp(8));
        hint.setText(R.string.searching);
        inner.addView(hint);
        TextView ver = new TextView(this); ver.setTextColor(Color.parseColor("#555555")); ver.setTextSize(12); ver.setPadding(dp(6), dp(24), dp(6), 0);
        ver.setText(getString(R.string.version, BuildConfig.VERSION_NAME));
        inner.addView(ver);
        sv.addView(inner);
        root.addView(sv, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));
        setContentView(root);

        // zuletzt bekannte Wände sofort zeigen
        try {
            JSONArray a = new JSONArray(prefs.getString("known", "[]"));
            for (int i = 0; i < a.length(); i++) {
                JSONObject o = a.getJSONObject(i);
                Wall w = new Wall(); w.ip = o.getString("ip"); w.name = o.optString("name", w.ip); w.manual = o.optBoolean("manual"); w.ver = o.optString("ver");
                walls.put(w.ip, w); addRow(w);
            }
        } catch (Exception ignored) { }
        nsd = (NsdManager) getSystemService(Context.NSD_SERVICE);
        checkUpdate();
    }

    Button flatButton(String s) {
        Button b = new Button(this); b.setText(s); b.setTextSize(22); b.setTextColor(getColor(R.color.fg));
        b.setBackgroundColor(Color.TRANSPARENT); b.setMinWidth(dp(48)); b.setMinimumWidth(dp(48));
        return b;
    }

    @Override protected void onResume() { super.onResume(); visible = true; startDiscovery(); pollAll(); if (updater != null) updater.onResume(); }
    @Override public void onBackPressed() { if (updater != null && updater.isActive()) return; super.onBackPressed(); }   // während des Updates nicht wegdrücken
    @Override protected void onPause() { super.onPause(); visible = false; stopDiscovery(); }

    // ---------- Wände im WLAN finden (mDNS, Dienst _wled._tcp; Trilumag erkennt man am Eintrag "tl") ----------
    void startDiscovery() {
        if (disc != null) return;
        WifiManager wm = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        if (wm != null) { lock = wm.createMulticastLock("trilumag"); lock.setReferenceCounted(false); lock.acquire(); }
        disc = new NsdManager.DiscoveryListener() {
            public void onStartDiscoveryFailed(String s, int e) { }
            public void onStopDiscoveryFailed(String s, int e) { }
            public void onDiscoveryStarted(String s) { }
            public void onDiscoveryStopped(String s) { }
            public void onServiceLost(NsdServiceInfo i) { }
            public void onServiceFound(NsdServiceInfo i) { ui.post(() -> { resolveQueue.add(i); nextResolve(); }); }
        };
        try { nsd.discoverServices("_wled._tcp", NsdManager.PROTOCOL_DNS_SD, disc); } catch (Exception e) { disc = null; }
        ui.postDelayed(() -> { if (walls.isEmpty()) hint.setText(R.string.none_found); }, 6000);
    }
    void stopDiscovery() {
        if (disc != null) { try { nsd.stopServiceDiscovery(disc); } catch (Exception ignored) { } disc = null; }
        if (lock != null && lock.isHeld()) lock.release();
    }
    void restartDiscovery() { stopDiscovery(); hint.setText(R.string.searching); hint.setVisibility(View.VISIBLE); startDiscovery(); pollAll(); }

    // Android kann nur einen Dienst auf einmal auflösen
    @SuppressWarnings("deprecation")
    void nextResolve() {
        if (resolving || resolveQueue.isEmpty()) return;
        resolving = true;
        NsdServiceInfo si = resolveQueue.poll();
        nsd.resolveService(si, new NsdManager.ResolveListener() {
            public void onResolveFailed(NsdServiceInfo i, int e) { ui.post(() -> { resolving = false; nextResolve(); }); }
            public void onServiceResolved(NsdServiceInfo i) {
                Map<String, byte[]> at = i.getAttributes();
                boolean tl = at != null && at.containsKey("tl");
                String name = i.getServiceName();
                boolean byName = name != null && name.toLowerCase().contains("trilumag");
                String ip = i.getHost() != null ? i.getHost().getHostAddress() : null;
                String ver = tl && at.get("tl") != null ? new String(at.get("tl"), StandardCharsets.UTF_8) : "";
                String nm = at != null && at.get("name") != null ? new String(at.get("name"), StandardCharsets.UTF_8) : name;
                ui.post(() -> {
                    resolving = false;
                    if ((tl || byName) && ip != null && !ip.contains(":")) found(ip, nm, ver);
                    nextResolve();
                });
            }
        });
    }

    void found(String ip, String name, String ver) {
        Wall w = walls.get(ip);
        if (w == null) { w = new Wall(); w.ip = ip; walls.put(ip, w); addRow(w); }
        w.name = name; if (ver != null && !ver.isEmpty()) w.ver = ver;
        w.reachable = true; updateRow(w); save(); poll(w);
    }

    void save() {
        JSONArray a = new JSONArray();
        for (Wall w : walls.values()) {
            try { JSONObject o = new JSONObject(); o.put("ip", w.ip); o.put("name", w.name); o.put("manual", w.manual); o.put("ver", w.ver); a.put(o); } catch (Exception ignored) { }
        }
        prefs.edit().putString("known", a.toString()).apply();
    }

    // ---------- Liste ----------
    void addRow(Wall w) {
        hint.setVisibility(View.GONE);
        LinearLayout card = new LinearLayout(this); card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(16), dp(14), dp(12), dp(8));
        GradientDrawable bg = new GradientDrawable(); bg.setColor(getColor(R.color.card)); bg.setCornerRadius(dp(16)); card.setBackground(bg);
        LinearLayout top = new LinearLayout(this); top.setGravity(Gravity.CENTER_VERTICAL);
        w.dot = new View(this); GradientDrawable d = new GradientDrawable(); d.setShape(GradientDrawable.OVAL); d.setColor(Color.GRAY); w.dot.setBackground(d);
        top.addView(w.dot, new LinearLayout.LayoutParams(dp(22), dp(22)));
        LinearLayout texts = new LinearLayout(this); texts.setOrientation(LinearLayout.VERTICAL); texts.setPadding(dp(14), 0, dp(8), 0);
        w.title = new TextView(this); w.title.setTextColor(getColor(R.color.fg)); w.title.setTextSize(18); w.title.setTypeface(Typeface.DEFAULT_BOLD);
        w.sub = new TextView(this); w.sub.setTextColor(getColor(R.color.muted)); w.sub.setTextSize(13);
        texts.addView(w.title); texts.addView(w.sub);
        top.addView(texts, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        w.sw = new Switch(this);
        w.sw.setOnClickListener(v -> send(w, "{\"on\":" + w.sw.isChecked() + "}"));
        top.addView(w.sw);
        card.addView(top);
        w.bar = new SeekBar(this); w.bar.setMax(254);
        w.bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar s, int p, boolean user) { if (user) { w.bri = p + 1; } }
            public void onStartTrackingTouch(SeekBar s) { w.busy = true; }
            public void onStopTrackingTouch(SeekBar s) { w.busy = false; send(w, "{\"on\":true,\"bri\":" + w.bri + "}"); }
        });
        LinearLayout.LayoutParams blp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(40)); blp.topMargin = dp(6);
        card.addView(w.bar, blp);
        card.setOnClickListener(v -> open(w));
        card.setOnLongClickListener(v -> { askRemove(w); return true; });
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.bottomMargin = dp(10);
        list.addView(card, lp);
        w.row = card;
        updateRow(w);
    }

    void updateRow(Wall w) {
        if (w.row == null) return;
        w.title.setText(w.name);
        w.sub.setText(w.reachable ? w.ip + (w.ver.isEmpty() ? "" : " · " + w.ver) : w.ip + " · " + getString(R.string.unreachable));
        ((GradientDrawable) w.dot.getBackground()).setColor(w.reachable && w.on ? w.color : Color.parseColor("#333333"));
        w.sw.setChecked(w.on); w.sw.setEnabled(w.reachable);
        if (!w.busy) w.bar.setProgress(Math.max(0, w.bri - 1));
        w.bar.setEnabled(w.reachable);
        w.row.setAlpha(w.reachable ? 1f : .55f);
    }

    void open(Wall w) {
        Intent i = new Intent(this, WallActivity.class);
        i.putExtra("ip", w.ip); i.putExtra("name", w.name);
        startActivity(i);
    }

    void askAdd() {
        EditText e = new EditText(this); e.setHint(R.string.add_hint); e.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        new AlertDialog.Builder(this).setTitle(R.string.add_title).setView(e)
            .setPositiveButton(R.string.ok, (d, x) -> {
                String ip = e.getText().toString().trim();
                if (ip.isEmpty()) return;
                Wall w = walls.get(ip);
                if (w == null) { w = new Wall(); w.ip = ip; w.name = ip; w.manual = true; walls.put(ip, w); addRow(w); }
                save(); poll(w);
            }).setNegativeButton(R.string.cancel, null).show();
    }

    void askRemove(Wall w) {
        new AlertDialog.Builder(this).setMessage(getString(R.string.remove_q, w.name))
            .setPositiveButton(R.string.remove, (d, x) -> { walls.remove(w.ip); list.removeView(w.row); save(); })
            .setNegativeButton(R.string.cancel, null).show();
    }

    // ---------- Zustand abfragen und schalten (WLED-Schnittstelle der Wand: /json/state) ----------
    void pollAll() {
        for (Wall w : walls.values()) poll(w);
        if (visible) ui.postDelayed(this::pollAllTick, 4000);
    }
    void pollAllTick() { if (visible) { for (Wall w : walls.values()) poll(w); ui.postDelayed(this::pollAllTick, 4000); } }

    void poll(Wall w) {
        pool.execute(() -> {
            String r = http("GET", "http://" + w.ip + "/json/state", null, 2500);
            ui.post(() -> {
                if (r == null) { w.reachable = false; updateRow(w); return; }
                try {
                    JSONObject o = new JSONObject(r);
                    w.reachable = true; w.on = o.optBoolean("on"); if (!w.busy) w.bri = o.optInt("bri", w.bri);
                    JSONArray seg = o.optJSONArray("seg");
                    if (seg != null && seg.length() > 0) {
                        JSONArray col = seg.getJSONObject(0).optJSONArray("col");
                        if (col != null && col.length() > 0) {
                            JSONArray c = col.getJSONArray(0);
                            int rr = c.optInt(0), gg = c.optInt(1), bb = c.optInt(2), ww = c.length() > 3 ? c.optInt(3) : 0;
                            w.color = Color.rgb(Math.min(255, rr + ww), Math.min(255, gg + ww), Math.min(255, bb + ww));
                        }
                    }
                } catch (Exception e) { w.reachable = false; }
                updateRow(w);
            });
        });
    }

    void send(Wall w, String json) {
        pool.execute(() -> { http("POST", "http://" + w.ip + "/json/state", json, 3000); ui.postDelayed(() -> poll(w), 300); });
    }

    static String http(String method, String url, String body, int timeout) {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL(url).openConnection();
            c.setConnectTimeout(timeout); c.setReadTimeout(timeout); c.setRequestMethod(method);
            c.setRequestProperty("User-Agent", "Trilumag-App");
            if (body != null) {
                c.setDoOutput(true); c.setRequestProperty("Content-Type", "application/json");
                try (OutputStream os = c.getOutputStream()) { os.write(body.getBytes(StandardCharsets.UTF_8)); }
            }
            if (c.getResponseCode() / 100 != 2) return null;
            try (InputStream is = c.getInputStream()) {
                ByteArrayOutputStream out = new ByteArrayOutputStream(); byte[] buf = new byte[4096]; int n;
                while ((n = is.read(buf)) > 0) out.write(buf, 0, n);
                return out.toString("UTF-8");
            }
        } catch (Exception e) { return null; }
        finally { if (c != null) c.disconnect(); }
    }

    // ---------- Gibt es eine neuere App? (Releases auf GitHub mit einer Datei trilumag-app-<Version>.apk) ----------
    static final java.util.regex.Pattern APK = java.util.regex.Pattern.compile("^trilumag-app-([0-9][0-9.]*)\\.apk$");
    void checkUpdate() {
        long last = prefs.getLong("appChecked", 0);
        String known = prefs.getString("appVer", "");
        String knownUrl = prefs.getString("appUrl", "");
        if (!known.isEmpty() && newer(known, BuildConfig.VERSION_NAME)) showUpdate(known, knownUrl);
        if (System.currentTimeMillis() - last < 3 * 3600 * 1000L) return;     // höchstens alle 3 Stunden nachsehen
        pool.execute(() -> {
            String r = http("GET", "https://api.github.com/repos/" + REPO + "/releases?per_page=100", null, 10000);
            if (r == null) return;
            try {
                JSONArray a = new JSONArray(r);
                String best = null, bestUrl = null;
                for (int i = 0; i < a.length(); i++) {
                    JSONObject rel = a.getJSONObject(i);
                    if (rel.optBoolean("draft") || rel.optBoolean("prerelease")) continue;
                    JSONArray as = rel.optJSONArray("assets");
                    for (int k = 0; as != null && k < as.length(); k++) {
                        java.util.regex.Matcher m = APK.matcher(as.getJSONObject(k).optString("name"));
                        if (m.matches() && (best == null || newer(m.group(1), best))) { best = m.group(1); bestUrl = as.getJSONObject(k).optString("browser_download_url"); }
                    }
                }
                if (best == null) return;
                String v = best, u = bestUrl;
                prefs.edit().putLong("appChecked", System.currentTimeMillis()).putString("appVer", v).putString("appUrl", u).apply();
                if (newer(v, BuildConfig.VERSION_NAME)) ui.post(() -> showUpdate(v, u));
            } catch (Exception ignored) { }
        });
    }

    void showUpdate(String v, String url) {
        updateText.setText(getString(R.string.update_avail, v));
        updateBtn.setOnClickListener(x -> { if (updater == null || !updater.isActive()) { updater = new Updater(this, root, v, url); updater.start(); } });
        updateBox.setVisibility(View.VISIBLE);
    }

    /** a neuer als b? Versionen wie 0.7.51; "dev" gilt als ganz alt */
    static boolean newer(String a, String b) {
        String[] x = a.split("\\."), y = b.split("\\.");
        for (int i = 0; i < Math.max(x.length, y.length); i++) {
            int p = i < x.length ? num(x[i]) : 0, q = i < y.length ? num(y[i]) : 0;
            if (p != q) return p > q;
        }
        return false;
    }
    static int num(String s) { try { return Integer.parseInt(s.replaceAll("\\D", "")); } catch (Exception e) { return -1; } }

    @Override protected void onDestroy() { super.onDestroy(); pool.shutdownNow(); }
}
