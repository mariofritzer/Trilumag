package at.trilumag.app;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageInstaller;
import android.graphics.Color;
import android.graphics.RenderEffect;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Locale;

/**
 * Lädt die neue App im Hintergrund und installiert sie. Währenddessen liegt über der App eine Ebene:
 * dahinter alles verschwommen, davor Titel, Fortschritt und was gerade passiert.
 */
class Updater {
    static final String ACTION = "at.trilumag.app.INSTALL_DONE";
    final Activity a; final View content; final String ver, url;
    final Handler ui = new Handler(Looper.getMainLooper());
    FrameLayout layer; ProgressBar bar; TextView pct, step, size; Button btnA, btnB;
    volatile boolean cancelled; boolean waitingForPermission, active;
    File apk; BroadcastReceiver rx;

    Updater(Activity a, View content, String ver, String url) { this.a = a; this.content = content; this.ver = ver; this.url = url; }

    int dp(float v) { return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, a.getResources().getDisplayMetrics()); }

    boolean isActive() { return active; }

    void start() {
        active = true; cancelled = false;
        a.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (layer == null) buildLayer();
        layer.setVisibility(View.VISIBLE); layer.setAlpha(0f); layer.animate().alpha(1f).setDuration(250).start();
        if (Build.VERSION.SDK_INT >= 31) content.setRenderEffect(RenderEffect.createBlurEffect(dp(14), dp(14), Shader.TileMode.CLAMP));
        download();
    }

    void close() {
        active = false; cancelled = true; waitingForPermission = false;
        a.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (Build.VERSION.SDK_INT >= 31) content.setRenderEffect(null);
        if (layer != null) layer.animate().alpha(0f).setDuration(200).withEndAction(() -> layer.setVisibility(View.GONE)).start();
        if (rx != null) { try { a.unregisterReceiver(rx); } catch (Exception ignored) { } rx = null; }
    }

    // ---------- Oberfläche ----------
    void buildLayer() {
        layer = new FrameLayout(a);
        layer.setBackgroundColor(Build.VERSION.SDK_INT >= 31 ? Color.parseColor("#80000000") : Color.parseColor("#D9000000"));
        layer.setClickable(true); layer.setFocusable(true);      // nichts dahinter antippbar

        LinearLayout card = new LinearLayout(a); card.setOrientation(LinearLayout.VERTICAL); card.setGravity(Gravity.CENTER_HORIZONTAL);
        card.setPadding(dp(24), dp(24), dp(24), dp(20));
        GradientDrawable bg = new GradientDrawable(); bg.setColor(Color.parseColor("#F21A1A1A")); bg.setCornerRadius(dp(22)); bg.setStroke(dp(1), Color.parseColor("#33FFFFFF"));
        card.setBackground(bg); card.setElevation(dp(12));

        ImageView ic = new ImageView(a); ic.setImageResource(R.drawable.ic_fg);
        GradientDrawable icBg = new GradientDrawable(); icBg.setShape(GradientDrawable.OVAL); icBg.setColor(Color.parseColor("#111111")); ic.setBackground(icBg);
        card.addView(ic, new LinearLayout.LayoutParams(dp(72), dp(72)));

        TextView title = new TextView(a); title.setText(R.string.upd_title); title.setTextColor(Color.WHITE); title.setTextSize(20); title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setGravity(Gravity.CENTER); title.setPadding(0, dp(14), 0, dp(2));
        card.addView(title);
        TextView vv = new TextView(a); vv.setText(a.getString(R.string.upd_versions, BuildConfig.VERSION_NAME, ver)); vv.setTextColor(Color.parseColor("#9A9A9A")); vv.setTextSize(14); vv.setGravity(Gravity.CENTER);
        card.addView(vv);

        bar = new ProgressBar(a, null, android.R.attr.progressBarStyleHorizontal); bar.setMax(1000); bar.setIndeterminate(true);
        bar.setProgressTintList(android.content.res.ColorStateList.valueOf(Color.parseColor("#F0C040")));
        bar.setIndeterminateTintList(android.content.res.ColorStateList.valueOf(Color.parseColor("#F0C040")));
        LinearLayout.LayoutParams bl = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(10)); bl.topMargin = dp(22);
        card.addView(bar, bl);

        LinearLayout row = new LinearLayout(a); row.setPadding(0, dp(8), 0, 0);
        pct = new TextView(a); pct.setTextColor(Color.WHITE); pct.setTextSize(15); pct.setTypeface(Typeface.DEFAULT_BOLD);
        size = new TextView(a); size.setTextColor(Color.parseColor("#9A9A9A")); size.setTextSize(13); size.setGravity(Gravity.END);
        row.addView(pct, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1)); row.addView(size);
        card.addView(row, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        step = new TextView(a); step.setTextColor(Color.parseColor("#D8D8D8")); step.setTextSize(15); step.setGravity(Gravity.CENTER); step.setPadding(0, dp(14), 0, dp(6));
        card.addView(step, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        LinearLayout btns = new LinearLayout(a); btns.setGravity(Gravity.CENTER); btns.setPadding(0, dp(8), 0, 0);
        btnB = new Button(a); btnB.setAllCaps(false); btnA = new Button(a); btnA.setAllCaps(false);
        btns.addView(btnB); btns.addView(btnA);
        card.addView(btns);

        FrameLayout.LayoutParams cl = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER);
        cl.leftMargin = cl.rightMargin = dp(28);
        layer.addView(card, cl);
        ((ViewGroup) a.getWindow().getDecorView()).addView(layer, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    }

    void show(int stepRes, Object... args) { ui.post(() -> step.setText(a.getString(stepRes, args))); }

    void buttons(String aTxt, View.OnClickListener aOn, String bTxt, View.OnClickListener bOn) {
        ui.post(() -> {
            btnA.setVisibility(aTxt == null ? View.GONE : View.VISIBLE); btnA.setText(aTxt); btnA.setOnClickListener(aOn);
            btnB.setVisibility(bTxt == null ? View.GONE : View.VISIBLE); btnB.setText(bTxt); btnB.setOnClickListener(bOn);
        });
    }

    void fail(int res, Object... args) {
        ui.post(() -> {
            bar.setIndeterminate(false); pct.setText(""); size.setText("");
            step.setText(a.getString(res, args)); step.setTextColor(Color.parseColor("#FF8A80"));
        });
        buttons(a.getString(R.string.upd_retry), v -> { ui.post(() -> step.setTextColor(Color.parseColor("#D8D8D8"))); cancelled = false; download(); },
                a.getString(R.string.upd_close), v -> close());
    }

    // ---------- 1. Herunterladen ----------
    void download() {
        apk = new File(a.getCacheDir(), "update.apk");
        ui.post(() -> { bar.setIndeterminate(true); pct.setText(""); size.setText(""); });
        show(R.string.upd_connect);
        buttons(null, null, a.getString(R.string.cancel), v -> close());
        new Thread(() -> {
            HttpURLConnection c = null;
            try {
                c = (HttpURLConnection) new URL(url).openConnection();
                c.setInstanceFollowRedirects(true); c.setConnectTimeout(10000); c.setReadTimeout(20000);
                int code = c.getResponseCode();
                if (code != 200) { fail(R.string.upd_err_http, code); return; }
                long total = c.getContentLengthLong();
                show(R.string.upd_download);
                long got = 0, lastUi = 0;
                try (InputStream in = c.getInputStream(); OutputStream out = new FileOutputStream(apk)) {
                    byte[] b = new byte[16384]; int n;
                    while ((n = in.read(b)) > 0) {
                        if (cancelled) return;
                        out.write(b, 0, n); got += n;
                        long now = System.currentTimeMillis();
                        if (now - lastUi > 80) { lastUi = now; progress(got, total); }
                    }
                }
                progress(got, total);
                if (total > 0 && got != total) { fail(R.string.upd_err_short); return; }
                if (!cancelled) ui.post(this::verify);
            } catch (Exception e) {
                if (!cancelled) fail(R.string.upd_err_net, e.getClass().getSimpleName());
            } finally { if (c != null) c.disconnect(); }
        }).start();
    }

    void progress(long got, long total) {
        ui.post(() -> {
            if (total > 0) {
                bar.setIndeterminate(false); bar.setProgress((int) (got * 1000 / total));
                pct.setText(String.format(Locale.getDefault(), "%d %%", got * 100 / total));
                size.setText(String.format(Locale.getDefault(), "%.1f / %.1f MB", got / 1048576.0, total / 1048576.0));
            } else size.setText(String.format(Locale.getDefault(), "%.1f MB", got / 1048576.0));
        });
    }

    // ---------- 2. Datei prüfen ----------
    void verify() {
        show(R.string.upd_check);
        bar.setIndeterminate(true);
        PackageInfo pi = a.getPackageManager().getPackageArchiveInfo(apk.getPath(), 0);
        if (pi == null || !a.getPackageName().equals(pi.packageName)) { fail(R.string.upd_err_file); return; }
        if (pi.getLongVersionCode() <= BuildConfig.VERSION_CODE) { fail(R.string.upd_err_old); return; }
        install();
    }

    // ---------- 3. Installieren ----------
    void install() {
        if (!a.getPackageManager().canRequestPackageInstalls()) {
            // einmalig erlauben, dass Trilumag Apps installieren darf
            waitingForPermission = true;
            bar.setIndeterminate(false); bar.setProgress(1000);
            show(R.string.upd_allow);
            buttons(a.getString(R.string.upd_allow_btn), v -> {
                try { a.startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:" + a.getPackageName()))); }
                catch (Exception e) { a.startActivity(new Intent(Settings.ACTION_SECURITY_SETTINGS)); }
            }, a.getString(R.string.cancel), v -> close());
            return;
        }
        waitingForPermission = false;
        show(R.string.upd_install);
        buttons(null, null, null, null);
        new Thread(() -> {
            try {
                PackageInstaller pkg = a.getPackageManager().getPackageInstaller();
                PackageInstaller.SessionParams p = new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
                p.setAppPackageName(a.getPackageName());
                if (Build.VERSION.SDK_INT >= 31) p.setRequireUserAction(PackageInstaller.SessionParams.USER_ACTION_NOT_REQUIRED);
                int id = pkg.createSession(p);
                try (PackageInstaller.Session s = pkg.openSession(id)) {
                    long len = apk.length(), done = 0;
                    try (InputStream in = new FileInputStream(apk); OutputStream out = s.openWrite("trilumag.apk", 0, len)) {
                        byte[] b = new byte[65536]; int n;
                        while ((n = in.read(b)) > 0) { out.write(b, 0, n); done += n; s.setStagingProgress((float) done / len); final long d = done;
                            ui.post(() -> { bar.setIndeterminate(false); bar.setProgress((int) (d * 1000 / len)); }); }
                        s.fsync(out);
                    }
                    ui.post(this::listen);
                    Intent i = new Intent(ACTION).setPackage(a.getPackageName());
                    int fl = PendingIntent.FLAG_UPDATE_CURRENT | (Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0);
                    PendingIntent pi = PendingIntent.getBroadcast(a, id, i, fl);
                    show(R.string.upd_finish);
                    s.commit(pi.getIntentSender());
                }
            } catch (Exception e) { fail(R.string.upd_err_install, e.getMessage() == null ? e.getClass().getSimpleName() : e.getMessage()); }
        }).start();
    }

    void listen() {
        if (rx != null) return;
        rx = new BroadcastReceiver() {
            @Override public void onReceive(Context c, Intent i) {
                int st = i.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE);
                if (st == PackageInstaller.STATUS_PENDING_USER_ACTION) {
                    show(R.string.upd_confirm);
                    Intent confirm = i.getParcelableExtra(Intent.EXTRA_INTENT);
                    if (confirm != null) { confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK); a.startActivity(confirm); }
                } else if (st == PackageInstaller.STATUS_SUCCESS) {
                    show(R.string.upd_done);          // Android beendet die alte App gleich selbst
                } else if (st == PackageInstaller.STATUS_FAILURE_ABORTED) {
                    fail(R.string.upd_err_aborted);
                } else {
                    String m = i.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE);
                    fail(R.string.upd_err_install, m == null ? String.valueOf(st) : m);
                }
            }
        };
        IntentFilter f = new IntentFilter(ACTION);
        if (Build.VERSION.SDK_INT >= 33) a.registerReceiver(rx, f, Context.RECEIVER_NOT_EXPORTED);
        else a.registerReceiver(rx, f);
    }

    /** zurück aus den Einstellungen: ist die Erlaubnis jetzt da, gleich weitermachen */
    void onResume() { if (active && waitingForPermission && a.getPackageManager().canRequestPackageInstalls()) install(); }
}
