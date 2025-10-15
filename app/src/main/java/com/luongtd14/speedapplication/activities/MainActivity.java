package com.luongtd14.speedapplication.activities;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import androidx.media3.common.MediaItem;
import androidx.media3.exoplayer.ExoPlayer;

import com.luongtd14.speedapplication.databinding.ActivityMainBinding;
import com.luongtd14.speedapplication.utils.PathUtils;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "LUONGTD";

    private ActivityMainBinding binding;
    private String filePath;
    private ExoPlayer exoPlayer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            requestPermissions(new String[]{Manifest.permission.READ_MEDIA_VIDEO, Manifest.permission.WRITE_EXTERNAL_STORAGE}, 1001);
        } else {
            requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE}, 1001);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                Uri uri = Uri.fromParts("package", getPackageName(), null);
                intent.setData(uri);
                startActivity(intent);
            }
        }

        binding.btnFile.setOnClickListener(v -> chooseFileFromDevice());
        binding.btnPlay.setOnClickListener(v -> playVideoWithExoPlayer());
        binding.btnEdit.setOnClickListener(v -> {
            stopPlayVideo();
            Intent intent = new Intent(this, EditActivity.class);
            intent.putExtra("filePath", filePath);
            startActivity(intent);
        });
    }

    private void playVideoWithExoPlayer() {
        exoPlayer = new ExoPlayer.Builder(this).build();
        binding.playerView.setPlayer(exoPlayer);
        Uri uri = Uri.parse(filePath);
        MediaItem mediaItem = MediaItem.fromUri(uri);
        exoPlayer.setMediaItem(mediaItem);
        exoPlayer.prepare();
        exoPlayer.play();
    }
    private void pausePlayVideo(){
        if (exoPlayer != null) {
            exoPlayer.pause();
        }
    }
    private void stopPlayVideo() {
        if (exoPlayer != null) {
            exoPlayer.stop();
            exoPlayer.release();
            exoPlayer = null;
            binding.playerView.setPlayer(null);
        }
    }

    ActivityResultLauncher<Intent> someActivityResultLauncher = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(),
            result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                    Uri uri = result.getData().getData();
                    Log.e(TAG, "uri: " + uri);
                    filePath = PathUtils.getPathFromUri(this, uri);
                    Log.e(TAG, "filePath: " + filePath);
                }
            });

    private void chooseFileFromDevice() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("video/*");
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, false);
        someActivityResultLauncher.launch(intent);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopPlayVideo();
    }
}