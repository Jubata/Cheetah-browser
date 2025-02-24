// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/speech_recognizer.h"

#include "base/bind.h"
#include "base/strings/string16.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/speech_recognition_error.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"

namespace vr {

namespace {

// Length of timeout to cancel recognition if there's no speech heard.
static const int kNoSpeechTimeoutInSeconds = 5;

// Length of timeout to cancel recognition if no different results are received.
static const int kNoNewSpeechTimeoutInSeconds = 2;

// Invalid speech session.
static const int kInvalidSessionId = -1;

static content::SpeechRecognitionManager* g_manager_for_test = nullptr;

content::SpeechRecognitionManager* GetSpeechRecognitionManager() {
  if (g_manager_for_test)
    return g_manager_for_test;
  return content::SpeechRecognitionManager::GetInstance();
}

}  // namespace

// Not thread safe. This object can be created on any thread but must only be
// destroyed on IO thread. All of its functions are also IO thread only.
// Notes on the life time of this object:
// The life time of this object is tied to SpeechRecognizer object on browser UI
// thread. However, it doesn't assume SpeechRecognizer on Browser UI thread
// lives longer. So it is safe to delete SpeechRecognizer at any time on UI
// thread to cancel a speech recognition session.
class SpeechRecognizerOnIO : public content::SpeechRecognitionEventListener {
 public:
  SpeechRecognizerOnIO();
  ~SpeechRecognizerOnIO() override;

  void Start(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      const base::WeakPtr<IOBrowserUIInterface>& browser_ui,
      const std::string& locale,
      const std::string& auth_scope,
      const std::string& auth_token);

  void Stop();

  // Starts a timer for |timeout_seconds|. When the timer expires, will stop
  // capturing audio and get a final utterance from the recognition manager.
  void StartSpeechTimeout(int timeout_seconds);
  void SpeechTimeout();

  // Overidden from content::SpeechRecognitionEventListener:
  // These are always called on the IO thread.
  void OnRecognitionStart(int session_id) override;
  void OnRecognitionEnd(int session_id) override;
  void OnRecognitionResults(
      int session_id,
      const content::SpeechRecognitionResults& results) override;
  void OnRecognitionError(
      int session_id,
      const content::SpeechRecognitionError& error) override;
  void OnSoundStart(int session_id) override;
  void OnSoundEnd(int session_id) override;
  void OnAudioLevelsChange(int session_id,
                           float volume,
                           float noise_volume) override;
  void OnEnvironmentEstimationComplete(int session_id) override;
  void OnAudioStart(int session_id) override;
  void OnAudioEnd(int session_id) override;

  void SetTimerForTest(std::unique_ptr<base::Timer> speech_timer);

 private:
  void NotifyRecognitionStateChanged(SpeechRecognitionState new_state);

  // Only dereferenced from the UI thread, but copied on IO thread.
  base::WeakPtr<IOBrowserUIInterface> browser_ui_;

  // All remaining members only accessed from the IO thread.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  std::string locale_;
  std::unique_ptr<base::Timer> speech_timeout_;
  int session_;
  base::string16 last_result_str_;

  base::WeakPtrFactory<SpeechRecognizerOnIO> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognizerOnIO);
};

SpeechRecognizerOnIO::SpeechRecognizerOnIO()
    : speech_timeout_(new base::Timer(false, false)),
      session_(kInvalidSessionId),
      weak_factory_(this) {}

SpeechRecognizerOnIO::~SpeechRecognizerOnIO() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (GetSpeechRecognitionManager())
    GetSpeechRecognitionManager()->StopAudioCaptureForSession(session_);
}

void SpeechRecognizerOnIO::Start(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    const base::WeakPtr<IOBrowserUIInterface>& browser_ui,
    const std::string& locale,
    const std::string& auth_scope,
    const std::string& auth_token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(session_ == kInvalidSessionId)
      << "stop previous session before start new one";

  browser_ui_ = browser_ui;

  content::SpeechRecognitionSessionConfig config;
  config.language = locale;
  config.continuous = true;
  config.interim_results = true;
  config.max_hypotheses = 1;
  config.filter_profanities = true;
  config.url_request_context_getter = url_request_context_getter;
  config.event_listener = weak_factory_.GetWeakPtr();
  // kInvalidUniqueID is not a valid render process, so the speech permission
  // check allows the request through.
  config.initial_context.render_process_id =
      content::ChildProcessHost::kInvalidUniqueID;
  config.auth_scope = auth_scope;
  config.auth_token = auth_token;

  auto* speech_instance = GetSpeechRecognitionManager();
  if (!speech_instance)
    return;

  session_ = speech_instance->CreateSession(config);
  speech_instance->StartSession(session_);
}

