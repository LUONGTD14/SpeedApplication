package com.luongtd14.speedapplication.activities;

import android.content.Intent;
import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.media.MediaMetadataRetriever;
import android.os.Bundle;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import com.luongtd14.speedapplication.R;
import com.luongtd14.speedapplication.databinding.ActivityEditBinding;

public class EditActivity extends AppCompatActivity {


    static {
        System.loadLibrary("video_speed_editor");
    }
    ActivityEditBinding binding;
    String filePath;
    int width, height;
    float[] starts = {0f, 5f, 7f, 15f};
    float[] ends   = {5f, 7f, 15f, 999f};
    float[] speeds = {1f, 0.5f, 1.5f, 1f};

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);

        binding = ActivityEditBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        Intent intent = getIntent();
        filePath = intent.getStringExtra("filePath");
        if (filePath != null) {
            retrieverInfo();
            binding.surfaceView.setVideoSize(width, height);
            binding.btnTrans.setOnClickListener(v -> {
                int result = processVideo(
                        filePath    ,
                        "/sdcard/output_speed.mp4",
                        starts, ends, speeds
                );

            });
        }
    }
    private void retrieverInfo(){
        try {
            MediaMetadataRetriever retriever = new MediaMetadataRetriever();
            retriever.setDataSource(filePath);
            width = Integer.parseInt(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH));
            height = Integer.parseInt(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT));
            int rotation = Integer.parseInt(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_ROTATION));
            if (rotation == 90 || rotation == 270) {
                int temp = width;
                width = height;
                height = temp;
            }
            retriever.release();
        }catch (Exception e){
            e.printStackTrace();
        }
    }

    public static native int processVideo(
            String inputPath,
            String outputPath,
            float[] segmentStart,
            float[] segmentEnd,
            float[] speed
    );
}