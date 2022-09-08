// Sora
#include <sora/camera_device_capturer.h>
#include <sora/sora_default_client.h>

// CLI11
#include <CLI/CLI.hpp>

// Boost
#include <boost/optional/optional.hpp>

#include "sdl_renderer.h"

#ifdef _WIN32
#include <rtc_base/win/scoped_com_initializer.h>
#endif

struct SDLSampleConfig : sora::SoraDefaultClientConfig {
  std::string signaling_url;
  std::string channel_id;
  std::string role;
  std::string video_codec_type;
  boost::optional<bool> multistream;
  int width = 640;
  int height = 480;
  bool show_me = false;
  bool fullscreen = false;
};

class SDLSample : public std::enable_shared_from_this<SDLSample>,
                  public sora::SoraDefaultClient {
 public:
  SDLSample(SDLSampleConfig config)
      : sora::SoraDefaultClient(config), config_(config) {}

  void Run() {
    renderer_.reset(
        new SDLRenderer(config_.width, config_.height, config_.fullscreen));

    if (config_.role != "recvonly") {
      sora::CameraDeviceCapturerConfig cam_config;
      cam_config.width = 640;
      cam_config.height = 480;
      cam_config.fps = 30;
      auto video_source = sora::CreateCameraDeviceCapturer(cam_config);

      std::string audio_track_id = rtc::CreateRandomString(16);
      std::string video_track_id = rtc::CreateRandomString(16);
      audio_track_ = factory()->CreateAudioTrack(
          audio_track_id,
          factory()->CreateAudioSource(cricket::AudioOptions()).get());
      video_track_ =
          factory()->CreateVideoTrack(video_track_id, video_source.get());
      if (config_.show_me) {
        renderer_->AddTrack(video_track_.get());
      }
    }

    ioc_.reset(new boost::asio::io_context(1));

    sora::SoraSignalingConfig config;
    config.pc_factory = factory();
    config.io_context = ioc_.get();
    config.observer = shared_from_this();
    config.signaling_urls.push_back(config_.signaling_url);
    config.channel_id = config_.channel_id;
    config.multistream = config_.multistream;
    config.role = config_.role;
    config.video_codec_type = config_.video_codec_type;
    conn_ = sora::SoraSignaling::Create(config);

    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_guard(ioc_->get_executor());

    boost::asio::signal_set signals(*ioc_, SIGINT, SIGTERM);
    signals.async_wait(
        [this](const boost::system::error_code&, int) { conn_->Disconnect(); });

    conn_->Connect();

    renderer_->SetDispatchFunction([this](std::function<void()> f) {
      if (ioc_->stopped())
        return;
      boost::asio::dispatch(ioc_->get_executor(), f);
    });

    ioc_->run();
  }

  void OnSetOffer(std::string offer) override {
    std::string stream_id = rtc::CreateRandomString(16);
    if (audio_track_ != nullptr) {
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>>
          audio_result =
              conn_->GetPeerConnection()->AddTrack(audio_track_, {stream_id});
    }
    if (video_track_ != nullptr) {
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>>
          video_result =
              conn_->GetPeerConnection()->AddTrack(video_track_, {stream_id});
    }
  }
  void OnDisconnect(sora::SoraSignalingErrorCode ec,
                    std::string message) override {
    RTC_LOG(LS_INFO) << "OnDisconnect: " << message;
    renderer_.reset();
    ioc_->stop();
  }

  void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
      override {
    auto track = transceiver->receiver()->track();
    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
      renderer_->AddTrack(
          static_cast<webrtc::VideoTrackInterface*>(track.get()));
    }
  }
  void OnRemoveTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override {
    auto track = receiver->track();
    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
      renderer_->RemoveTrack(
          static_cast<webrtc::VideoTrackInterface*>(track.get()));
    }
  }

 private:
  SDLSampleConfig config_;
  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track_;
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_;
  std::shared_ptr<sora::SoraSignaling> conn_;
  std::unique_ptr<boost::asio::io_context> ioc_;
  std::unique_ptr<SDLRenderer> renderer_;
};

void add_optional_bool(CLI::App& app,
                       const std::string& option_name,
                       boost::optional<bool>& v,
                       const std::string& help_text) {
  auto f = [&v](const std::string& input) {
    if (input == "true") {
      v = true;
    } else if (input == "false") {
      v = false;
    } else if (input == "none") {
      v = boost::none;
    } else {
      throw CLI::ConversionError(input, "optional<bool>");
    }
  };
  app.add_option_function<std::string>(option_name, f, help_text)
      ->type_name("TEXT")
      ->check(CLI::IsMember({"true", "false", "none"}));
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
  webrtc::ScopedCOMInitializer com_initializer(
      webrtc::ScopedCOMInitializer::kMTA);
  if (!com_initializer.Succeeded()) {
    std::cerr << "CoInitializeEx failed" << std::endl;
    return 1;
  }
#endif

  SDLSampleConfig config;

  CLI::App app("SDL Sample for Sora C++ SDK");
  app.set_help_all_flag("--help-all",
                        "Print help message for all modes and exit");

  int log_level = (int)rtc::LS_ERROR;
  auto log_level_map = std::vector<std::pair<std::string, int>>(
      {{"verbose", 0}, {"info", 1}, {"warning", 2}, {"error", 3}, {"none", 4}});
  app.add_option("--log-level", log_level, "Log severity level threshold")
      ->transform(CLI::CheckedTransformer(log_level_map, CLI::ignore_case));

  // Sora に関するオプション
  app.add_option("--signaling-url", config.signaling_url, "Signaling URL")
      ->required();
  app.add_option("--channel-id", config.channel_id, "Channel ID")->required();
  app.add_option("--role", config.role, "Role")
      ->check(CLI::IsMember({"sendonly", "recvonly", "sendrecv"}))
      ->required();
  app.add_option("--video-codec-type", config.video_codec_type,
                 "Video codec for send")
      ->check(CLI::IsMember({"", "VP8", "VP9", "AV1", "H264"}));
  add_optional_bool(app, "--multistream", config.multistream,
                    "Use multistream (default: none)");

  // SDL に関するオプション
  app.add_option("--width", config.width, "SDL window width");
  app.add_option("--height", config.height, "SDL window height");
  app.add_flag("--fullscreen", config.fullscreen);
  app.add_flag("--show-me", config.show_me);

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    exit(app.exit(e));
  }

  if (log_level != rtc::LS_NONE) {
    rtc::LogMessage::LogToDebug((rtc::LoggingSeverity)log_level);
    rtc::LogMessage::LogTimestamps();
    rtc::LogMessage::LogThreads();
  }

  auto sdlsample = sora::CreateSoraClient<SDLSample>(config);
  sdlsample->Run();

  return 0;
}
