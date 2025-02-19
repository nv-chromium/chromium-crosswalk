// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_player.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "media/base/android/media_codec_audio_decoder.h"
#include "media/base/android/media_codec_video_decoder.h"
#include "media/base/android/media_player_manager.h"
#include "media/base/buffers.h"

#define RUN_ON_MEDIA_THREAD(METHOD, ...)                                     \
  do {                                                                       \
    if (!GetMediaTaskRunner()->BelongsToCurrentThread()) {                   \
      DCHECK(ui_task_runner_->BelongsToCurrentThread());                     \
      GetMediaTaskRunner()->PostTask(                                        \
          FROM_HERE, base::Bind(&MediaCodecPlayer::METHOD, media_weak_this_, \
                                ##__VA_ARGS__));                             \
      return;                                                                \
    }                                                                        \
  } while (0)

namespace media {

class MediaThread : public base::Thread {
 public:
  MediaThread() : base::Thread("BrowserMediaThread") {
    Start();
  }
};

// Create media thread
base::LazyInstance<MediaThread>::Leaky
    g_media_thread = LAZY_INSTANCE_INITIALIZER;


scoped_refptr<base::SingleThreadTaskRunner> GetMediaTaskRunner() {
  return g_media_thread.Pointer()->task_runner();
}

// MediaCodecPlayer implementation.

MediaCodecPlayer::MediaCodecPlayer(
    int player_id,
    base::WeakPtr<MediaPlayerManager> manager,
    const RequestMediaResourcesCB& request_media_resources_cb,
    scoped_ptr<DemuxerAndroid> demuxer,
    const GURL& frame_url)
    : MediaPlayerAndroid(player_id,
                         manager.get(),
                         request_media_resources_cb,
                         frame_url),
      ui_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      demuxer_(demuxer.Pass()),
      state_(kStatePaused),
      interpolator_(&default_tick_clock_),
      pending_start_(false),
      pending_seek_(kNoTimestamp()),
      media_weak_factory_(this) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << "MediaCodecPlayer::MediaCodecPlayer: player_id:" << player_id;

  request_resources_cb_ = base::Bind(request_media_resources_cb_, player_id);

  completion_cb_ =
      base::Bind(&MediaPlayerManager::OnPlaybackComplete, manager, player_id);
  seek_done_cb_ =
      base::Bind(&MediaPlayerManager::OnSeekComplete, manager, player_id);
  error_cb_ = base::Bind(&MediaPlayerManager::OnError, manager, player_id);
  attach_listener_cb_ = base::Bind(&MediaPlayerAndroid::AttachListener,
                                   WeakPtrForUIThread(), nullptr);
  detach_listener_cb_ =
      base::Bind(&MediaPlayerAndroid::DetachListener, WeakPtrForUIThread());
  metadata_changed_cb_ = base::Bind(&MediaPlayerAndroid::OnMediaMetadataChanged,
                                    WeakPtrForUIThread());
  time_update_cb_ =
      base::Bind(&MediaPlayerAndroid::OnTimeUpdate, WeakPtrForUIThread());

  media_weak_this_ = media_weak_factory_.GetWeakPtr();

  // Finish initializaton on Media thread
  GetMediaTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&MediaCodecPlayer::Initialize, media_weak_this_));
}

MediaCodecPlayer::~MediaCodecPlayer()
{
  DVLOG(1) << "MediaCodecPlayer::~MediaCodecPlayer";
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
}

void MediaCodecPlayer::Initialize() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  interpolator_.SetUpperBound(base::TimeDelta());

  CreateDecoders();

  // This call might in turn call MediaCodecPlayer::OnDemuxerConfigsAvailable()
  // which propagates configs into decoders. Therefore CreateDecoders() should
  // be called first.
  demuxer_->Initialize(this);
}

// The implementation of MediaPlayerAndroid interface.

void MediaCodecPlayer::DeleteOnCorrectThread() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  DetachListener();

  // The base class part that deals with MediaPlayerListener
  // has to be destroyed on UI thread.
  DestroyListenerOnUIThread();

  // Post deletion onto Media thread
  GetMediaTaskRunner()->DeleteSoon(FROM_HERE, this);
}

