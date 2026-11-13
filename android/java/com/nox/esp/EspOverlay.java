// Java 8 / EspOverlay.java / Android 26+ ARM64 / Unity column-major P*V, inverted Y
package com.nox.esp;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.view.View;
import android.view.ViewGroup;

public final class EspOverlay extends View {
    private static final int JOINTS = 16;
    private static final int STRIDE = 10 + JOINTS * 3;
    private static final int[] BONES = {
        0,1, 1,2, 2,3, 1,4, 4,5, 5,6,
        1,7, 7,8, 8,9, 3,10, 10,11, 11,12,
        3,13, 13,14, 14,15
    };
    private static final int ENEMY_COLOR = Color.rgb(255, 80, 80);
    private static volatile Frame latest;
    private static volatile EspOverlay active;

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final float[] head = new float[2];
    private final float[] feet = new float[2];
    private final float[] joints = new float[JOINTS * 2];
    private final boolean[] valid_joint = new boolean[JOINTS];
    private final float density;

    private static final class Frame {
        final float[] matrix;
        final float[] actors;
        final String[] names;
        final float[] camera;
        final int localTeam;

        Frame(float[] m, float[] a, String[] n, float[] c, int t) {
            matrix = m.clone();
            actors = a.clone();
            names = n.clone();
            camera = c.clone();
            localTeam = t;
        }
    }

    static {
        System.loadLibrary("nox_esp");
        nativeInit();
    }

    private static native void nativeInit();

    public EspOverlay(Activity activity) {
        super(activity);
        density = getResources().getDisplayMetrics().density;
        paint.setTypeface(Typeface.create("sans-serif", Typeface.BOLD));
        setClickable(false);
        setFocusable(false);
        setWillNotDraw(false);
    }

    public static void attach(Activity activity) {
        activity.runOnUiThread(() -> {
            ViewGroup root = activity.findViewById(android.R.id.content);
            if (root == null) return;
            for (int i = 0; i < root.getChildCount(); i++) {
                if (root.getChildAt(i) instanceof EspOverlay) {
                    active = (EspOverlay) root.getChildAt(i);
                    return;
                }
            }
            EspOverlay overlay = new EspOverlay(activity);
            root.addView(overlay, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
            active = overlay;
            overlay.invalidate();
        });
    }

    // Called from the native bridge. No fabricated entities are supplied.
    private static void publishFrame(float[] matrix, float[] actors,
                                     String[] names, float[] camera,
                                     int localTeam) {
        if (matrix == null || matrix.length != 16 ||
            actors == null || actors.length % STRIDE != 0 ||
            names == null || names.length != actors.length / STRIDE ||
            camera == null || camera.length != 3) return;
        latest = new Frame(matrix, actors, names, camera, localTeam);
        EspOverlay view = active;
        if (view != null) view.postInvalidateOnAnimation();
    }

    private boolean project(float[] m, float x, float y, float z,
                            float[] out, int offset) {
        float w = m[3]*x + m[7]*y + m[11]*z + m[15];
        if (!Float.isFinite(w) || w <= 0.001f) return false;
        float px = (m[0]*x + m[4]*y + m[8]*z + m[12]) / w;
        float py = (m[1]*x + m[5]*y + m[9]*z + m[13]) / w;
        if (!Float.isFinite(px) || !Float.isFinite(py) ||
            Math.abs(px) > 8 || Math.abs(py) > 8) return false;
        out[offset] = (px + 1f) * getWidth() * .5f;
        out[offset + 1] = (1f - py) * getHeight() * .5f;
        return true;
    }

    private void line(Canvas canvas, float x1, float y1, float x2, float y2,
                      int color, float width) {
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(width * density);
        paint.setColor(color);
        canvas.drawLine(x1, y1, x2, y2, paint);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        Frame frame = latest;
        if (frame == null) {
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(Color.WHITE);
            paint.setTextSize(11 * density);
            canvas.drawText("NOX / aguardando dados de entidades", 10*density,
                            22*density, paint);
            return;
        }
        float[] data = frame.actors;
        for (int i = 0; i < data.length / STRIDE; ++i) {
            int b = i * STRIDE;
            if (data[b+1] <= 0 || (int)data[b] == frame.localTeam) continue;
            if (!project(frame.matrix, data[b+7], data[b+8], data[b+9], head, 0) ||
                !project(frame.matrix, data[b+4], data[b+5], data[b+6], feet, 0))
                continue;

            float height = Math.abs(feet[1] - head[1]);
            if (height < 6 || height > getHeight() * 3) continue;
            float width = height * .48f;
            float centerX = (head[0] + feet[0]) * .5f;
            float left = centerX - width * .5f;
            float right = centerX + width * .5f;
            float top = Math.min(head[1], feet[1]);
            float bottom = Math.max(head[1], feet[1]);

            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(2 * density);
            paint.setColor(Color.BLACK);
            canvas.drawRect(left-1, top-1, right+1, bottom+1, paint);
            paint.setColor(ENEMY_COLOR);
            canvas.drawRect(left, top, right, bottom, paint);
            line(canvas, getWidth()*.5f, 0, centerX, top, ENEMY_COLOR, 1);

            float hp = data[b+2], maxHp = data[b+3];
            if (Float.isFinite(hp) && Float.isFinite(maxHp) && maxHp > 0) {
                float ratio = Math.max(0, Math.min(1, hp / maxHp));
                float barLeft = left - 8 * density;
                float barRight = barLeft + 4 * density;
                paint.setStyle(Paint.Style.FILL);
                paint.setColor(Color.BLACK);
                canvas.drawRect(barLeft-1, top-1, barRight+1, bottom+1, paint);
                paint.setColor(ratio > .6f ? Color.GREEN :
                               ratio > .3f ? Color.YELLOW : Color.RED);
                canvas.drawRect(barLeft, bottom-(bottom-top)*ratio,
                                barRight, bottom, paint);
            }

            for (int j = 0; j < JOINTS; ++j) {
                int p = b + 10 + j*3;
                valid_joint[j] = project(frame.matrix, data[p], data[p+1],
                                         data[p+2], joints, j*2);
            }
            for (int k = 0; k < BONES.length; k += 2) {
                int a = BONES[k], z = BONES[k+1];
                if (valid_joint[a] && valid_joint[z]) {
                    line(canvas, joints[a*2], joints[a*2+1],
                         joints[z*2], joints[z*2+1], Color.WHITE, 1.5f);
                }
            }

            float dx = data[b+4] - frame.camera[0];
            float dy = data[b+5] - frame.camera[1];
            float dz = data[b+6] - frame.camera[2];
            float distance = (float)Math.sqrt(dx*dx + dy*dy + dz*dz);
            if (Float.isFinite(distance)) {
                paint.setStyle(Paint.Style.FILL);
                paint.setColor(Color.WHITE);
                paint.setTextSize(11*density);
                String name = frame.names[i] == null ? "Player" : frame.names[i];
                canvas.drawText(name + " [" + Math.round(distance) + "m]",
                                left, top-5*density, paint);
            }
        }
    }
}
