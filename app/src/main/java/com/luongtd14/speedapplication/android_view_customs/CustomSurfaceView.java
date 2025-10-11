package com.luongtd14.speedapplication.android_view_customs;

import android.content.Context;
import android.util.AttributeSet;
import android.view.SurfaceView;

public class CustomSurfaceView extends SurfaceView {
    private int videoWidth = 0;
    private int videoHeight = 0;

    private static int maxWidth = -1;
    private static int maxHeight = -1;

    public CustomSurfaceView(Context context) {
        super(context);
    }

    public CustomSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setVideoSize(int width, int height) {
        if (width > 0 && height > 0) {
            videoWidth = width;
            videoHeight = height;
            requestLayout();
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = MeasureSpec.getSize(widthMeasureSpec);
        int height = MeasureSpec.getSize(heightMeasureSpec);

        if (videoWidth == 0 || videoHeight == 0) {
            setMeasuredDimension(width, height);
            return;
        }

        float aspectRatio = (float) videoWidth / videoHeight;
        int newWidth = width;
        int newHeight = (int) (width / aspectRatio);

        if (newHeight > height) {
            newHeight = height;
            newWidth = (int) (height * aspectRatio);
        }

        if (maxWidth > 0 && maxHeight > 0) {
            if (newWidth > maxWidth) {
                newWidth = maxWidth;
                newHeight = (int) (maxWidth / aspectRatio);
            }
            if (newHeight > maxHeight) {
                newHeight = maxHeight;
                newWidth = (int) (maxHeight * aspectRatio);
            }
        }

        setMeasuredDimension(newWidth, newHeight);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (maxWidth == -1 || maxHeight == -1) {
            maxWidth = getWidth();
            maxHeight = getHeight();
        }
    }
}