void MediaCodecPlayer::SetVideoSurface(gfx::ScopedJavaSurface surface) {
  RUN_ON_MEDIA_THREAD(SetVideoSurface, base::Passed(&surface));

  DVLOG(1) << __FUNCTION__ << (surface.IsEmpty() ? " empty" : " non-empty");

  // Save the empty-ness before we pass the surface to the decoder.
  bool surface_is_empty = surface.IsEmpty();

  // Apparently RemoveVideoSurface() can be called several times in a row,
  // ignore the second and subsequent calls.
  if (surface_is_empty && !video_decoder_->HasVideoSurface()) {
    DVLOG(1) << __FUNCTION__ << ": surface already removed, ignoring";
    return;
  }

  video_decoder_->SetVideoSurface(surface.Pass());

  if (surface_is_empty) {
    // Remove video surface.
    switch (state_) {
      case kStatePlaying:
        if (VideoFinished())
          break;

        DVLOG(1) << __FUNCTION__ << ": stopping and restarting";
        // Stop decoders as quickly as possible.
        StopDecoders();  // synchronous stop

        // Prefetch or wait for initial configuration.
        if (HasAudio() || HasVideo()) {
          SetState(kStatePrefetching);
          StartPrefetchDecoders();
        } else {
          SetState(kStateWaitingForConfig);
        }
        break;

      default:
        break;  // ignore
    }
  } else {
    // Replace video surface.
    switch (state_) {
      case kStateWaitingForSurface:
        SetState(kStatePlaying);
        StartPlaybackOrBrowserSeek();
        break;

      case kStatePlaying:
        if (VideoFinished())
          break;

        DVLOG(1) << __FUNCTION__ << ": requesting to stop and restart";
        SetState(kStateStopping);
        RequestToStopDecoders();
        SetPendingStart(true);
        break;

      default:
        break;  // ignore
    }
  }
}

void MediaCodecPlayer::Start() {
  RUN_ON_MEDIA_THREAD(Start);

  DVLOG(1) << __FUNCTION__;

  switch (state_) {
    case kStatePaused:
      // Prefetch or wait for initial configuration.
      if (HasAudio() || HasVideo()) {
        SetState(kStatePrefetching);
        StartPrefetchDecoders();
      } else {
        SetState(kStateWaitingForConfig);
      }
      break;
    case kStateStopping:
    case kStateWaitingForSeek:
      SetPendingStart(true);
      break;
    case kStateWaitingForConfig:
    case kStatePrefetching:
    case kStatePlaying:
    case kStateWaitingForSurface:
    case kStateError:
      break;  // Ignore
    default:
      NOTREACHED();
      break;
  }
}

void MediaCodecPlayer::Pause(bool is_media_related_action) {
  RUN_ON_MEDIA_THREAD(Pause, is_media_related_action);

  DVLOG(1) << __FUNCTION__;

  SetPendingStart(false);

  switch (state_) {
    case kStateWaitingForConfig:
    case kStatePrefetching:
    case kStateWaitingForSurface:
      SetState(kStatePaused);
      StopDecoders();
      break;
    case kStatePlaying:
      SetState(kStateStopping);
      RequestToStopDecoders();
      break;
    case kStatePaused:
    case kStateStopping:
    case kStateWaitingForSeek:
    case kStateError:
      break;  // Ignore
    default:
      NOTREACHED();
      break;
  }
}

void MediaCodecPlayer::SeekTo(base::TimeDelta timestamp) {
  RUN_ON_MEDIA_THREAD(SeekTo, timestamp);

  DVLOG(1) << __FUNCTION__ << " " << timestamp;

  switch (state_) {
    case kStatePaused:
      SetState(kStateWaitingForSeek);
      RequestDemuxerSeek(timestamp);
      break;
    case kStateWaitingForConfig:
    case kStatePrefetching:
    case kStateWaitingForSurface:
      SetState(kStateWaitingForSeek);
      StopDecoders();
      SetPendingStart(true);
      RequestDemuxerSeek(timestamp);
      break;
    case kStatePlaying:
      SetState(kStateStopping);
      RequestToStopDecoders();
      SetPendingStart(true);
      SetPendingSeek(timestamp);
      break;
    case kStateStopping:
      SetPendingSeek(timestamp);
      break;
    case kStateWaitingForSeek:
      SetPendingSeek(timestamp);
      break;
    case kStateError:
      break;  // ignore
    default:
      NOTREACHED();
      break;
  }
}

