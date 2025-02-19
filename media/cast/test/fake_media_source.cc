// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/fake_media_source.h"

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/media.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/cast/cast_sender.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_deleters.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/in_memory_url_protocol.h"
#include "ui/gfx/geometry/size.h"

namespace {

static const int kSoundFrequency = 440;  // Frequency of sinusoid wave.
static const float kSoundVolume = 0.10f;
static const int kAudioFrameMs = 10;  // Each audio frame is exactly 10ms.
static const int kAudioPacketsPerSecond = 1000 / kAudioFrameMs;

// Bounds for variable frame size mode.
static const int kMinFakeFrameWidth = 60;
static const int kMinFakeFrameHeight = 34;
static const int kStartingFakeFrameWidth = 854;
static const int kStartingFakeFrameHeight = 480;
static const int kMaxFakeFrameWidth = 1280;
static const int kMaxFakeFrameHeight = 720;
static const int kMaxFrameSizeChangeMillis = 5000;

void AVFreeFrame(AVFrame* frame) {
  av_frame_free(&frame);
}

base::TimeDelta PtsToTimeDelta(int64 pts, const AVRational& time_base) {
  return pts * base::TimeDelta::FromSeconds(1) * time_base.num / time_base.den;
}

int64 TimeDeltaToPts(base::TimeDelta delta, const AVRational& time_base) {
  return static_cast<int64>(
      delta.InSecondsF() * time_base.den / time_base.num +
      0.5 /* rounding */);
}

}  // namespace

