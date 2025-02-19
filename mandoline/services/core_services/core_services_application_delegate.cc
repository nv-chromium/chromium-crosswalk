// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/services/core_services/core_services_application_delegate.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/simple_thread.h"
#include "components/clipboard/clipboard_application_delegate.h"
#include "components/filesystem/file_system_app.h"
#include "mandoline/services/core_services/application_delegate_factory.h"
#include "mandoline/tab/web_view_application_delegate.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/services/tracing/tracing_app.h"
#include "url/gurl.h"

namespace core_services {

// A helper class for hosting a mojo::ApplicationImpl on its own thread.
class ApplicationThread : public base::SimpleThread {
 public:
  ApplicationThread(
      const base::WeakPtr<CoreServicesApplicationDelegate>
          core_services_application,
      const std::string& url,
      scoped_ptr<mojo::ApplicationDelegate> delegate,
      mojo::InterfaceRequest<mojo::Application> request)
      : base::SimpleThread(url),
        core_services_application_(core_services_application),
        core_services_application_task_runner_(
            base::MessageLoop::current()->task_runner()),
        url_(url),
        delegate_(delegate.Pass()),
        request_(request.Pass()) {
  }

  ~ApplicationThread() override {
    Join();
  }

  // Overridden from base::SimpleThread:
  void Run() override {
    scoped_ptr<mojo::ApplicationRunner> runner(
        new mojo::ApplicationRunner(delegate_.release()));
    if (url_ == "mojo://network_service/") {
      runner->set_message_loop_type(base::MessageLoop::TYPE_IO);
    } else if (url_ == "mojo://view_manager/") {
      runner->set_message_loop_type(base::MessageLoop::TYPE_UI);
    }
    runner->Run(request_.PassMessagePipe().release().value(), false);

    core_services_application_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CoreServicesApplicationDelegate::ApplicationThreadDestroyed,
                   core_services_application_,
                   this));
  }

 private:
  base::WeakPtr<CoreServicesApplicationDelegate> core_services_application_;
  scoped_refptr<base::SingleThreadTaskRunner>
      core_services_application_task_runner_;
  std::string url_;
  scoped_ptr<mojo::ApplicationDelegate> delegate_;
  mojo::InterfaceRequest<mojo::Application> request_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationThread);
};

CoreServicesApplicationDelegate::CoreServicesApplicationDelegate()
    : weak_factory_(this) {
}

CoreServicesApplicationDelegate::~CoreServicesApplicationDelegate() {
  application_threads_.clear();
}

void CoreServicesApplicationDelegate::ApplicationThreadDestroyed(
    ApplicationThread* thread) {
  ScopedVector<ApplicationThread>::iterator iter =
      std::find(application_threads_.begin(),
                application_threads_.end(),
                thread);
  DCHECK(iter != application_threads_.end());
  application_threads_.erase(iter);
}

bool CoreServicesApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService(this);
  return true;
}

void CoreServicesApplicationDelegate::Quit() {
  // This will delete all threads. This also performs a blocking join, waiting
  // for the threads to end.
  application_threads_.clear();
  weak_factory_.InvalidateWeakPtrs();
}

void CoreServicesApplicationDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::ContentHandler> request) {
  handler_bindings_.AddBinding(this, request.Pass());
}

void CoreServicesApplicationDelegate::StartApplication(
    mojo::InterfaceRequest<mojo::Application> request,
    mojo::URLResponsePtr response) {
  const std::string url = response->url;

  scoped_ptr<mojo::ApplicationDelegate> delegate;
  if (url == "mojo://clipboard/") {
    delegate.reset(new clipboard::ClipboardApplicationDelegate);
  } else if (url == "mojo://filesystem/") {
    delegate.reset(new filesystem::FileSystemApp);
  } else if (url == "mojo://tracing/") {
    delegate.reset(new tracing::TracingApp);
  } else if (url == "mojo://web_view/") {
    delegate.reset(new web_view::WebViewApplicationDelegate);
  } else {
#if defined(USE_AURA)
    delegate = CreateApplicationDelegateAura(url);
#endif
#if !defined(OS_ANDROID)
    if (!delegate)
      delegate = CreateApplicationDelegateNotAndroid(url);
#endif
    if (!delegate)
      delegate = CreatePlatformSpecificApplicationDelegate(url);

    if (!delegate)
      NOTREACHED() << "This application package does not support " << url;
  }

  scoped_ptr<ApplicationThread> thread(
      new ApplicationThread(weak_factory_.GetWeakPtr(), url, delegate.Pass(),
                            request.Pass()));
  thread->Start();
  application_threads_.push_back(thread.Pass());
}

}  // namespace core_services