void MediaCodecPlayer::Release() {
  RUN_ON_MEDIA_THREAD(Release);

  DVLOG(1) << __FUNCTION__;

  // Stop decoding threads and delete MediaCodecs, but keep IPC between browser
  // and renderer processes going. Seek should work across and after Release().

  ReleaseDecoderResources();

  SetPendingStart(false);

  if (state_ != kStateWaitingForSeek)
    SetState(kStatePaused);

  base::TimeDelta pending_seek_time = GetPendingSeek();
  if (pending_seek_time != kNoTimestamp()) {
    SetPendingSeek(kNoTimestamp());
    SetState(kStateWaitingForSeek);
    RequestDemuxerSeek(pending_seek_time);
  }
}

void MediaCodecPlayer::SetVolume(double volume) {
  RUN_ON_MEDIA_THREAD(SetVolume, volume);

  DVLOG(1) << __FUNCTION__ << " " << volume;
  audio_decoder_->SetVolume(volume);
}

int MediaCodecPlayer::GetVideoWidth() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  return metadata_cache_.video_size.width();
}

int MediaCodecPlayer::GetVideoHeight() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  return metadata_cache_.video_size.height();
}

base::TimeDelta MediaCodecPlayer::GetCurrentTime() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  return current_time_cache_;
}

base::TimeDelta MediaCodecPlayer::GetDuration() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  return metadata_cache_.duration;
}

bool MediaCodecPlayer::IsPlaying() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // TODO(timav): Use another variable since |state_| should only be accessed on
  // Media thread.
  return state_ == kStatePlaying || state_ == kStateStopping;
}

bool MediaCodecPlayer::CanPause() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return false;
}

bool MediaCodecPlayer::CanSeekForward() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return false;
}

bool MediaCodecPlayer::CanSeekBackward() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return false;
}

bool MediaCodecPlayer::IsPlayerReady() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  // This method is called to check whether it's safe to release the player when
  // the OS needs more resources. This class can be released at any time.
  return true;
}

void MediaCodecPlayer::SetCdm(BrowserCdm* cdm) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

// Callbacks from Demuxer.

void MediaCodecPlayer::OnDemuxerConfigsAvailable(
    const DemuxerConfigs& configs) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  DVLOG(1) << __FUNCTION__;

  duration_ = configs.duration;

  SetDemuxerConfigs(configs);

  // Update cache and notify manager on UI thread
  gfx::Size video_size = HasVideo() ? configs.video_size : gfx::Size();
  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(metadata_changed_cb_, duration_, video_size));
}

void MediaCodecPlayer::OnDemuxerDataAvailable(const DemuxerData& data) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  DCHECK_LT(0u, data.access_units.size());
  CHECK_GE(1u, data.demuxer_configs.size());

  DVLOG(2) << "Player::" << __FUNCTION__;

  if (data.type == DemuxerStream::AUDIO)
    audio_decoder_->OnDemuxerDataAvailable(data);

  if (data.type == DemuxerStream::VIDEO)
    video_decoder_->OnDemuxerDataAvailable(data);
}

