// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/base/buffers.h"
#include "media/base/channel_layout.h"
#include "media/formats/webm/tracks_builder.h"
#include "media/formats/webm/webm_constants.h"
#include "media/formats/webm/webm_tracks_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;

namespace media {

static const double kDefaultTimecodeScaleInUs = 1000.0;  // 1 ms resolution

class WebMTracksParserTest : public testing::Test {
 public:
  WebMTracksParserTest() {}
};

static void VerifyTextTrackInfo(const uint8* buffer,
                                int buffer_size,
                                TextKind text_kind,
                                const std::string& name,
                                const std::string& language) {
  scoped_ptr<WebMTracksParser> parser(
      new WebMTracksParser(new MediaLog(), false));

  int result = parser->Parse(buffer, buffer_size);
  EXPECT_GT(result, 0);
  EXPECT_EQ(result, buffer_size);

  const WebMTracksParser::TextTracks& text_tracks = parser->text_tracks();
  EXPECT_EQ(text_tracks.size(), WebMTracksParser::TextTracks::size_type(1));

  const WebMTracksParser::TextTracks::const_iterator itr = text_tracks.begin();
  EXPECT_EQ(itr->first, 1);  // track num

  const TextTrackConfig& config = itr->second;
  EXPECT_EQ(config.kind(), text_kind);
  EXPECT_TRUE(config.label() == name);
  EXPECT_TRUE(config.language() == language);
}

TEST_F(WebMTracksParserTest, SubtitleNoNameNoLang) {
  InSequence s;

  TracksBuilder tb;
  tb.AddTextTrack(1, 1, kWebMCodecSubtitles, "", "");

  const std::vector<uint8> buf = tb.Finish();
  VerifyTextTrackInfo(&buf[0], buf.size(), kTextSubtitles, "", "");
}

TEST_F(WebMTracksParserTest, SubtitleYesNameNoLang) {
  InSequence s;

  TracksBuilder tb;
  tb.AddTextTrack(1, 1, kWebMCodecSubtitles, "Spock", "");

  const std::vector<uint8> buf = tb.Finish();
  VerifyTextTrackInfo(&buf[0], buf.size(), kTextSubtitles, "Spock", "");
}

TEST_F(WebMTracksParserTest, SubtitleNoNameYesLang) {
  InSequence s;

  TracksBuilder tb;
  tb.AddTextTrack(1, 1, kWebMCodecSubtitles, "", "eng");

  const std::vector<uint8> buf = tb.Finish();
  VerifyTextTrackInfo(&buf[0], buf.size(), kTextSubtitles, "", "eng");
}

TEST_F(WebMTracksParserTest, SubtitleYesNameYesLang) {
  InSequence s;

  TracksBuilder tb;
  tb.AddTextTrack(1, 1, kWebMCodecSubtitles, "Picard", "fre");

  const std::vector<uint8> buf = tb.Finish();
  VerifyTextTrackInfo(&buf[0], buf.size(), kTextSubtitles, "Picard", "fre");
}

TEST_F(WebMTracksParserTest, IgnoringTextTracks) {
  InSequence s;

  TracksBuilder tb;
  tb.AddTextTrack(1, 1, kWebMCodecSubtitles, "Subtitles", "fre");
  tb.AddTextTrack(2, 2, kWebMCodecSubtitles, "Commentary", "fre");

  const std::vector<uint8> buf = tb.Finish();
  scoped_ptr<WebMTracksParser> parser(
      new WebMTracksParser(new MediaLog(), true));

  int result = parser->Parse(&buf[0], buf.size());
  EXPECT_GT(result, 0);
  EXPECT_EQ(result, static_cast<int>(buf.size()));

  EXPECT_EQ(parser->text_tracks().size(), 0u);

  const std::set<int64>& ignored_tracks = parser->ignored_tracks();
  EXPECT_TRUE(ignored_tracks.find(1) != ignored_tracks.end());
  EXPECT_TRUE(ignored_tracks.find(2) != ignored_tracks.end());

  // Test again w/o ignoring the test tracks.
  parser.reset(new WebMTracksParser(new MediaLog(), false));

  result = parser->Parse(&buf[0], buf.size());
  EXPECT_GT(result, 0);

  EXPECT_EQ(parser->ignored_tracks().size(), 0u);
  EXPECT_EQ(parser->text_tracks().size(), 2u);
}

TEST_F(WebMTracksParserTest, AudioVideoDefaultDurationUnset) {
  // Other audio/video decoder config fields are necessary in the test
  // audio/video TrackEntry configurations. This method does only very minimal
  // verification of their inclusion and parsing; the goal is to confirm
  // TrackEntry DefaultDuration defaults to -1 if not included in audio or
  // video TrackEntry.
  TracksBuilder tb;
  tb.AddAudioTrack(1, 1, "A_VORBIS", "audio", "", -1, 2, 8000);
  tb.AddVideoTrack(2, 2, "V_VP8", "video", "", -1, 320, 240);
  const std::vector<uint8> buf = tb.Finish();

  scoped_ptr<WebMTracksParser> parser(
      new WebMTracksParser(new MediaLog(), true));
  int result = parser->Parse(&buf[0], buf.size());
  EXPECT_LE(0, result);
  EXPECT_EQ(static_cast<int>(buf.size()), result);

  EXPECT_EQ(kNoTimestamp(),
            parser->GetAudioDefaultDuration(kDefaultTimecodeScaleInUs));
  EXPECT_EQ(kNoTimestamp(),
            parser->GetVideoDefaultDuration(kDefaultTimecodeScaleInUs));

  const VideoDecoderConfig& video_config = parser->video_decoder_config();
  EXPECT_TRUE(video_config.IsValidConfig());
  EXPECT_EQ(320, video_config.coded_size().width());
  EXPECT_EQ(240, video_config.coded_size().height());

  const AudioDecoderConfig& audio_config = parser->audio_decoder_config();
  EXPECT_TRUE(audio_config.IsValidConfig());
  EXPECT_EQ(CHANNEL_LAYOUT_STEREO, audio_config.channel_layout());
  EXPECT_EQ(8000, audio_config.samples_per_second());
}

TEST_F(WebMTracksParserTest, AudioVideoDefaultDurationSet) {
  // Confirm audio or video TrackEntry DefaultDuration values are parsed, if
  // present.
  TracksBuilder tb;
  tb.AddAudioTrack(1, 1, "A_VORBIS", "audio", "", 12345678, 2, 8000);
  tb.AddVideoTrack(2, 2, "V_VP8", "video", "", 987654321, 320, 240);
  const std::vector<uint8> buf = tb.Finish();

  scoped_ptr<WebMTracksParser> parser(
      new WebMTracksParser(new MediaLog(), true));
  int result = parser->Parse(&buf[0], buf.size());
  EXPECT_LE(0, result);
  EXPECT_EQ(static_cast<int>(buf.size()), result);

  EXPECT_EQ(base::TimeDelta::FromMicroseconds(12000),
            parser->GetAudioDefaultDuration(kDefaultTimecodeScaleInUs));
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(985000),
            parser->GetVideoDefaultDuration(5000.0));  // 5 ms resolution
  EXPECT_EQ(kNoTimestamp(), parser->GetAudioDefaultDuration(12346.0));
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(12345),
            parser->GetAudioDefaultDuration(12345.0));
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(12003),
            parser->GetAudioDefaultDuration(1000.3));  // 1.0003 ms resolution
}

TEST_F(WebMTracksParserTest, InvalidZeroDefaultDurationSet) {
  // Confirm parse error if TrackEntry DefaultDuration is present, but is 0ns.
  TracksBuilder tb(true);
  tb.AddAudioTrack(1, 1, "A_VORBIS", "audio", "", 0, 2, 8000);
  const std::vector<uint8> buf = tb.Finish();

  scoped_ptr<WebMTracksParser> parser(
      new WebMTracksParser(new MediaLog(), true));
  EXPECT_EQ(-1, parser->Parse(&buf[0], buf.size()));
}

TEST_F(WebMTracksParserTest, HighTrackUID) {
  // Confirm no parse error if TrackEntry TrackUID has MSb set
  // (http://crbug.com/397067).
  TracksBuilder tb(true);
  tb.AddAudioTrack(1, 1ULL << 31, "A_VORBIS", "audio", "", 40, 2, 8000);
  const std::vector<uint8> buf = tb.Finish();

  scoped_ptr<WebMTracksParser> parser(
      new WebMTracksParser(new MediaLog(), true));
  EXPECT_GT(parser->Parse(&buf[0], buf.size()),0);
}

}  // namespace media