namespace media {
namespace cast {

FakeMediaSource::FakeMediaSource(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::TickClock* clock,
    const AudioSenderConfig& audio_config,
    const VideoSenderConfig& video_config,
    bool keep_frames)
    : task_runner_(task_runner),
      output_audio_params_(AudioParameters::AUDIO_PCM_LINEAR,
                           media::GuessChannelLayout(audio_config.channels),
                           audio_config.frequency,
                           32,
                           audio_config.frequency / kAudioPacketsPerSecond),
      video_config_(video_config),
      keep_frames_(keep_frames),
      variable_frame_size_mode_(false),
      synthetic_count_(0),
      clock_(clock),
      audio_frame_count_(0),
      video_frame_count_(0),
      av_format_context_(NULL),
      audio_stream_index_(-1),
      playback_rate_(1.0),
      video_stream_index_(-1),
      video_frame_rate_numerator_(video_config.max_frame_rate),
      video_frame_rate_denominator_(1),
      video_first_pts_(0),
      video_first_pts_set_(false),
      weak_factory_(this) {
  CHECK(output_audio_params_.IsValid());
  audio_bus_factory_.reset(new TestAudioBusFactory(audio_config.channels,
                                                   audio_config.frequency,
                                                   kSoundFrequency,
                                                   kSoundVolume));
}

FakeMediaSource::~FakeMediaSource() {
}

void FakeMediaSource::SetSourceFile(const base::FilePath& video_file,
                                    int final_fps) {
  DCHECK(!video_file.empty());

  LOG(INFO) << "Source: " << video_file.value();
  if (!file_data_.Initialize(video_file)) {
    LOG(ERROR) << "Cannot load file.";
    return;
  }
  protocol_.reset(
      new InMemoryUrlProtocol(file_data_.data(), file_data_.length(), false));
  glue_.reset(new FFmpegGlue(protocol_.get()));

  if (!glue_->OpenContext()) {
    LOG(ERROR) << "Cannot open file.";
    return;
  }

  // AVFormatContext is owned by the glue.
  av_format_context_ = glue_->format_context();
  if (avformat_find_stream_info(av_format_context_, NULL) < 0) {
    LOG(ERROR) << "Cannot find stream information.";
    return;
  }

  // Prepare FFmpeg decoders.
  for (unsigned int i = 0; i < av_format_context_->nb_streams; ++i) {
    AVStream* av_stream = av_format_context_->streams[i];
    AVCodecContext* av_codec_context = av_stream->codec;
    AVCodec* av_codec = avcodec_find_decoder(av_codec_context->codec_id);

    if (!av_codec) {
      LOG(ERROR) << "Cannot find decoder for the codec: "
                 << av_codec_context->codec_id;
      continue;
    }

    // Number of threads for decoding.
    av_codec_context->thread_count = 2;
    av_codec_context->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
    av_codec_context->request_sample_fmt = AV_SAMPLE_FMT_S16;

    if (avcodec_open2(av_codec_context, av_codec, NULL) < 0) {
      LOG(ERROR) << "Cannot open AVCodecContext for the codec: "
                 << av_codec_context->codec_id;
      return;
    }

    if (av_codec->type == AVMEDIA_TYPE_AUDIO) {
      if (av_codec_context->sample_fmt == AV_SAMPLE_FMT_S16P) {
        LOG(ERROR) << "Audio format not supported.";
        continue;
      }
      ChannelLayout layout = ChannelLayoutToChromeChannelLayout(
          av_codec_context->channel_layout,
          av_codec_context->channels);
      if (layout == CHANNEL_LAYOUT_UNSUPPORTED) {
        LOG(ERROR) << "Unsupported audio channels layout.";
        continue;
      }
      if (audio_stream_index_ != -1) {
        LOG(WARNING) << "Found multiple audio streams.";
      }
      audio_stream_index_ = static_cast<int>(i);
      source_audio_params_.Reset(
          AudioParameters::AUDIO_PCM_LINEAR,
          layout,
          av_codec_context->channels,
          av_codec_context->sample_rate,
          8 * av_get_bytes_per_sample(av_codec_context->sample_fmt),
          av_codec_context->sample_rate / kAudioPacketsPerSecond);
      CHECK(source_audio_params_.IsValid());
      LOG(INFO) << "Source file has audio.";
    } else if (av_codec->type == AVMEDIA_TYPE_VIDEO) {
      VideoPixelFormat format =
          AVPixelFormatToVideoPixelFormat(av_codec_context->pix_fmt);
      if (format != PIXEL_FORMAT_YV12) {
        LOG(ERROR) << "Cannot handle non YV12 video format: " << format;
        continue;
      }
      if (video_stream_index_ != -1) {
        LOG(WARNING) << "Found multiple video streams.";
      }
      video_stream_index_ = static_cast<int>(i);
      if (final_fps > 0) {
        // If video is played at a manual speed audio needs to match.
        playback_rate_ = 1.0 * final_fps *
            av_stream->r_frame_rate.den / av_stream->r_frame_rate.num;
        video_frame_rate_numerator_ = final_fps;
        video_frame_rate_denominator_ = 1;
      } else {
        playback_rate_ = 1.0;
        video_frame_rate_numerator_ = av_stream->r_frame_rate.num;
        video_frame_rate_denominator_ = av_stream->r_frame_rate.den;
      }
      LOG(INFO) << "Source file has video.";
    } else {
      LOG(ERROR) << "Unknown stream type; ignore.";
    }
  }

  Rewind();
}

void FakeMediaSource::SetVariableFrameSizeMode(bool enabled) {
  variable_frame_size_mode_ = enabled;
}

void FakeMediaSource::Start(scoped_refptr<AudioFrameInput> audio_frame_input,
                            scoped_refptr<VideoFrameInput> video_frame_input) {
  audio_frame_input_ = audio_frame_input;
  video_frame_input_ = video_frame_input;

  LOG(INFO) << "Max Frame rate: " << video_config_.max_frame_rate;
  LOG(INFO) << "Source Frame rate: "
            << video_frame_rate_numerator_ << "/"
            << video_frame_rate_denominator_ << " fps.";
  LOG(INFO) << "Audio playback rate: " << playback_rate_;

  if (start_time_.is_null())
    start_time_ = clock_->NowTicks();

  if (!is_transcoding_audio() && !is_transcoding_video()) {
    // Send fake patterns.
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&FakeMediaSource::SendNextFakeFrame,
                   weak_factory_.GetWeakPtr()));
    return;
  }

  // Send transcoding streams.
  audio_algo_.Initialize(source_audio_params_);
  audio_algo_.FlushBuffers();
  audio_fifo_input_bus_ = AudioBus::Create(
      source_audio_params_.channels(),
      source_audio_params_.frames_per_buffer());
  // Audio FIFO can carry all data fron AudioRendererAlgorithm.
  audio_fifo_.reset(
      new AudioFifo(source_audio_params_.channels(),
                    audio_algo_.QueueCapacity()));
  audio_converter_.reset(new media::AudioConverter(
      source_audio_params_, output_audio_params_, true));
  audio_converter_->AddInput(this);
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FakeMediaSource::SendNextFrame, weak_factory_.GetWeakPtr()));
}