void MediaCodecPlayer::OnDemuxerSeekDone(
    base::TimeDelta actual_browser_seek_time) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  DVLOG(1) << __FUNCTION__ << " actual_time:" << actual_browser_seek_time;

  DCHECK(seek_info_.get());
  DCHECK(seek_info_->seek_time != kNoTimestamp());

  // A browser seek must not jump into the past. Ideally, it seeks to the
  // requested time, but it might jump into the future.
  DCHECK(!seek_info_->is_browser_seek ||
         seek_info_->seek_time <= actual_browser_seek_time);

  // Restrict the current time to be equal to seek_time
  // for the next StartPlaybackDecoders() call.

  base::TimeDelta seek_time = seek_info_->is_browser_seek
                                  ? actual_browser_seek_time
                                  : seek_info_->seek_time;

  interpolator_.SetBounds(seek_time, seek_time);
  audio_decoder_->SetBaseTimestamp(seek_time);

  // The Flush() might set the state to kStateError.
  if (state_ == kStateError) {
    // Notify the Renderer.
    if (!seek_info_->is_browser_seek)
      ui_task_runner_->PostTask(FROM_HERE,
                                base::Bind(seek_done_cb_, seek_time));

    seek_info_.reset();
    return;
  }

  DCHECK_EQ(kStateWaitingForSeek, state_);

  base::TimeDelta pending_seek_time = GetPendingSeek();
  if (pending_seek_time != kNoTimestamp()) {
    // Keep kStateWaitingForSeek
    SetPendingSeek(kNoTimestamp());
    RequestDemuxerSeek(pending_seek_time);
    return;
  }

  if (HasPendingStart()) {
    SetPendingStart(false);
    // Prefetch or wait for initial configuration.
    if (HasAudio() || HasVideo()) {
      SetState(kStatePrefetching);
      StartPrefetchDecoders();
    } else {
      SetState(kStateWaitingForConfig);
    }
  } else {
    SetState(kStatePaused);
  }

  // Notify the Renderer.
  if (!seek_info_->is_browser_seek)
    ui_task_runner_->PostTask(FROM_HERE, base::Bind(seek_done_cb_, seek_time));

  seek_info_.reset();
}

void MediaCodecPlayer::OnDemuxerDurationChanged(
    base::TimeDelta duration) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " duration:" << duration;

  duration_ = duration;
}

// Events from Player, called on UI thread

void MediaCodecPlayer::OnMediaMetadataChanged(base::TimeDelta duration,
                                              const gfx::Size& video_size) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (duration != kNoTimestamp())
    metadata_cache_.duration = duration;

  if (!video_size.IsEmpty())
    metadata_cache_.video_size = video_size;

  manager()->OnMediaMetadataChanged(player_id(), metadata_cache_.duration,
                                    metadata_cache_.video_size.width(),
                                    metadata_cache_.video_size.height(), true);
}

void MediaCodecPlayer::OnTimeUpdate(base::TimeDelta current_timestamp,
                                    base::TimeTicks current_time_ticks) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  current_time_cache_ = current_timestamp;
  manager()->OnTimeUpdate(player_id(), current_timestamp, current_time_ticks);
}

// Events from Decoders, called on Media thread

void MediaCodecPlayer::RequestDemuxerData(DemuxerStream::Type stream_type) {
  DVLOG(2) << __FUNCTION__ << " streamType:" << stream_type;

  // Use this method instead of directly binding with
  // DemuxerAndroid::RequestDemuxerData() to avoid the race condition on
  // deletion:
  // 1. DeleteSoon is posted from UI to Media thread.
  // 2. RequestDemuxerData callback is posted from Decoder to Media thread.
  // 3. DeleteSoon arrives, we delete the player and detach from
  //    BrowserDemuxerAndroid.
  // 4. RequestDemuxerData is processed by the media thread queue. Since the
  //    weak_ptr was invalidated in (3), this is a no-op. If we used
  //    DemuxerAndroid::RequestDemuxerData() it would arrive and will try to
  //    call the client, but the client (i.e. this player) would not exist.
  demuxer_->RequestDemuxerData(stream_type);
}

void MediaCodecPlayer::OnPrefetchDone() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  if (state_ != kStatePrefetching) {
    DVLOG(1) << __FUNCTION__ << " wrong state " << AsString(state_)
             << " ignoring";
    return;  // Ignore
  }

  DVLOG(1) << __FUNCTION__;

  if (!HasAudio() && !HasVideo()) {
    // No configuration at all after prefetching.
    // This is an error, initial configuration is expected
    // before the first data chunk.
    DCHECK(!internal_error_cb_.is_null());
    GetMediaTaskRunner()->PostTask(FROM_HERE, internal_error_cb_);
    return;
  }

  if (HasVideo() && !video_decoder_->HasVideoSurface()) {
    SetState(kStateWaitingForSurface);
    return;
  }

  SetState(kStatePlaying);
  StartPlaybackOrBrowserSeek();
}

