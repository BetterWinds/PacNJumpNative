package com.jon.pnjnative;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.MotionEvent;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

/**
 * Bring-your-own-APK support. When the game has no data (BYO build, nothing imported yet) a tap opens the system file
 * picker; the chosen original Pac'N-Jump APK is copied to the app's private folder and handed to the native importer
 * through SDL's normal drop-file event. Opening an .apk with this app (VIEW intent) takes the same path.
 */
public class PNJActivity extends SDLActivity {
    private static final int REQ_PICK = 4711;
    private boolean picking = false;

    private boolean hasData() {
        if (new File(getFilesDir(), "pnj.pack").exists()) return true;
        try { getAssets().open("pnj.pack").close(); return true; } catch (Exception e) { /* not a web-built APK */ }
        try { getAssets().open("sysparts.png").close(); return true; } catch (Exception e) { return false; }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        handleIntent(getIntent());
        if (!hasData()) pick();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleIntent(intent);
    }

    private void handleIntent(Intent in) {
        if (in == null || !Intent.ACTION_VIEW.equals(in.getAction()) || in.getData() == null) return;
        Uri u = in.getData();
        in.setAction(Intent.ACTION_MAIN);          // handle each VIEW once
        copyAndImport(u);
    }

    private void pick() {
        if (picking) return;
        picking = true;
        Toast.makeText(this, "Pick your original Pac'N-Jump Android APK", Toast.LENGTH_LONG).show();
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        try { startActivityForResult(i, REQ_PICK); }
        catch (Exception e) { picking = false; Toast.makeText(this, "No file picker available", Toast.LENGTH_LONG).show(); }
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req != REQ_PICK) return;
        picking = false;
        if (res == Activity.RESULT_OK && data != null && data.getData() != null) copyAndImport(data.getData());
    }

    /** tap anywhere on the empty "needs data" screen to open the picker again */
    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (ev.getAction() == MotionEvent.ACTION_UP && !hasData() && !picking) pick();
        return super.dispatchTouchEvent(ev);
    }

    private void copyAndImport(final Uri uri) {
        new Thread(new Runnable() {
            public void run() {
                try {
                    File out = new File(getFilesDir(), "import.apk");
                    InputStream in = getContentResolver().openInputStream(uri);
                    if (in == null) throw new Exception("cannot open file");
                    FileOutputStream os = new FileOutputStream(out);
                    byte[] buf = new byte[1 << 16]; int n;
                    while ((n = in.read(buf)) > 0) os.write(buf, 0, n);
                    os.close(); in.close();
                    SDLActivity.onNativeDropFile(out.getAbsolutePath());
                } catch (final Exception e) {
                    runOnUiThread(new Runnable() { public void run() {
                        Toast.makeText(PNJActivity.this, "Could not read that file: " + e.getMessage(), Toast.LENGTH_LONG).show(); } });
                }
            }
        }).start();
    }
}