void FakeMediaSource::SendNextFakeFrame() {
  UpdateNextFrameSize();
  scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateBlackFrame(current_frame_size_);
  PopulateVideoFrame(video_frame.get(), synthetic_count_);
  ++synthetic_count_;

  const base::TimeTicks now = clock_->NowTicks();

  base::TimeDelta video_time = VideoFrameTime(++video_frame_count_);
  video_frame->set_timestamp(video_time);
  if (keep_frames_)
    inserted_video_frame_queue_.push(video_frame);
  video_frame_input_->InsertRawVideoFrame(video_frame,
                                          start_time_ + video_time);

  // Send just enough audio data to match next video frame's time.
  base::TimeDelta audio_time = AudioFrameTime(audio_frame_count_);
  while (audio_time < video_time) {
    if (is_transcoding_audio()) {
      Decode(true);
      CHECK(!audio_bus_queue_.empty()) << "No audio decoded.";
      scoped_ptr<AudioBus> bus(audio_bus_queue_.front());
      audio_bus_queue_.pop();
      audio_frame_input_->InsertAudio(
          bus.Pass(), start_time_ + audio_time);
    } else {
      audio_frame_input_->InsertAudio(
          audio_bus_factory_->NextAudioBus(
              base::TimeDelta::FromMilliseconds(kAudioFrameMs)),
          start_time_ + audio_time);
    }
    audio_time = AudioFrameTime(++audio_frame_count_);
  }

  // This is the time since FakeMediaSource was started.
  const base::TimeDelta elapsed_time = now - start_time_;

  // Handle the case when frame generation cannot keep up.
  // Move the time ahead to match the next frame.
  while (video_time < elapsed_time) {
    LOG(WARNING) << "Skipping one frame.";
    video_time = VideoFrameTime(++video_frame_count_);
  }

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeMediaSource::SendNextFakeFrame,
                 weak_factory_.GetWeakPtr()),
      video_time - elapsed_time);
}

void FakeMediaSource::UpdateNextFrameSize() {
  if (variable_frame_size_mode_) {
    bool update_size_change_time = false;
    if (current_frame_size_.IsEmpty()) {
      current_frame_size_ = gfx::Size(kStartingFakeFrameWidth,
                                      kStartingFakeFrameHeight);
      update_size_change_time = true;
    } else if (clock_->NowTicks() >= next_frame_size_change_time_) {
      current_frame_size_ = gfx::Size(
          base::RandInt(kMinFakeFrameWidth, kMaxFakeFrameWidth),
          base::RandInt(kMinFakeFrameHeight, kMaxFakeFrameHeight));
      update_size_change_time = true;
    }

    if (update_size_change_time) {
      next_frame_size_change_time_ = clock_->NowTicks() +
          base::TimeDelta::FromMillisecondsD(
              base::RandDouble() * kMaxFrameSizeChangeMillis);
    }
  } else {
    current_frame_size_ = gfx::Size(kStartingFakeFrameWidth,
                                    kStartingFakeFrameHeight);
    next_frame_size_change_time_ = base::TimeTicks();
  }
}

bool FakeMediaSource::SendNextTranscodedVideo(base::TimeDelta elapsed_time) {
  if (!is_transcoding_video())
    return false;

  Decode(false);
  if (video_frame_queue_.empty())
    return false;

  const scoped_refptr<VideoFrame> video_frame = video_frame_queue_.front();
  if (elapsed_time < video_frame->timestamp())
    return false;
  video_frame_queue_.pop();

  // Use the timestamp from the file if we're transcoding.
  video_frame->set_timestamp(ScaleTimestamp(video_frame->timestamp()));
  if (keep_frames_)
    inserted_video_frame_queue_.push(video_frame);
  video_frame_input_->InsertRawVideoFrame(
      video_frame, start_time_ + video_frame->timestamp());

  // Make sure queue is not empty.
  Decode(false);
  return true;
}

bool FakeMediaSource::SendNextTranscodedAudio(base::TimeDelta elapsed_time) {
  if (!is_transcoding_audio())
    return false;

  Decode(true);
  if (audio_bus_queue_.empty())
    return false;

  base::TimeDelta audio_time = audio_sent_ts_->GetTimestamp();
  if (elapsed_time < audio_time)
    return false;
  scoped_ptr<AudioBus> bus(audio_bus_queue_.front());
  audio_bus_queue_.pop();
  audio_sent_ts_->AddFrames(bus->frames());
  audio_frame_input_->InsertAudio(
      bus.Pass(), start_time_ + audio_time);

  // Make sure queue is not empty.
  Decode(true);
  return true;
}