void MediaCodecPlayer::OnStopDone() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  if (!(audio_decoder_->IsStopped() && video_decoder_->IsStopped()))
    return;  // Wait until other stream is stopped

  // At this point decoder threads should not be running
  if (interpolator_.interpolating())
    interpolator_.StopInterpolating();

  base::TimeDelta seek_time;
  switch (state_) {
    case kStateStopping: {
      base::TimeDelta seek_time = GetPendingSeek();
      if (seek_time != kNoTimestamp()) {
        SetState(kStateWaitingForSeek);
        SetPendingSeek(kNoTimestamp());
        RequestDemuxerSeek(seek_time);
      } else if (HasPendingStart()) {
        SetPendingStart(false);
        SetState(kStatePrefetching);
        StartPrefetchDecoders();
      } else {
        SetState(kStatePaused);
      }
    } break;
    case kStatePlaying:
      // Unexpected stop means completion
      SetState(kStatePaused);
      break;
    default:
      // DVLOG(0) << __FUNCTION__ << " illegal state: " << AsString(state_);
      // NOTREACHED();
      // Ignore! There can be a race condition: audio posts OnStopDone,
      // then video posts, then first OnStopDone arrives at which point
      // both streams are already stopped, then second OnStopDone arrives. When
      // the second one arrives, the state us not kStateStopping any more.
      break;
  }

  // DetachListener to UI thread
  ui_task_runner_->PostTask(FROM_HERE, detach_listener_cb_);

  if (AudioFinished() && VideoFinished())
    ui_task_runner_->PostTask(FROM_HERE, completion_cb_);
}

void MediaCodecPlayer::OnError() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  // kStateError blocks all events
  SetState(kStateError);

  ReleaseDecoderResources();

  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(error_cb_, MEDIA_ERROR_DECODE));
}

void MediaCodecPlayer::OnStarvation(DemuxerStream::Type type) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " stream type:" << type;

  if (state_ != kStatePlaying)
    return;  // Ignore

  SetState(kStateStopping);
  RequestToStopDecoders();
  SetPendingStart(true);
}

void MediaCodecPlayer::OnTimeIntervalUpdate(DemuxerStream::Type type,
                                            base::TimeDelta now_playing,
                                            base::TimeDelta last_buffered) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  DVLOG(2) << __FUNCTION__ << ": stream type:" << type << " [" << now_playing
           << "," << last_buffered << "]";

  // I assume that audio stream cannot be added after we get configs by
  // OnDemuxerConfigsAvailable(), but that audio can finish early.

  if (type == DemuxerStream::VIDEO) {
    // Ignore video PTS if there is audio stream or if it's behind current
    // time as set by audio stream.
    if (!AudioFinished() || now_playing < interpolator_.GetInterpolatedTime())
      return;
  }

  interpolator_.SetBounds(now_playing, last_buffered);

  // Post to UI thread
  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(time_update_cb_, GetInterpolatedTime(),
                                       base::TimeTicks::Now()));
}

void MediaCodecPlayer::OnVideoCodecCreated() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  // This callback requests resources by releasing other players.
  ui_task_runner_->PostTask(FROM_HERE, request_resources_cb_);
}

void MediaCodecPlayer::OnVideoResolutionChanged(const gfx::Size& size) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  DVLOG(1) << __FUNCTION__ << " " << size.width() << "x" << size.height();

  // Update cache and notify manager on UI thread
  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(metadata_changed_cb_, kNoTimestamp(), size));
}

// State machine operations, called on Media thread

void MediaCodecPlayer::SetState(PlayerState new_state) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  DVLOG(1) << "SetState:" << AsString(state_) << " -> " << AsString(new_state);
  state_ = new_state;
}

void MediaCodecPlayer::SetPendingStart(bool need_to_start) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << ": " << need_to_start;
  pending_start_ = need_to_start;
}

bool MediaCodecPlayer::HasPendingStart() const {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  return pending_start_;
}

void MediaCodecPlayer::SetPendingSeek(base::TimeDelta timestamp) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << ": " << timestamp;
  pending_seek_ = timestamp;
}

base::TimeDelta MediaCodecPlayer::GetPendingSeek() const {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  return pending_seek_;
}

bool MediaCodecPlayer::HasAudio() const {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  return audio_decoder_->HasStream();
}