void SpeechRecognizerOnIO::Stop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (session_ == kInvalidSessionId)
    return;

  if (GetSpeechRecognitionManager())
    GetSpeechRecognitionManager()->StopAudioCaptureForSession(session_);
  session_ = kInvalidSessionId;
  speech_timeout_->Stop();
  weak_factory_.InvalidateWeakPtrs();
}

void SpeechRecognizerOnIO::NotifyRecognitionStateChanged(
    SpeechRecognitionState new_state) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&IOBrowserUIInterface::OnSpeechRecognitionStateChanged,
                 browser_ui_, new_state));
}

void SpeechRecognizerOnIO::StartSpeechTimeout(int timeout_seconds) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  speech_timeout_->Start(FROM_HERE,
                         base::TimeDelta::FromSeconds(timeout_seconds),
                         base::Bind(&SpeechRecognizerOnIO::SpeechTimeout,
                                    weak_factory_.GetWeakPtr()));
}

void SpeechRecognizerOnIO::SpeechTimeout() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NotifyRecognitionStateChanged(SPEECH_RECOGNITION_END);
  Stop();
}

void SpeechRecognizerOnIO::OnRecognitionStart(int session_id) {
  NotifyRecognitionStateChanged(SPEECH_RECOGNITION_RECOGNIZING);
}

void SpeechRecognizerOnIO::OnRecognitionEnd(int session_id) {
  NotifyRecognitionStateChanged(SPEECH_RECOGNITION_END);
  Stop();
}

void SpeechRecognizerOnIO::OnRecognitionResults(
    int session_id,
    const content::SpeechRecognitionResults& results) {
  base::string16 result_str;
  size_t final_count = 0;
  // The number of results with |is_provisional| false. If |final_count| ==
  // results.size(), then all results are non-provisional and the recognition is
  // complete.
  for (const auto& result : results) {
    if (!result.is_provisional)
      final_count++;
    result_str += result.hypotheses[0].utterance;
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&IOBrowserUIInterface::OnSpeechResult, browser_ui_, result_str,
                 final_count == results.size()));

  if (result_str != last_result_str_) {
    StartSpeechTimeout(kNoNewSpeechTimeoutInSeconds);
  }

  last_result_str_ = result_str;
}

void SpeechRecognizerOnIO::OnRecognitionError(
    int session_id,
    const content::SpeechRecognitionError& error) {
  switch (error.code) {
    case content::SPEECH_RECOGNITION_ERROR_NETWORK:
      NotifyRecognitionStateChanged(SPEECH_RECOGNITION_NETWORK_ERROR);
      break;
    case content::SPEECH_RECOGNITION_ERROR_NO_SPEECH:
    case content::SPEECH_RECOGNITION_ERROR_NO_MATCH:
      NotifyRecognitionStateChanged(SPEECH_RECOGNITION_TRY_AGAIN);
      break;
    default:
      break;
  }
}

void SpeechRecognizerOnIO::OnSoundStart(int session_id) {
  StartSpeechTimeout(kNoSpeechTimeoutInSeconds);
  NotifyRecognitionStateChanged(SPEECH_RECOGNITION_IN_SPEECH);
}

void SpeechRecognizerOnIO::OnSoundEnd(int session_id) {}

void SpeechRecognizerOnIO::OnAudioLevelsChange(int session_id,
                                               float volume,
                                               float noise_volume) {
  // Both |volume| and |noise_volume| are defined to be in the range [0.0, 1.0].
  DCHECK_LE(0.0, volume);
  DCHECK_GE(1.0, volume);
  DCHECK_LE(0.0, noise_volume);
  DCHECK_GE(1.0, noise_volume);
  volume = std::max(0.0f, volume - noise_volume);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&IOBrowserUIInterface::OnSpeechSoundLevelChanged, browser_ui_,
                 volume));
}

