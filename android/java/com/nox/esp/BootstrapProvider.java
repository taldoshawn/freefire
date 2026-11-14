// Java 8 / BootstrapProvider.java / Android 26+ / optional diagnostics
// Always fail open: a diagnostic overlay must never prevent the host game from starting.
package com.nox.esp;

import android.app.Activity;
import android.app.Application;
import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.View;

public final class BootstrapProvider extends ContentProvider {
    private static final String TAG = "NOX-RUNTIME";
    private static final long STARTUP_GRACE_MS = 12000L;
    private static volatile boolean disabled;

    @Override
    public boolean onCreate() {
        Context context = getContext();
        if (context == null) return true;
        Context appContext = context.getApplicationContext();
        if (!(appContext instanceof Application)) {
            Log.w(TAG, "No Application context: diagnostics skipped");
            return true;
        }
        try {
            ((Application) appContext).registerActivityLifecycleCallbacks(
                new Application.ActivityLifecycleCallbacks() {
                    @Override public void onActivityCreated(Activity a, Bundle state) {}
                    @Override public void onActivityStarted(Activity a) {}
                    @Override public void onActivityResumed(Activity a) {
                        if (disabled || a == null || a.isFinishing()) return;
                        if (!"com.dts.freefireth.FFMainActivity".equals(
                                a.getClass().getName())) return;
                        // Do not load JNI during startup or while the Unity host is booting.
                        try {
                            View decor = a.getWindow().getDecorView();
                            decor.postDelayed(() -> {
                                if (disabled || a.isFinishing() || a.isDestroyed()) return;
                                try {
                                    EspOverlay.attach(a);
                                } catch (LinkageError | RuntimeException failure) {
                                    disabled = true;
                                    Log.e(TAG, "Overlay disabled to protect host startup",
                                          failure);
                                }
                            }, STARTUP_GRACE_MS);
                        } catch (RuntimeException failure) {
                            disabled = true;
                            Log.e(TAG, "Overlay scheduling disabled", failure);
                        }
                    }
                    @Override public void onActivityPaused(Activity a) {}
                    @Override public void onActivityStopped(Activity a) {}
                    @Override public void onActivitySaveInstanceState(Activity a, Bundle state) {}
                    @Override public void onActivityDestroyed(Activity a) {}
                });
        } catch (RuntimeException failure) {
            disabled = true;
            Log.e(TAG, "Cannot register diagnostics lifecycle", failure);
        }
        return true;
    }

    @Override public Cursor query(Uri uri, String[] projection, String selection,
                                  String[] args, String sortOrder) { return null; }
    @Override public String getType(Uri uri) { return null; }
    @Override public Uri insert(Uri uri, ContentValues values) { return null; }
    @Override public int delete(Uri uri, String selection, String[] args) { return 0; }
    @Override public int update(Uri uri, ContentValues values, String selection,
                                String[] args) { return 0; }
}
