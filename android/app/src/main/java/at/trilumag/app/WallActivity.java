package at.trilumag.app;

import android.app.Activity;
import android.app.DownloadManager;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.content.ContentValues;
import android.os.Build;
import android.provider.MediaStore;
import android.util.Base64;
import android.webkit.JavascriptInterface;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStream;
import android.view.Window;
import android.webkit.CookieManager;
import android.webkit.URLUtil;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Toast;

/** Zeigt die Steuerung einer Wand (http://ip/) im Vollbild. */
public class WallActivity extends Activity {
    private static final int PICK = 7;
    private WebView web;
    private String host;
    private ValueCallback<Uri[]> pick;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        Window w = getWindow();
        w.setStatusBarColor(Color.BLACK);
        w.setNavigationBarColor(Color.BLACK);
        String ip = getIntent().getStringExtra("ip");
        String name = getIntent().getStringExtra("name");
        if (name != null) setTitle(name);
        host = ip;
        web = new WebView(this);
        web.setBackgroundColor(Color.BLACK);
        setContentView(web);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setMediaPlaybackRequiresUserGesture(false);
        s.setCacheMode(WebSettings.LOAD_DEFAULT);
        web.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView v, WebResourceRequest r) {
                Uri u = r.getUrl();
                String sch = u.getScheme();
                if (("http".equals(sch) || "https".equals(sch)) && isLan(u.getHost())) return false;
                try { startActivity(new Intent(Intent.ACTION_VIEW, u)); } catch (Exception e) { }
                return true;
            }
            @Override
            public void onReceivedError(WebView v, int code, String desc, String url) {
                if (url != null && url.equals(v.getUrl()))
                    Toast.makeText(WallActivity.this, getString(R.string.unreachable), Toast.LENGTH_LONG).show();
            }
        });
        web.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onShowFileChooser(WebView v, ValueCallback<Uri[]> cb, FileChooserParams p) {
                if (pick != null) pick.onReceiveValue(null);
                pick = cb;
                try { startActivityForResult(p.createIntent(), PICK); }
                catch (Exception e) { pick = null; return false; }
                return true;
            }
        });
        web.addJavascriptInterface(new Saver(), "TrilumagApp");
        web.setDownloadListener((url, ua, cd, mime, len) -> {
            if (url.startsWith("blob:") || url.startsWith("data:")) {
                String fn = URLUtil.guessFileName(url, cd, mime);
                if (url.startsWith("blob:") && mime != null && mime.contains("png")) fn = "trilumag-wand.png";
                web.evaluateJavascript("fetch(" + q(url) + ").then(r=>r.blob()).then(b=>{const f=new FileReader();f.onload=()=>TrilumagApp.save(f.result.split(',')[1]," + q(fn) + ",b.type);f.readAsDataURL(b);})", null);
                return;
            }
            try {
                DownloadManager.Request r = new DownloadManager.Request(Uri.parse(url));
                String fn = URLUtil.guessFileName(url, cd, mime);
                r.setMimeType(mime);
                String ck = CookieManager.getInstance().getCookie(url);
                if (ck != null) r.addRequestHeader("Cookie", ck);
                r.setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED);
                r.setDestinationInExternalPublicDir(Environment.DIRECTORY_DOWNLOADS, fn);
                ((DownloadManager) getSystemService(DOWNLOAD_SERVICE)).enqueue(r);
                Toast.makeText(this, getString(R.string.downloading) + " " + fn, Toast.LENGTH_SHORT).show();
            } catch (Exception e) {
                try { startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url))); } catch (Exception e2) { }
            }
        });
        if (b != null) web.restoreState(b);
        else { String path = getIntent().getStringExtra("path"); web.loadUrl("http://" + ip + (path != null ? path : "/")); }
    }

    private static String q(String v) { return "'" + v.replace("\\", "\\\\").replace("'", "\\'") + "'"; }

    /** Speichert Dateien, die die Seite selbst erzeugt (z. B. das Wandbild als PNG), unter Downloads. */
    private class Saver {
        @JavascriptInterface
        public void save(String b64, String name, String mime) {
            try {
                byte[] d = Base64.decode(b64, Base64.DEFAULT);
                if (mime == null || mime.isEmpty()) mime = "application/octet-stream";
                if (Build.VERSION.SDK_INT >= 29) {
                    ContentValues v = new ContentValues();
                    v.put(MediaStore.Downloads.DISPLAY_NAME, name);
                    v.put(MediaStore.Downloads.MIME_TYPE, mime);
                    Uri u = getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, v);
                    try (OutputStream o = getContentResolver().openOutputStream(u)) { o.write(d); }
                } else {
                    File f = new File(getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS), name);
                    try (FileOutputStream o = new FileOutputStream(f)) { o.write(d); }
                }
                runOnUiThread(() -> Toast.makeText(WallActivity.this, getString(R.string.saved_file) + ": " + name, Toast.LENGTH_LONG).show());
            } catch (Exception e) {
                runOnUiThread(() -> Toast.makeText(WallActivity.this, e.toString(), Toast.LENGTH_LONG).show());
            }
        }
    }

    /** Adressen im Heimnetz bleiben in der App, alles andere öffnet der Browser. */
    private boolean isLan(String h) {
        if (h == null) return false;
        if (h.equals(host) || h.endsWith(".local")) return true;
        return h.startsWith("192.168.") || h.startsWith("10.") || h.matches("^172\\.(1[6-9]|2\\d|3[01])\\..*");
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        if (req == PICK && pick != null) {
            pick.onReceiveValue(WebChromeClient.FileChooserParams.parseResult(res, data));
            pick = null;
        } else super.onActivityResult(req, res, data);
    }

    @Override
    protected void onSaveInstanceState(Bundle o) { super.onSaveInstanceState(o); web.saveState(o); }

    @Override
    protected void onResume() { super.onResume(); web.onResume(); }

    @Override
    protected void onPause() { web.onPause(); super.onPause(); }

    @Override
    protected void onDestroy() { web.destroy(); super.onDestroy(); }

    @Override
    public void onBackPressed() {
        if (web.canGoBack()) web.goBack(); else super.onBackPressed();
    }
}
