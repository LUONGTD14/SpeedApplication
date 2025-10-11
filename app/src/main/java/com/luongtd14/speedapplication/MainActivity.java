package com.luongtd14.speedapplication;

import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

import com.luongtd14.speedapplication.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'speedapplication' library on application startup.
    static {
        System.loadLibrary("speedapplication");
    }

    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

    }

}