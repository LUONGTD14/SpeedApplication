#include <jni.h>
#include <android/log.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaMuxer.h>
#include "sonic/sonic.h"
#include <vector>
#include <asm-generic/fcntl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "VideoSpeed", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "VideoSpeed", __VA_ARGS__)

struct Segment {
    float start;
    float end;
    float speed;
};

static inline int64_t secToUs(float s) {
    return (int64_t) (s * 1000000.0f);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_luongtd14_speedapplication_activities_EditActivity_processVideo(
        JNIEnv *env, jclass clazz,
        jstring jInput,
        jstring jOutput,
        jfloatArray jStarts,
        jfloatArray jEnds,
        jfloatArray jSpeeds) {

    const char* inputPath = env->GetStringUTFChars(jInput, nullptr);
    const char* outputPath = env->GetStringUTFChars(jOutput, nullptr);

    AMediaExtractor* extractor = nullptr;
    AMediaCodec* audioDecoder = nullptr;
    AMediaCodec* audioEncoder = nullptr;
    AMediaMuxer* muxer = nullptr;
    sonicStream sonic = nullptr;
    AMediaFormat* audioFormat = nullptr;
    AMediaFormat* videoFormat = nullptr;

    int result = -1;

    LOGI("Starting video processing with segments: %s -> %s", inputPath, outputPath);

    do {
        // Process segments data
        jsize n = env->GetArrayLength(jStarts);
        std::vector<Segment> segments(n);
        jfloat *starts = env->GetFloatArrayElements(jStarts, nullptr);
        jfloat *ends   = env->GetFloatArrayElements(jEnds, nullptr);
        jfloat *speeds = env->GetFloatArrayElements(jSpeeds, nullptr);

        for (int i = 0; i < n; i++) {
            segments[i] = {starts[i], ends[i], speeds[i]};
            LOGI("Segment %d: %.2f -> %.2f speed %.2f", i, starts[i], ends[i], speeds[i]);
        }

        env->ReleaseFloatArrayElements(jStarts, starts, 0);
        env->ReleaseFloatArrayElements(jEnds, ends, 0);
        env->ReleaseFloatArrayElements(jSpeeds, speeds, 0);

        // Create extractor
        extractor = AMediaExtractor_new();
        if (!extractor) {
            LOGE("Failed to create media extractor");
            break;
        }

        // Open input file
        int inputFd = open(inputPath, O_RDONLY);
        if (inputFd < 0) {
            LOGE("Cannot open input file: %s, error: %s", inputPath, strerror(errno));
            break;
        }

        // Get file size
        off_t fileSize = lseek(inputFd, 0, SEEK_END);
        lseek(inputFd, 0, SEEK_SET);
        media_status_t status = AMediaExtractor_setDataSourceFd(extractor, inputFd, 0, fileSize);
        if (status != AMEDIA_OK) {
            LOGE("Failed to set data source for extractor, status: %d", status);
            break;
        }

        // Find tracks
        int trackCount = AMediaExtractor_getTrackCount(extractor);
        int audioTrack = -1, videoTrack = -1;

        for (int i = 0; i < trackCount; i++) {
            AMediaFormat* fmt = AMediaExtractor_getTrackFormat(extractor, i);
            const char* mime;
            if (AMediaFormat_getString(fmt, AMEDIAFORMAT_KEY_MIME, &mime)) {
                if (strstr(mime, "audio/")) {
                    audioTrack = i;
                    audioFormat = fmt;
                    LOGI("Found audio track: %d", i);
                } else if (strstr(mime, "video/")) {
                    videoTrack = i;
                    videoFormat = fmt;
                    LOGI("Found video track: %d", i);
                } else {
                    AMediaFormat_delete(fmt);
                }
            } else {
                AMediaFormat_delete(fmt);
            }
        }

        if (videoTrack < 0) {
            LOGE("No video track found");
            break;
        }

        int fd = open(outputPath, O_CREAT | O_RDWR, 0644);
        if (fd < 0) {
            LOGE("Cannot open output file: %s", outputPath);
        }
        // Create muxer
        muxer = AMediaMuxer_new(fd, AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
        if (!muxer) {
            LOGE("Failed to create media muxer");
            break;
        }

        // Add video track
        size_t videoTrackIndex = AMediaMuxer_addTrack(muxer, videoFormat);
        LOGI("Added video track to muxer with index: %zu", videoTrackIndex);

        size_t audioTrackIndex = (size_t)-1;

        // Setup audio if available
        if (audioTrack >= 0) {
            int sampleRate, channels;
            AMediaFormat_getInt32(audioFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, &sampleRate);
            AMediaFormat_getInt32(audioFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);

            LOGI("Audio config: sampleRate=%d, channels=%d", sampleRate, channels);

            // Create encoder format
            AMediaFormat* encoderFormat = AMediaFormat_new();
            AMediaFormat_setString(encoderFormat, AMEDIAFORMAT_KEY_MIME, "audio/mp4a-latm");
            AMediaFormat_setInt32(encoderFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, sampleRate);
            AMediaFormat_setInt32(encoderFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, channels);
            AMediaFormat_setInt32(encoderFormat, AMEDIAFORMAT_KEY_BIT_RATE, 128000);
            AMediaFormat_setInt32(encoderFormat, AMEDIAFORMAT_KEY_AAC_PROFILE, 2);

            // Create audio encoder
            audioEncoder = AMediaCodec_createEncoderByType("audio/mp4a-latm");
            if (!audioEncoder) {
                LOGE("Failed to create audio encoder");
                AMediaFormat_delete(encoderFormat);
                break;
            }

            status = AMediaCodec_configure(audioEncoder, encoderFormat, nullptr, nullptr,
                                           AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
            AMediaFormat_delete(encoderFormat);

            if (status != AMEDIA_OK) {
                LOGE("Failed to configure audio encoder, status: %d", status);
                break;
            }

            status = AMediaCodec_start(audioEncoder);
            if (status != AMEDIA_OK) {
                LOGE("Failed to start audio encoder, status: %d", status);
                break;
            }

            // Get encoder output format and add to muxer
            AMediaFormat* outputFormat = AMediaCodec_getOutputFormat(audioEncoder);
            if (outputFormat) {
                audioTrackIndex = AMediaMuxer_addTrack(muxer, outputFormat);
                LOGI("Added audio track to muxer with index: %zu", audioTrackIndex);
                AMediaFormat_delete(outputFormat);
            } else {
                LOGE("Failed to get encoder output format");
                break;
            }

            // Create sonic stream
            sonic = sonicCreateStream(sampleRate, channels);
            if (!sonic) {
                LOGE("Failed to create sonic stream");
                break;
            }
            sonicSetPitch(sonic, 1.0f);
            LOGI("Sonic stream created for segment-based processing");
        }

        // Start muxer
        status = AMediaMuxer_start(muxer);
        if (status != AMEDIA_OK) {
            LOGE("Failed to start muxer, status: %d", status);
            break;
        }
        LOGI("Muxer started successfully");

        // ===== VIDEO PROCESSING WITH SEGMENTS =====
        LOGI("Processing video track with segments...");
        AMediaExtractor_selectTrack(extractor, videoTrack);

        int videoSampleCount = 0;
        bool videoEOS = false;
        int currentSegment = 0;
        int64_t outputPts = 0;

        // Process video segments
        while (!videoEOS && currentSegment < segments.size()) {
            Segment& segment = segments[currentSegment];
            LOGI("Processing video segment %d: %.2fs-%.2fs at speed %.2fx",
                 currentSegment, segment.start, segment.end, segment.speed);

            // Seek to segment start
            int64_t segmentStartUs = secToUs(segment.start);
            int64_t segmentEndUs = secToUs(segment.end);

            status = AMediaExtractor_seekTo(extractor, segmentStartUs, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
            if (status != AMEDIA_OK) {
                LOGE("Failed to seek to segment start: %lld", (long long)segmentStartUs);
                break;
            }

            bool segmentComplete = false;
            int segmentSampleCount = 0;

            while (!videoEOS && !segmentComplete && segmentSampleCount < 10000) {
                const size_t BUFFER_SIZE = 1024 * 1024;
                uint8_t* buffer = new uint8_t[BUFFER_SIZE];

                ssize_t sampleSize = AMediaExtractor_readSampleData(extractor, buffer, BUFFER_SIZE);
                if (sampleSize < 0) {
                    videoEOS = true;
                    delete[] buffer;
                    break;
                }

                int64_t pts = AMediaExtractor_getSampleTime(extractor);
                uint32_t flags = AMediaExtractor_getSampleFlags(extractor);

                // Check if we've reached the end of this segment
                if (pts >= segmentEndUs) {
                    segmentComplete = true;
                    delete[] buffer;
                    AMediaExtractor_advance(extractor); // Move to next sample for next segment
                    break;
                }

                // Calculate output PTS based on speed
                int64_t segmentRelativePts = pts - segmentStartUs;
                int64_t speedAdjustedPts = (int64_t)(segmentRelativePts / segment.speed);
                int64_t finalPts = outputPts + speedAdjustedPts;

                AMediaCodecBufferInfo info;
                info.offset = 0;
                info.size = sampleSize;
                info.presentationTimeUs = finalPts;
                info.flags = flags;

                status = AMediaMuxer_writeSampleData(muxer, videoTrackIndex, buffer, &info);
                delete[] buffer; // Delete immediately after use

                if (status != AMEDIA_OK) {
                    LOGE("Failed to write video sample, status: %d", status);
                    break;
                }

                AMediaExtractor_advance(extractor);
                videoSampleCount++;
                segmentSampleCount++;

                if (segmentSampleCount % 100 == 0) {
                    LOGI("Segment %d: processed %d samples, PTS: %lld -> %lld",
                         currentSegment, segmentSampleCount, (long long)pts, (long long)finalPts);
                }
            }

            // Update output PTS for next segment
            int64_t segmentDuration = segmentEndUs - segmentStartUs;
            outputPts += (int64_t)(segmentDuration / segment.speed);

            LOGI("Completed video segment %d: %d samples, duration: %lldus -> %lldus",
                 currentSegment, segmentSampleCount, (long long)segmentDuration,
                 (long long)(segmentDuration / segment.speed));

            currentSegment++;
        }

        LOGI("Video track completed: %d samples across %zu segments, total output duration: %lldus",
             videoSampleCount, segments.size(), (long long)outputPts);

        // If no audio track, we're done
        if (audioTrack < 0) {
            LOGI("No audio track, video-only output created");
            result = 0;
            break;
        }

        // ===== SIMPLIFIED AUDIO PROCESSING =====
        // Instead of complex segment-based audio processing, we'll use a simpler approach:
        // Process the entire audio track but apply speed changes in real-time based on video PTS

        LOGI("Processing audio track with dynamic speed adjustment...");
        AMediaExtractor_selectTrack(extractor, audioTrack);

        int sampleRate, channels;
        AMediaFormat_getInt32(audioFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, &sampleRate);
        AMediaFormat_getInt32(audioFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);

        const char* audioMime;
        AMediaFormat_getString(audioFormat, AMEDIAFORMAT_KEY_MIME, &audioMime);

        // Create audio decoder
        audioDecoder = AMediaCodec_createDecoderByType(audioMime);
        if (!audioDecoder) {
            LOGE("Failed to create audio decoder");
            break;
        }

        status = AMediaCodec_configure(audioDecoder, audioFormat, nullptr, nullptr, 0);
        if (status != AMEDIA_OK) {
            LOGE("Failed to configure audio decoder, status: %d", status);
            break;
        }

        status = AMediaCodec_start(audioDecoder);
        if (status != AMEDIA_OK) {
            LOGE("Failed to start audio decoder, status: %d", status);
            break;
        }

        // Audio processing state
        bool audioEOS = false;
        bool decoderEOS = false;
        int64_t currentPts = 0;
        int audioSampleCount = 0;
        int currentAudioSegment = 0;

        const int PCM_BUFFER_SIZE = 4096;
        std::vector<short> pcmBuffer(PCM_BUFFER_SIZE * channels);
        std::vector<short> processedBuffer(PCM_BUFFER_SIZE * channels * 4);

        // Process entire audio track, adjusting speed based on current time
        while (!audioEOS) {
            // Feed data to decoder
            if (!decoderEOS) {
                ssize_t inIndex = AMediaCodec_dequeueInputBuffer(audioDecoder, 10000);
                if (inIndex >= 0) {
                    size_t bufSize;
                    uint8_t* buf = AMediaCodec_getInputBuffer(audioDecoder, inIndex, &bufSize);
                    if (buf) {
                        ssize_t sampleSize = AMediaExtractor_readSampleData(extractor, buf, bufSize);
                        if (sampleSize < 0) {
                            decoderEOS = true;
                            AMediaCodec_queueInputBuffer(audioDecoder, inIndex, 0, 0, 0,
                                                         AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                            LOGI("Sent EOS to audio decoder");
                        } else {
                            int64_t pts = AMediaExtractor_getSampleTime(extractor);
                            AMediaCodec_queueInputBuffer(audioDecoder, inIndex, 0, sampleSize, pts, 0);
                            AMediaExtractor_advance(extractor);
                        }
                    }
                }
            }

            // Process decoder output
            AMediaCodecBufferInfo decInfo;
            ssize_t outIndex = AMediaCodec_dequeueOutputBuffer(audioDecoder, &decInfo, 10000);

            if (outIndex >= 0) {
                if (decInfo.size > 0) {
                    size_t outSize;
                    uint8_t* outBuf = AMediaCodec_getOutputBuffer(audioDecoder, outIndex, &outSize);
                    if (outBuf) {
                        // Determine current speed based on audio PTS
                        float currentSpeed = 1.0f;
                        for (int i = 0; i < segments.size(); i++) {
                            int64_t segmentStartUs = secToUs(segments[i].start);
                            int64_t segmentEndUs = secToUs(segments[i].end);
                            if (decInfo.presentationTimeUs >= segmentStartUs && decInfo.presentationTimeUs < segmentEndUs) {
                                currentSpeed = segments[i].speed;
                                if (i != currentAudioSegment) {
                                    currentAudioSegment = i;
                                    LOGI("Audio switched to segment %d: speed %.2fx at PTS %lld",
                                         i, currentSpeed, (long long)decInfo.presentationTimeUs);
                                }
                                break;
                            }
                        }

                        // Set sonic speed
                        sonicSetSpeed(sonic, currentSpeed);

                        // Process with sonic
                        int sampleCount = decInfo.size / (channels * sizeof(short));
                        sonicWriteShortToStream(sonic, reinterpret_cast<short*>(outBuf), sampleCount);

                        // Read processed data
                        int totalProcessed = 0;
                        while (true) {
                            int read = sonicReadShortFromStream(sonic,
                                                                processedBuffer.data() + totalProcessed,
                                                                (processedBuffer.size() - totalProcessed) / channels);
                            if (read <= 0) break;
                            totalProcessed += read * channels;
                        }

                        // Send to encoder
                        if (totalProcessed > 0) {
                            ssize_t encInIndex = AMediaCodec_dequeueInputBuffer(audioEncoder, 10000);
                            if (encInIndex >= 0) {
                                size_t encSize;
                                uint8_t* encBuf = AMediaCodec_getInputBuffer(audioEncoder, encInIndex, &encSize);
                                if (encBuf) {
                                    size_t bytesToCopy = totalProcessed * sizeof(short);
                                    if (bytesToCopy > encSize) {
                                        bytesToCopy = encSize;
                                    }
                                    memcpy(encBuf, processedBuffer.data(), bytesToCopy);

                                    int samplesCopied = bytesToCopy / (channels * sizeof(short));
                                    AMediaCodec_queueInputBuffer(audioEncoder, encInIndex, 0,
                                                                 bytesToCopy, currentPts, 0);
                                    currentPts += (int64_t)((1000000LL * samplesCopied) / sampleRate);
                                }
                            }
                        }
                    }
                }

                AMediaCodec_releaseOutputBuffer(audioDecoder, outIndex, false);

                if (decInfo.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
                    LOGI("Audio decoder output EOS");
                    // Flush sonic stream
                    sonicFlushStream(sonic);
                    int totalProcessed = 0;
                    while (true) {
                        int read = sonicReadShortFromStream(sonic,
                                                            processedBuffer.data() + totalProcessed,
                                                            (processedBuffer.size() - totalProcessed) / channels);
                        if (read <= 0) break;
                        totalProcessed += read * channels;
                    }

                    // Send remaining processed data
                    if (totalProcessed > 0) {
                        ssize_t encInIndex = AMediaCodec_dequeueInputBuffer(audioEncoder, 10000);
                        if (encInIndex >= 0) {
                            size_t encSize;
                            uint8_t* encBuf = AMediaCodec_getInputBuffer(audioEncoder, encInIndex, &encSize);
                            if (encBuf) {
                                size_t bytesToCopy = std::min(totalProcessed * sizeof(short), encSize);
                                memcpy(encBuf, processedBuffer.data(), bytesToCopy);
                                AMediaCodec_queueInputBuffer(audioEncoder, encInIndex, 0,
                                                             bytesToCopy, currentPts, 0);
                            }
                        }
                    }

                    // Send EOS to encoder
                    ssize_t encInIndex = AMediaCodec_dequeueInputBuffer(audioEncoder, 10000);
                    if (encInIndex >= 0) {
                        AMediaCodec_queueInputBuffer(audioEncoder, encInIndex, 0, 0, currentPts,
                                                     AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                        LOGI("Sent EOS to audio encoder");
                    }
                }
            }

            // Process encoder output
            AMediaCodecBufferInfo encInfo;
            ssize_t encOutIndex = AMediaCodec_dequeueOutputBuffer(audioEncoder, &encInfo, 0);
            while (encOutIndex >= 0) {
                size_t encOutSize;
                uint8_t* encBuf = AMediaCodec_getOutputBuffer(audioEncoder, encOutIndex, &encOutSize);

                if (encBuf && (encInfo.size > 0)) {
                    AMediaMuxer_writeSampleData(muxer, audioTrackIndex, encBuf, &encInfo);
                    audioSampleCount++;
                }

                AMediaCodec_releaseOutputBuffer(audioEncoder, encOutIndex, false);

                if (encInfo.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
                    LOGI("Audio encoder output EOS");
                    audioEOS = true;
                }

                encOutIndex = AMediaCodec_dequeueOutputBuffer(audioEncoder, &encInfo, 0);
            }

            if (audioSampleCount % 100 == 0 && audioSampleCount > 0) {
                LOGI("Processed %d audio samples, current segment: %d", audioSampleCount, currentAudioSegment);
            }
        }

        LOGI("Audio track completed: %d samples", audioSampleCount);
        result = 0;

    } while (false);

    // Cleanup
    LOGI("Starting cleanup...");

    if (sonic) {
        sonicDestroyStream(sonic);
        sonic = nullptr;
    }

    if (muxer) {
        AMediaMuxer_stop(muxer);
        AMediaMuxer_delete(muxer);
        muxer = nullptr;
    }

    if (audioEncoder) {
        AMediaCodec_stop(audioEncoder);
        AMediaCodec_delete(audioEncoder);
        audioEncoder = nullptr;
    }

    if (audioDecoder) {
        AMediaCodec_stop(audioDecoder);
        AMediaCodec_delete(audioDecoder);
        audioDecoder = nullptr;
    }

    if (extractor) {
        AMediaExtractor_delete(extractor);
        extractor = nullptr;
    }

    if (audioFormat) {
        AMediaFormat_delete(audioFormat);
        audioFormat = nullptr;
    }

    if (videoFormat) {
        AMediaFormat_delete(videoFormat);
        videoFormat = nullptr;
    }

    env->ReleaseStringUTFChars(jInput, inputPath);
    env->ReleaseStringUTFChars(jOutput, outputPath);

    if (result == 0) {
        LOGI("Successfully processed video with segment-based speed adjustment");
    } else {
        LOGE("Video processing failed");
    }

    return result;
}