void FakeMediaSource::SendNextFrame() {
  // Send as much as possible. Audio is sent according to
  // system time.
  while (SendNextTranscodedAudio(clock_->NowTicks() - start_time_));

  // Video is sync'ed to audio.
  while (SendNextTranscodedVideo(audio_sent_ts_->GetTimestamp()));

  if (audio_bus_queue_.empty() && video_frame_queue_.empty()) {
    // Both queues are empty can only mean that we have reached
    // the end of the stream.
    LOG(INFO) << "Rewind.";
    Rewind();
  }

  // Send next send.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeMediaSource::SendNextFrame, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kAudioFrameMs));
}

base::TimeDelta FakeMediaSource::VideoFrameTime(int frame_number) {
  return frame_number * base::TimeDelta::FromSeconds(1) *
      video_frame_rate_denominator_ / video_frame_rate_numerator_;
}

base::TimeDelta FakeMediaSource::ScaleTimestamp(base::TimeDelta timestamp) {
  return base::TimeDelta::FromSecondsD(timestamp.InSecondsF() / playback_rate_);
}

base::TimeDelta FakeMediaSource::AudioFrameTime(int frame_number) {
  return frame_number * base::TimeDelta::FromMilliseconds(kAudioFrameMs);
}

void FakeMediaSource::Rewind() {
  CHECK(av_seek_frame(av_format_context_, -1, 0, AVSEEK_FLAG_BACKWARD) >= 0)
      << "Failed to rewind to the beginning.";
}

ScopedAVPacket FakeMediaSource::DemuxOnePacket(bool* audio) {
  ScopedAVPacket packet(new AVPacket());
  if (av_read_frame(av_format_context_, packet.get()) < 0) {
    VLOG(1) << "Failed to read one AVPacket.";
    packet.reset();
    return packet.Pass();
  }

  int stream_index = static_cast<int>(packet->stream_index);
  if (stream_index == audio_stream_index_) {
    *audio = true;
  } else if (stream_index == video_stream_index_) {
    *audio = false;
  } else {
    // Ignore unknown packet.
    LOG(INFO) << "Unknown packet.";
    packet.reset();
  }
  return packet.Pass();
}

void FakeMediaSource::DecodeAudio(ScopedAVPacket packet) {
  // Audio.
  AVFrame* avframe = av_frame_alloc();

  // Make a shallow copy of packet so we can slide packet.data as frames are
  // decoded from the packet; otherwise av_free_packet() will corrupt memory.
  AVPacket packet_temp = *packet.get();

  do {
    int frame_decoded = 0;
    int result = avcodec_decode_audio4(
        av_audio_context(), avframe, &frame_decoded, &packet_temp);
    CHECK(result >= 0) << "Failed to decode audio.";
    packet_temp.size -= result;
    packet_temp.data += result;
    if (!frame_decoded)
      continue;

    int frames_read = avframe->nb_samples;
    if (frames_read < 0)
      break;

    if (!audio_sent_ts_) {
      // Initialize the base time to the first packet in the file.
      // This is set to the frequency we send to the receiver.
      // Not the frequency of the source file. This is because we
      // increment the frame count by samples we sent.
      audio_sent_ts_.reset(
          new AudioTimestampHelper(output_audio_params_.sample_rate()));
      // For some files this is an invalid value.
      base::TimeDelta base_ts;
      audio_sent_ts_->SetBaseTimestamp(base_ts);
    }

    scoped_refptr<AudioBuffer> buffer =
        AudioBuffer::CopyFrom(
            AVSampleFormatToSampleFormat(
                av_audio_context()->sample_fmt),
            ChannelLayoutToChromeChannelLayout(
                av_audio_context()->channel_layout,
                av_audio_context()->channels),
            av_audio_context()->channels,
            av_audio_context()->sample_rate,
            frames_read,
            &avframe->data[0],
            PtsToTimeDelta(avframe->pkt_pts, av_audio_stream()->time_base));
    audio_algo_.EnqueueBuffer(buffer);
    av_frame_unref(avframe);
  } while (packet_temp.size > 0);
  av_frame_free(&avframe);

  const int frames_needed_to_scale =
      playback_rate_ * av_audio_context()->sample_rate /
      kAudioPacketsPerSecond;
  while (frames_needed_to_scale <= audio_algo_.frames_buffered()) {
    if (!audio_algo_.FillBuffer(audio_fifo_input_bus_.get(), 0,
                                audio_fifo_input_bus_->frames(),
                                playback_rate_)) {
      // Nothing can be scaled. Decode some more.
      return;
    }

    // Prevent overflow of audio data in the FIFO.
    if (audio_fifo_input_bus_->frames() + audio_fifo_->frames()
        <= audio_fifo_->max_frames()) {
      audio_fifo_->Push(audio_fifo_input_bus_.get());
    } else {
      LOG(WARNING) << "Audio FIFO full; dropping samples.";
    }

    // Make sure there's enough data to resample audio.
    if (audio_fifo_->frames() <
        2 * source_audio_params_.sample_rate() / kAudioPacketsPerSecond) {
      continue;
    }

    scoped_ptr<media::AudioBus> resampled_bus(
        media::AudioBus::Create(
            output_audio_params_.channels(),
            output_audio_params_.sample_rate() / kAudioPacketsPerSecond));
    audio_converter_->Convert(resampled_bus.get());
    audio_bus_queue_.push(resampled_bus.release());
  }
}