bool MediaCodecPlayer::HasVideo() const {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  return video_decoder_->HasStream();
}

void MediaCodecPlayer::SetDemuxerConfigs(const DemuxerConfigs& configs) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " " << configs;

  DCHECK(audio_decoder_);
  DCHECK(video_decoder_);

  // At least one valid codec must be present.
  DCHECK(configs.audio_codec != kUnknownAudioCodec ||
         configs.video_codec != kUnknownVideoCodec);

  if (configs.audio_codec != kUnknownAudioCodec)
    audio_decoder_->SetDemuxerConfigs(configs);

  if (configs.video_codec != kUnknownVideoCodec)
    video_decoder_->SetDemuxerConfigs(configs);

  if (state_ == kStateWaitingForConfig) {
    SetState(kStatePrefetching);
    StartPrefetchDecoders();
  }
}

void MediaCodecPlayer::StartPrefetchDecoders() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  bool do_audio = false;
  bool do_video = false;
  int count = 0;
  if (!AudioFinished()) {
    do_audio = true;
    ++count;
  }
  if (!VideoFinished()) {
    do_video = true;
    ++count;
  }

  DCHECK_LT(0, count);  // at least one decoder should be active

  base::Closure prefetch_cb = base::BarrierClosure(
      count, base::Bind(&MediaCodecPlayer::OnPrefetchDone, media_weak_this_));

  if (do_audio)
    audio_decoder_->Prefetch(prefetch_cb);

  if (do_video)
    video_decoder_->Prefetch(prefetch_cb);
}

void MediaCodecPlayer::StartPlaybackOrBrowserSeek() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  // TODO(timav): consider replacing this method with posting a
  // browser seek task (i.e. generate an event) from StartPlaybackDecoders().

  StartStatus status = StartPlaybackDecoders();

  switch (status) {
    case kStartBrowserSeekRequired:
      // Browser seek
      SetState(kStateWaitingForSeek);
      SetPendingStart(true);
      StopDecoders();
      RequestDemuxerSeek(GetInterpolatedTime(), true);
      break;
    case kStartFailed:
      GetMediaTaskRunner()->PostTask(FROM_HERE, internal_error_cb_);
      break;
    case kStartOk:
      break;
  }
}

MediaCodecPlayer::StartStatus MediaCodecPlayer::StartPlaybackDecoders() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  bool do_audio = !AudioFinished();
  bool do_video = !VideoFinished();

  // If there is nothing to play, the state machine should determine this at the
  // prefetch state and never call this method.
  DCHECK(do_audio || do_video);

  // Configure all streams before the start since we may discover that browser
  // seek is required. Start with video: if browser seek is required it would
  // not make sense to configure audio.

  if (do_video) {
    MediaCodecDecoder::ConfigStatus status = video_decoder_->Configure();
    switch (status) {
      case MediaCodecDecoder::kConfigOk:
        break;
      case MediaCodecDecoder::kConfigKeyFrameRequired:
        // TODO(timav): post a task or return the status?
        return kStartBrowserSeekRequired;
      case MediaCodecDecoder::kConfigFailure:
        return kStartFailed;
    }
  }

  if (do_audio) {
    MediaCodecDecoder::ConfigStatus status = audio_decoder_->Configure();
    if (status != MediaCodecDecoder::kConfigOk) {
      return kStartFailed;
    }
  }

  // At this point decoder threads should not be running.
  if (!interpolator_.interpolating())
    interpolator_.StartInterpolating();

  base::TimeDelta current_time = GetInterpolatedTime();

  if (do_audio) {
    if (!audio_decoder_->Start(current_time)) {
      return kStartFailed;
    }

    // Attach listener on UI thread
    ui_task_runner_->PostTask(FROM_HERE, attach_listener_cb_);
  }

  if (do_video) {
    if (!video_decoder_->Start(current_time)) {
      return kStartFailed;
    }
  }

  return kStartOk;
}

void MediaCodecPlayer::StopDecoders() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  video_decoder_->SyncStop();
  audio_decoder_->SyncStop();
}