void SpeechRecognizerOnIO::OnEnvironmentEstimationComplete(int session_id) {}

void SpeechRecognizerOnIO::OnAudioStart(int session_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NotifyRecognitionStateChanged(SPEECH_RECOGNITION_READY);
}

void SpeechRecognizerOnIO::OnAudioEnd(int session_id) {}

void SpeechRecognizerOnIO::SetTimerForTest(
    std::unique_ptr<base::Timer> speech_timer) {
  speech_timeout_ = std::move(speech_timer);
}

SpeechRecognizer::SpeechRecognizer(
    VoiceResultDelegate* delegate,
    BrowserUiInterface* ui,
    net::URLRequestContextGetter* url_request_context_getter,
    const std::string& locale)
    : delegate_(delegate),
      ui_(ui),
      url_request_context_getter_(url_request_context_getter),
      locale_(locale),
      speech_recognizer_on_io_(base::MakeUnique<SpeechRecognizerOnIO>()),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

SpeechRecognizer::~SpeechRecognizer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (speech_recognizer_on_io_) {
    content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                       speech_recognizer_on_io_.release());
  }
}

void SpeechRecognizer::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string auth_scope;
  std::string auth_token;
  GetSpeechAuthParameters(&auth_scope, &auth_token);

  // It is safe to use unretained because speech_recognizer_on_io_ only gets
  // deleted on IO thread when SpeechRecognizer is deleted.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&SpeechRecognizerOnIO::Start,
                     base::Unretained(speech_recognizer_on_io_.get()),
                     url_request_context_getter_, weak_factory_.GetWeakPtr(),
                     locale_, auth_scope, auth_token));
  if (ui_)
    ui_->SetSpeechRecognitionEnabled(true);
}

void SpeechRecognizer::Stop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  weak_factory_.InvalidateWeakPtrs();

  // It is safe to use unretained because speech_recognizer_on_io_ only gets
  // deleted on IO thread when SpeechRecognizer is deleted.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&SpeechRecognizerOnIO::Stop,
                     base::Unretained(speech_recognizer_on_io_.get())));
  if (ui_)
    ui_->SetSpeechRecognitionEnabled(false);
}

void SpeechRecognizer::OnSpeechResult(const base::string16& query,
                                      bool is_final) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!is_final)
    return;

  if (ui_)
    ui_->SetRecognitionResult(query);
  if (delegate_)
    delegate_->OnVoiceResults(query);
}

void SpeechRecognizer::OnSpeechSoundLevelChanged(float level) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(bshe): notify VR UI thread to draw UI.
}

void SpeechRecognizer::OnSpeechRecognitionStateChanged(
    SpeechRecognitionState new_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!ui_)
    return;
  ui_->OnSpeechRecognitionStateChanged(new_state);
  SpeechRecognitionState state = static_cast<SpeechRecognitionState>(new_state);
  switch (state) {
    case SPEECH_RECOGNITION_IN_SPEECH:
    case SPEECH_RECOGNITION_READY:
    case SPEECH_RECOGNITION_RECOGNIZING:
    case SPEECH_RECOGNITION_NETWORK_ERROR:
      break;
    case SPEECH_RECOGNITION_TRY_AGAIN:
      if (ui_) {
        ui_->SetRecognitionResult(
            l10n_util::GetStringUTF16(IDS_VR_NO_SPEECH_RECOGNITION_RESULT));
      }
      break;
    case SPEECH_RECOGNITION_OFF:
    case SPEECH_RECOGNITION_END:
      ui_->SetSpeechRecognitionEnabled(false);
      break;
  }
}

void SpeechRecognizer::GetSpeechAuthParameters(std::string* auth_scope,
                                               std::string* auth_token) {
  // TODO(bshe): audio history requires auth_scope and auto_token. Get real
  // value if we need to save audio history.
}

// static
void SpeechRecognizer::SetManagerForTest(
    content::SpeechRecognitionManager* manager) {
  g_manager_for_test = manager;
}

// static
void SpeechRecognizer::SetSpeechTimerForTest(
    std::unique_ptr<base::Timer> speech_timer) {
  if (!speech_recognizer_on_io_)
    return;
  speech_recognizer_on_io_->SetTimerForTest(std::move(speech_timer));
}

}  // namespace vr