void FakeMediaSource::DecodeVideo(ScopedAVPacket packet) {
  // Video.
  int got_picture;
  AVFrame* avframe = av_frame_alloc();
  CHECK(avcodec_decode_video2(
      av_video_context(), avframe, &got_picture, packet.get()) >= 0)
      << "Video decode error.";
  if (!got_picture) {
    av_frame_free(&avframe);
    return;
  }
  gfx::Size size(av_video_context()->width, av_video_context()->height);

  if (!video_first_pts_set_) {
    video_first_pts_ = avframe->pkt_pts;
    video_first_pts_set_ = true;
  }
  const AVRational& time_base = av_video_stream()->time_base;
  base::TimeDelta timestamp =
      PtsToTimeDelta(avframe->pkt_pts - video_first_pts_, time_base);
  if (timestamp < last_video_frame_timestamp_) {
    // Stream has rewound.  Rebase |video_first_pts_|.
    const AVRational& frame_rate = av_video_stream()->r_frame_rate;
    timestamp = last_video_frame_timestamp_ +
        (base::TimeDelta::FromSeconds(1) * frame_rate.den / frame_rate.num);
    const int64 adjustment_pts = TimeDeltaToPts(timestamp, time_base);
    video_first_pts_ = avframe->pkt_pts - adjustment_pts;
  }

  video_frame_queue_.push(VideoFrame::WrapExternalYuvData(
      media::PIXEL_FORMAT_YV12, size, gfx::Rect(size), size,
      avframe->linesize[0], avframe->linesize[1], avframe->linesize[2],
      avframe->data[0], avframe->data[1], avframe->data[2], timestamp));
  video_frame_queue_.back()->AddDestructionObserver(
      base::Bind(&AVFreeFrame, avframe));
  last_video_frame_timestamp_ = timestamp;
}

void FakeMediaSource::Decode(bool decode_audio) {
  // Read the stream until one video frame can be decoded.
  while (true) {
    if (decode_audio && !audio_bus_queue_.empty())
      return;
    if (!decode_audio && !video_frame_queue_.empty())
      return;

    bool audio_packet = false;
    ScopedAVPacket packet = DemuxOnePacket(&audio_packet);
    if (!packet) {
      VLOG(1) << "End of stream.";
      return;
    }

    if (audio_packet)
      DecodeAudio(packet.Pass());
    else
      DecodeVideo(packet.Pass());
  }
}

double FakeMediaSource::ProvideInput(media::AudioBus* output_bus,
                                   base::TimeDelta buffer_delay) {
  if (audio_fifo_->frames() >= output_bus->frames()) {
    audio_fifo_->Consume(output_bus, 0, output_bus->frames());
    return 1.0;
  } else {
    LOG(WARNING) << "Not enough audio data for resampling.";
    output_bus->Zero();
    return 0.0;
  }
}

scoped_refptr<media::VideoFrame>
FakeMediaSource::PopOldestInsertedVideoFrame() {
  CHECK(!inserted_video_frame_queue_.empty());
  scoped_refptr<media::VideoFrame> video_frame =
      inserted_video_frame_queue_.front();
  inserted_video_frame_queue_.pop();
  return video_frame;
}

AVStream* FakeMediaSource::av_audio_stream() {
  return av_format_context_->streams[audio_stream_index_];
}

AVStream* FakeMediaSource::av_video_stream() {
  return av_format_context_->streams[video_stream_index_];
}

AVCodecContext* FakeMediaSource::av_audio_context() {
  return av_audio_stream()->codec;
}

AVCodecContext* FakeMediaSource::av_video_context() {
  return av_video_stream()->codec;
}

}  // namespace cast
}  // namespace media