void MediaCodecPlayer::RequestToStopDecoders() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  bool do_audio = false;
  bool do_video = false;

  if (audio_decoder_->IsPrefetchingOrPlaying())
    do_audio = true;
  if (video_decoder_->IsPrefetchingOrPlaying())
    do_video = true;

  if (!do_audio && !do_video) {
    GetMediaTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&MediaCodecPlayer::OnStopDone, media_weak_this_));
    return;
  }

  if (do_audio)
    audio_decoder_->RequestToStop();
  if (do_video)
    video_decoder_->RequestToStop();
}

void MediaCodecPlayer::RequestDemuxerSeek(base::TimeDelta seek_time,
                                          bool is_browser_seek) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " " << seek_time
           << (is_browser_seek ? " BROWSER_SEEK" : "");

  // Flush decoders before requesting demuxer.
  audio_decoder_->Flush();
  video_decoder_->Flush();

  // Save active seek data. Logically it is attached to kStateWaitingForSeek.
  DCHECK_EQ(kStateWaitingForSeek, state_);
  seek_info_.reset(new SeekInfo(seek_time, is_browser_seek));

  demuxer_->RequestDemuxerSeek(seek_time, is_browser_seek);
}

void MediaCodecPlayer::ReleaseDecoderResources() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  if (audio_decoder_)
    audio_decoder_->ReleaseDecoderResources();

  if (video_decoder_)
    video_decoder_->ReleaseDecoderResources();

  // At this point decoder threads should not be running
  if (interpolator_.interpolating())
    interpolator_.StopInterpolating();
}

void MediaCodecPlayer::CreateDecoders() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  internal_error_cb_ = base::Bind(&MediaCodecPlayer::OnError, media_weak_this_);

  audio_decoder_.reset(new MediaCodecAudioDecoder(
      GetMediaTaskRunner(), base::Bind(&MediaCodecPlayer::RequestDemuxerData,
                                       media_weak_this_, DemuxerStream::AUDIO),
      base::Bind(&MediaCodecPlayer::OnStarvation, media_weak_this_,
                 DemuxerStream::AUDIO),
      base::Bind(&MediaCodecPlayer::OnStopDone, media_weak_this_),
      internal_error_cb_,
      base::Bind(&MediaCodecPlayer::OnTimeIntervalUpdate, media_weak_this_,
                 DemuxerStream::AUDIO)));

  video_decoder_.reset(new MediaCodecVideoDecoder(
      GetMediaTaskRunner(), base::Bind(&MediaCodecPlayer::RequestDemuxerData,
                                       media_weak_this_, DemuxerStream::VIDEO),
      base::Bind(&MediaCodecPlayer::OnStarvation, media_weak_this_,
                 DemuxerStream::VIDEO),
      base::Bind(&MediaCodecPlayer::OnStopDone, media_weak_this_),
      internal_error_cb_,
      base::Bind(&MediaCodecPlayer::OnTimeIntervalUpdate, media_weak_this_,
                 DemuxerStream::VIDEO),
      base::Bind(&MediaCodecPlayer::OnVideoResolutionChanged, media_weak_this_),
      base::Bind(&MediaCodecPlayer::OnVideoCodecCreated, media_weak_this_)));
}

bool MediaCodecPlayer::AudioFinished() const {
  return audio_decoder_->IsCompleted() || !audio_decoder_->HasStream();
}

bool MediaCodecPlayer::VideoFinished() const {
  return video_decoder_->IsCompleted() || !video_decoder_->HasStream();
}

base::TimeDelta MediaCodecPlayer::GetInterpolatedTime() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  base::TimeDelta interpolated_time = interpolator_.GetInterpolatedTime();
  return std::min(interpolated_time, duration_);
}

#undef RETURN_STRING
#define RETURN_STRING(x) \
  case x:                \
    return #x;

const char* MediaCodecPlayer::AsString(PlayerState state) {
  switch (state) {
    RETURN_STRING(kStatePaused);
    RETURN_STRING(kStateWaitingForConfig);
    RETURN_STRING(kStatePrefetching);
    RETURN_STRING(kStatePlaying);
    RETURN_STRING(kStateStopping);
    RETURN_STRING(kStateWaitingForSurface);
    RETURN_STRING(kStateWaitingForSeek);
    RETURN_STRING(kStateError);
  }
  return nullptr;  // crash early
}

#undef RETURN_STRING

}  // namespace media
