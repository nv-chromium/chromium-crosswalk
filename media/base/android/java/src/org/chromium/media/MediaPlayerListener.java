// Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.media.MediaPlayer;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.Log;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

// This class implements all the listener interface for android mediaplayer.
// Callbacks will be sent to the native class for processing.
@JNINamespace("media")
class MediaPlayerListener implements MediaPlayer.OnPreparedListener,
        MediaPlayer.OnCompletionListener,
        MediaPlayer.OnBufferingUpdateListener,
        MediaPlayer.OnSeekCompleteListener,
        MediaPlayer.OnVideoSizeChangedListener,
        MediaPlayer.OnInfoListener,
        MediaPlayer.OnErrorListener {
    // These values are mirrored as enums in media/base/android/media_player_bridge.h.
    // Please ensure they stay in sync.
    private static final int MEDIA_ERROR_FORMAT = 0;
    private static final int MEDIA_ERROR_DECODE = 1;
    private static final int MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK = 2;
    private static final int MEDIA_ERROR_INVALID_CODE = 3;

    // These values are copied from android media player.
    public static final int MEDIA_ERROR_MALFORMED = -1007;
    public static final int MEDIA_ERROR_TIMED_OUT = -110;

    // These are the meta data keys for seekable ranges defined in native player with same keys
    public static final int SEEKABLE_RANGE_START = 8193;
    public static final int SEEKABLE_RANGE_END   = 8194;

    // Used to determine the class instance to dispatch the native call to.
    private long mNativeMediaPlayerListener = 0;
    private final Context mContext;

    private MediaPlayerListener(long nativeMediaPlayerListener, Context context) {
        mNativeMediaPlayerListener = nativeMediaPlayerListener;
        mContext = context;
    }

    @Override
    public boolean onError(MediaPlayer mp, int what, int extra) {
        int errorType;
        switch (what) {
            case MediaPlayer.MEDIA_ERROR_UNKNOWN:
                switch (extra) {
                    case MEDIA_ERROR_MALFORMED:
                        errorType = MEDIA_ERROR_DECODE;
                        break;
                    case MEDIA_ERROR_TIMED_OUT:
                        errorType = MEDIA_ERROR_INVALID_CODE;
                        break;
                    default:
                        errorType = MEDIA_ERROR_FORMAT;
                        break;
                }
                break;
            case MediaPlayer.MEDIA_ERROR_SERVER_DIED:
                errorType = MEDIA_ERROR_DECODE;
                break;
            case MediaPlayer.MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK:
                errorType = MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK;
                break;
            default:
                // There are some undocumented error codes for android media player.
                // For example, when surfaceTexture got deleted before we setVideoSuface
                // to NULL, mediaplayer will report error -38. These errors should be ignored
                // and not be treated as an error to webkit.
                errorType = MEDIA_ERROR_INVALID_CODE;
                break;
        }
        nativeOnMediaError(mNativeMediaPlayerListener, errorType);
        return true;
    }

    @Override
    public boolean onInfo(MediaPlayer mp, int what, int extra) {
        if (what == MediaPlayer.MEDIA_INFO_METADATA_UPDATE) {
            try {
                Method getMetadata = mp.getClass().getDeclaredMethod(
                        "getMetadata", boolean.class, boolean.class);
                getMetadata.setAccessible(true);
                Object data = getMetadata.invoke(mp, false, false);
                if (data != null) {
                    Class<?> metadataClass = data.getClass();
                    Method hasMethod = metadataClass.getDeclaredMethod("has", int.class);
                    Boolean seekStartAvail = (Boolean)hasMethod.invoke(data, SEEKABLE_RANGE_START);
                    Boolean seekEndAvail = (Boolean)hasMethod.invoke(data, SEEKABLE_RANGE_END);

                    if (seekStartAvail == true && seekEndAvail == true) {
                        Method getIntMethod = metadataClass.getDeclaredMethod("getInt", int.class);
                        getIntMethod.setAccessible(true);
                        int seekableRangeStart = (Integer) getIntMethod.invoke(data, SEEKABLE_RANGE_START);
                        int seekableRangeEnd = (Integer) getIntMethod.invoke(data, SEEKABLE_RANGE_END);
                        nativeOnSeekableRangeChanged(mNativeMediaPlayerListener, seekableRangeStart, seekableRangeEnd);
                    }
                }
            } catch (NoSuchMethodException e) {
                Log.e("OnInfo", "Cannot find MediaPlayer.getMetadata() method: " + e);
            } catch (InvocationTargetException e) {
                Log.e("OnInfo", "Cannot invoke MediaPlayer.getMetadata() method: " + e);
            } catch (IllegalAccessException e) {
                Log.e("OnInfo", "Cannot access metadata: " + e);
            }
        }
        return true;
    }

    @Override
    public void onVideoSizeChanged(MediaPlayer mp, int width, int height) {
        nativeOnVideoSizeChanged(mNativeMediaPlayerListener, width, height);
    }

    @Override
    public void onSeekComplete(MediaPlayer mp) {
        nativeOnSeekComplete(mNativeMediaPlayerListener);
    }

    @Override
    public void onBufferingUpdate(MediaPlayer mp, int percent) {
        nativeOnBufferingUpdate(mNativeMediaPlayerListener, percent);
    }

    @Override
    public void onCompletion(MediaPlayer mp) {
        nativeOnPlaybackComplete(mNativeMediaPlayerListener);
    }

    @Override
    public void onPrepared(MediaPlayer mp) {
        nativeOnMediaPrepared(mNativeMediaPlayerListener);
    }

    @CalledByNative
    private static MediaPlayerListener create(long nativeMediaPlayerListener,
            Context context, MediaPlayerBridge mediaPlayerBridge) {
        final MediaPlayerListener listener =
                new MediaPlayerListener(nativeMediaPlayerListener, context);
        if (mediaPlayerBridge != null) {
            mediaPlayerBridge.setOnBufferingUpdateListener(listener);
            mediaPlayerBridge.setOnCompletionListener(listener);
            mediaPlayerBridge.setOnErrorListener(listener);
            mediaPlayerBridge.setOnPreparedListener(listener);
            mediaPlayerBridge.setOnSeekCompleteListener(listener);
            mediaPlayerBridge.setOnInfoListener(listener);
            mediaPlayerBridge.setOnVideoSizeChangedListener(listener);
        }
        return listener;
    }

    /**
     * See media/base/android/media_player_listener.cc for all the following functions.
     */
    private native void nativeOnMediaError(
            long nativeMediaPlayerListener,
            int errorType);

    private native void nativeOnSeekableRangeChanged(
            long nativeMediaPlayerListener,
            int seekableRangeStart, int seekableRangeEnd);

    private native void nativeOnVideoSizeChanged(
            long nativeMediaPlayerListener,
            int width, int height);

    private native void nativeOnBufferingUpdate(
            long nativeMediaPlayerListener,
            int percent);

    private native void nativeOnMediaPrepared(long nativeMediaPlayerListener);

    private native void nativeOnPlaybackComplete(long nativeMediaPlayerListener);

    private native void nativeOnSeekComplete(long nativeMediaPlayerListener);

    private native void nativeOnMediaInterrupted(long nativeMediaPlayerListener);
}
