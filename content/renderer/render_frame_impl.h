// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
#define CONTENT_RENDERER_RENDER_FRAME_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "content/common/accessibility_mode_enums.h"
#include "content/common/frame_message_enums.h"
#include "content/common/mojo/service_registry_impl.h"
#include "content/public/common/console_message_level.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/renderer_webcookiejar_impl.h"
#include "ipc/ipc_message.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_params.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerClient.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebHistoryCommitType.h"
#include "third_party/WebKit/public/web/WebScriptExecutionCallback.h"
#include "ui/gfx/range/range.h"

#if defined(ENABLE_PLUGINS)
#include "content/renderer/pepper/plugin_power_saver_helper.h"
#endif

#if defined(OS_ANDROID)
#include "content/renderer/media/android/renderer_media_player_manager.h"
#endif

#if defined(ENABLE_MOJO_MEDIA)
#include "media/mojo/interfaces/service_factory.mojom.h"
#endif

class GURL;
class TransportDIB;
struct FrameMsg_NewFrame_WidgetParams;
struct FrameMsg_PostMessage_Params;
struct FrameMsg_TextTrackSettings_Params;

namespace blink {
class WebGeolocationClient;
class WebMouseEvent;
class WebContentDecryptionModule;
class WebMediaPlayer;
class WebPresentationClient;
class WebPushClient;
class WebSecurityOrigin;
struct WebCompositionUnderline;
struct WebContextMenuData;
struct WebCursorInfo;
}

namespace gfx {
class Point;
class Range;
class Rect;
}

namespace media {
class CdmFactory;
class MediaPermission;
class WebEncryptedMediaClientImpl;
}

namespace mojo {
class ServiceProvider;
}

namespace content {

class ChildFrameCompositingHelper;
class CompositorDependencies;
class DevToolsAgent;
class DocumentState;
class ExternalPopupMenu;
class GeolocationDispatcher;
class ManifestManager;
class MediaStreamDispatcher;
class MediaStreamRendererFactory;
class MediaPermissionDispatcher;
class MidiDispatcher;
class NavigationState;
class NotificationPermissionDispatcher;
class PageState;
class PepperPluginInstanceImpl;
class PermissionDispatcher;
class PresentationDispatcher;
class PushMessagingDispatcher;
class RendererAccessibility;
class RendererCdmManager;
class RendererMediaPlayerManager;
class RendererPpapiHost;
class RenderFrameObserver;
class RenderViewImpl;
class RenderWidget;
class RenderWidgetFullscreenPepper;
class ScreenOrientationDispatcher;
class UserMediaClientImpl;
struct CommonNavigationParams;
struct CustomContextMenuContext;
struct FrameReplicationState;
struct NavigationParams;
struct RequestNavigationParams;
struct ResourceResponseHead;
struct StartNavigationParams;
struct StreamOverrideParameters;
class VRDispatcher;

class CONTENT_EXPORT RenderFrameImpl
    : public RenderFrame,
      NON_EXPORTED_BASE(public blink::WebFrameClient),
      NON_EXPORTED_BASE(public media::WebMediaPlayerDelegate) {
 public:
  // Creates a new RenderFrame. |render_view| is the RenderView object that this
  // frame belongs to.
  // Callers *must* call |SetWebFrame| immediately after creation.
  // Note: This is called only when RenderFrame is created by Blink through
  // createChildFrame.
  // TODO(creis): We should structure this so that |SetWebFrame| isn't needed.
  static RenderFrameImpl* Create(RenderViewImpl* render_view, int32 routing_id);

  // Creates a new RenderFrame with |routing_id| as a child of the RenderFrame
  // identified by |parent_routing_id| or as the top-level frame if the latter
  // is MSG_ROUTING_NONE. If |proxy_routing_id| is MSG_ROUTING_NONE, it creates
  // the Blink WebLocalFrame and inserts it into the frame tree after the frame
  // identified by |previous_sibling_routing_id|, or as the first child if
  // |previous_sibling_routing_id| is MSG_ROUTING_NONE. Otherwise, the frame is
  // semi-orphaned until it commits, at which point it replaces the proxy
  // identified by |proxy_routing_id|. Note: This is called only when
  // RenderFrame is being created in response to IPC message from the browser
  // process. All other frame creation is driven through Blink and Create.
  static void CreateFrame(int routing_id,
                          int parent_routing_id,
                          int previous_sibling_routing_id,
                          int proxy_routing_id,
                          const FrameReplicationState& replicated_state,
                          CompositorDependencies* compositor_deps,
                          const FrameMsg_NewFrame_WidgetParams& params);

  // Returns the RenderFrameImpl for the given routing ID.
  static RenderFrameImpl* FromRoutingID(int routing_id);

  // Just like RenderFrame::FromWebFrame but returns the implementation.
  static RenderFrameImpl* FromWebFrame(blink::WebFrame* web_frame);

  // Used by content_layouttest_support to hook into the creation of
  // RenderFrameImpls.
  struct CreateParams {
    CreateParams(RenderViewImpl* render_view, int32 routing_id)
        : render_view(render_view), routing_id(routing_id) {}
    ~CreateParams() {}

    RenderViewImpl* render_view;
    int32 routing_id;
  };

  using CreateRenderFrameImplFunction =
      RenderFrameImpl* (*)(const CreateParams&);
  static void InstallCreateHook(
      CreateRenderFrameImplFunction create_render_frame_impl);

  virtual ~RenderFrameImpl();

  bool is_swapped_out() const {
    return is_swapped_out_;
  }

  // TODO(nasko): This can be removed once we don't have a swapped out state on
  // RenderFrames. See https://crbug.com/357747.
  void set_render_frame_proxy(RenderFrameProxy* proxy) {
    render_frame_proxy_ = proxy;
  }

  // Out-of-process child frames receive a signal from RenderWidgetCompositor
  // when a compositor frame has committed.
  void DidCommitCompositorFrame();

  // TODO(jam): this is a temporary getter until all the code is transitioned
  // to using RenderFrame instead of RenderView.
  RenderViewImpl* render_view() { return render_view_.get(); }

  RendererWebCookieJarImpl* cookie_jar() { return &cookie_jar_; }

  // Returns the RenderWidget associated with this frame.
  RenderWidget* GetRenderWidget();

  DevToolsAgent* devtools_agent() { return devtools_agent_; }

  // This is called right after creation with the WebLocalFrame for this
  // RenderFrame. It must be called before Initialize.
  void SetWebFrame(blink::WebLocalFrame* web_frame);

  // This method must be called after the frame has been added to the frame
  // tree. It creates all objects that depend on the frame being at its proper
  // spot.
  void Initialize();

  // Notifications from RenderWidget.
  void WasHidden();
  void WasShown();
  void WidgetWillClose();

  // Start/Stop loading notifications.
  // TODO(nasko): Those are page-level methods at this time and come from
  // WebViewClient. We should move them to be WebFrameClient calls and put
  // logic in the browser side to balance starts/stops.
  // |to_different_document| will be true unless the load is a fragment
  // navigation, or triggered by history.pushState/replaceState.
  virtual void didStartLoading(bool to_different_document);
  virtual void didStopLoading();
  virtual void didChangeLoadProgress(double load_progress);

  AccessibilityMode accessibility_mode() {
    return accessibility_mode_;
  }

  RendererAccessibility* renderer_accessibility() {
    return renderer_accessibility_;
  }

  void HandleWebAccessibilityEvent(const blink::WebAXObject& obj,
                                   blink::WebAXEvent event);

  // The focused node changed to |node|. If focus was lost from this frame,
  // |node| will be null.
  void FocusedNodeChanged(const blink::WebNode& node);

  // TODO(dmazzoni): the only reason this is here is to plumb it through to
  // RendererAccessibility. It should use the RenderFrameObserver method, once
  // blink has a separate accessibility tree per frame.
  void FocusedNodeChangedForAccessibility(const blink::WebNode& node);

#if defined(ENABLE_PLUGINS)
  // Notification that a PPAPI plugin has been created.
  void PepperPluginCreated(RendererPpapiHost* host);

  // Notifies that |instance| has changed the cursor.
  // This will update the cursor appearance if it is currently over the plugin
  // instance.
  void PepperDidChangeCursor(PepperPluginInstanceImpl* instance,
                             const blink::WebCursorInfo& cursor);

  // Notifies that |instance| has received a mouse event.
  void PepperDidReceiveMouseEvent(PepperPluginInstanceImpl* instance);

  // Informs the render view that a PPAPI plugin has changed text input status.
  void PepperTextInputTypeChanged(PepperPluginInstanceImpl* instance);
  void PepperCaretPositionChanged(PepperPluginInstanceImpl* instance);

  // Cancels current composition.
  void PepperCancelComposition(PepperPluginInstanceImpl* instance);

  // Informs the render view that a PPAPI plugin has changed selection.
  void PepperSelectionChanged(PepperPluginInstanceImpl* instance);

  // Creates a fullscreen container for a pepper plugin instance.
  RenderWidgetFullscreenPepper* CreatePepperFullscreenContainer(
      PepperPluginInstanceImpl* plugin);

  bool IsPepperAcceptingCompositionEvents() const;

  // Notification that the given plugin has crashed.
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid);

  // Simulates IME events for testing purpose.
  void SimulateImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  void SimulateImeConfirmComposition(const base::string16& text,
                                     const gfx::Range& replacement_range);

  // TODO(jam): remove these once the IPC handler moves from RenderView to
  // RenderFrame.
  void OnImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  void OnImeConfirmComposition(const base::string16& text,
                               const gfx::Range& replacement_range,
                               bool keep_selection);
#endif  // defined(ENABLE_PLUGINS)

  // May return NULL in some cases, especially if userMediaClient() returns
  // NULL.
  MediaStreamDispatcher* GetMediaStreamDispatcher();

#if defined(OS_MACOSX) || defined(OS_ANDROID)
  void DidHideExternalPopupMenu();
#endif

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

  // RenderFrame implementation:
  RenderView* GetRenderView() override;
  int GetRoutingID() override;
  blink::WebLocalFrame* GetWebFrame() override;
  blink::WebElement GetFocusedElement() const override;
  WebPreferences& GetWebkitPreferences() override;
  int ShowContextMenu(ContextMenuClient* client,
                      const ContextMenuParams& params) override;
  void CancelContextMenu(int request_id) override;
  blink::WebNode GetContextMenuNode() const override;
  blink::WebPlugin* CreatePlugin(
      blink::WebFrame* frame,
      const WebPluginInfo& info,
      const blink::WebPluginParams& params,
      scoped_ptr<PluginInstanceThrottler> throttler) override;
  void LoadURLExternally(blink::WebLocalFrame* frame,
                         const blink::WebURLRequest& request,
                         blink::WebNavigationPolicy policy) override;
  void ExecuteJavaScript(const base::string16& javascript) override;
  bool IsHidden() override;
  ServiceRegistry* GetServiceRegistry() override;
#if defined(ENABLE_PLUGINS)
  void RegisterPeripheralPlugin(
      const GURL& content_origin,
      const base::Closure& unthrottle_callback) override;
  PluginPowerSaverHelper* plugin_power_saver_helper() {
    return plugin_power_saver_helper_;
  }
#endif
  bool IsFTPDirectoryListing() override;
  void AttachGuest(int element_instance_id) override;
  void DetachGuest(int element_instance_id) override;
  void SetSelectedText(const base::string16& selection_text,
                       size_t offset,
                       const gfx::Range& range) override;
  void EnsureMojoBuiltinsAreAvailable(v8::Isolate* isolate,
                                      v8::Local<v8::Context> context) override;
  void AddMessageToConsole(ConsoleMessageLevel level,
                           const std::string& message) override;

  // blink::WebFrameClient implementation:
  blink::WebPluginPlaceholder* createPluginPlaceholder(
      blink::WebLocalFrame*,
      const blink::WebPluginParams&) override;
  virtual blink::WebPlugin* createPlugin(blink::WebLocalFrame* frame,
                                         const blink::WebPluginParams& params);
  virtual blink::WebMediaPlayer* createMediaPlayer(
      blink::WebLocalFrame* frame,
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      blink::WebContentDecryptionModule* initial_cdm);
  virtual blink::WebApplicationCacheHost* createApplicationCacheHost(
      blink::WebLocalFrame* frame,
      blink::WebApplicationCacheHostClient* client);
  virtual blink::WebWorkerContentSettingsClientProxy*
      createWorkerContentSettingsClientProxy(blink::WebLocalFrame* frame);
  virtual blink::WebExternalPopupMenu* createExternalPopupMenu(
      const blink::WebPopupMenuInfo& popup_menu_info,
      blink::WebExternalPopupMenuClient* popup_menu_client);
  virtual blink::WebCookieJar* cookieJar(blink::WebLocalFrame* frame);
  virtual blink::WebServiceWorkerProvider* createServiceWorkerProvider(
      blink::WebLocalFrame* frame);
  virtual void didAccessInitialDocument(blink::WebLocalFrame* frame);
  virtual blink::WebFrame* createChildFrame(
      blink::WebLocalFrame* parent,
      blink::WebTreeScopeType scope,
      const blink::WebString& name,
      blink::WebSandboxFlags sandboxFlags);
  virtual void didDisownOpener(blink::WebLocalFrame* frame);
  virtual void frameDetached(blink::WebFrame* frame, DetachType type);
  virtual void frameFocused();
  virtual void willClose(blink::WebFrame* frame);
  virtual void didChangeName(blink::WebLocalFrame* frame,
                             const blink::WebString& name);
  virtual void didChangeSandboxFlags(blink::WebFrame* child_frame,
                                     blink::WebSandboxFlags flags);
  virtual void didMatchCSS(
      blink::WebLocalFrame* frame,
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors);
  virtual bool shouldReportDetailedMessageForSource(
      const blink::WebString& source);
  virtual void didAddMessageToConsole(const blink::WebConsoleMessage& message,
                                      const blink::WebString& source_name,
                                      unsigned source_line,
                                      const blink::WebString& stack_trace);
  virtual void loadURLExternally(blink::WebLocalFrame* frame,
                                 const blink::WebURLRequest& request,
                                 blink::WebNavigationPolicy policy,
                                 const blink::WebString& suggested_name);
  // The WebDataSource::ExtraData* is assumed to be a DocumentState* subclass.
  virtual blink::WebNavigationPolicy decidePolicyForNavigation(
      const NavigationPolicyInfo& info);
  virtual blink::WebHistoryItem historyItemForNewChildFrame(
      blink::WebFrame* frame);
  virtual void willSendSubmitEvent(blink::WebLocalFrame* frame,
                                   const blink::WebFormElement& form);
  virtual void willSubmitForm(blink::WebLocalFrame* frame,
                              const blink::WebFormElement& form);
  virtual void didCreateDataSource(blink::WebLocalFrame* frame,
                                   blink::WebDataSource* datasource);
  virtual void didStartProvisionalLoad(blink::WebLocalFrame* frame,
                                       double triggering_event_time);
  virtual void didReceiveServerRedirectForProvisionalLoad(
      blink::WebLocalFrame* frame);
  virtual void didFailProvisionalLoad(
      blink::WebLocalFrame* frame,
      const blink::WebURLError& error,
      blink::WebHistoryCommitType commit_type);
  virtual void didCommitProvisionalLoad(
      blink::WebLocalFrame* frame,
      const blink::WebHistoryItem& item,
      blink::WebHistoryCommitType commit_type);
  virtual void didCreateNewDocument(blink::WebLocalFrame* frame);
  virtual void didClearWindowObject(blink::WebLocalFrame* frame);
  virtual void didCreateDocumentElement(blink::WebLocalFrame* frame);
  virtual void didReceiveTitle(blink::WebLocalFrame* frame,
                               const blink::WebString& title,
                               blink::WebTextDirection direction);
  virtual void didChangeIcon(blink::WebLocalFrame* frame,
                             blink::WebIconURL::Type icon_type);
  virtual void didFinishDocumentLoad(blink::WebLocalFrame* frame,
                                     bool document_is_empty);
  virtual void didHandleOnloadEvents(blink::WebLocalFrame* frame);
  virtual void didFailLoad(blink::WebLocalFrame* frame,
                           const blink::WebURLError& error,
                           blink::WebHistoryCommitType commit_type);
  virtual void didFinishLoad(blink::WebLocalFrame* frame);
  virtual void didNavigateWithinPage(blink::WebLocalFrame* frame,
                                     const blink::WebHistoryItem& item,
                                     blink::WebHistoryCommitType commit_type);
  virtual void didUpdateCurrentHistoryItem(blink::WebLocalFrame* frame);
  virtual void didChangeThemeColor();
  virtual void dispatchLoad();
  virtual void requestNotificationPermission(
      const blink::WebSecurityOrigin& origin,
      blink::WebNotificationPermissionCallback* callback);
  virtual void didChangeSelection(bool is_empty_selection);
  virtual blink::WebColorChooser* createColorChooser(
      blink::WebColorChooserClient* client,
      const blink::WebColor& initial_color,
      const blink::WebVector<blink::WebColorSuggestion>& suggestions);
  virtual void runModalAlertDialog(const blink::WebString& message);
  virtual bool runModalConfirmDialog(const blink::WebString& message);
  virtual bool runModalPromptDialog(const blink::WebString& message,
                                    const blink::WebString& default_value,
                                    blink::WebString* actual_value);
  virtual bool runModalBeforeUnloadDialog(bool is_reload,
                                          const blink::WebString& message);
  virtual void showContextMenu(const blink::WebContextMenuData& data);
  virtual void clearContextMenu();
  virtual void willSendRequest(blink::WebLocalFrame* frame,
                               unsigned identifier,
                               blink::WebURLRequest& request,
                               const blink::WebURLResponse& redirect_response);
  virtual void didReceiveResponse(blink::WebLocalFrame* frame,
                                  unsigned identifier,
                                  const blink::WebURLResponse& response);
  virtual void didLoadResourceFromMemoryCache(
      blink::WebLocalFrame* frame,
      const blink::WebURLRequest& request,
      const blink::WebURLResponse& response);
  virtual void didDisplayInsecureContent(blink::WebLocalFrame* frame);
  virtual void didRunInsecureContent(blink::WebLocalFrame* frame,
                                     const blink::WebSecurityOrigin& origin,
                                     const blink::WebURL& target);
  virtual void didAbortLoading(blink::WebLocalFrame* frame);
  virtual void didCreateScriptContext(blink::WebLocalFrame* frame,
                                      v8::Local<v8::Context> context,
                                      int extension_group,
                                      int world_id);
  virtual void willReleaseScriptContext(blink::WebLocalFrame* frame,
                                        v8::Local<v8::Context> context,
                                        int world_id);
  virtual void didFirstVisuallyNonEmptyLayout(blink::WebLocalFrame* frame);
  virtual void didChangeScrollOffset(blink::WebLocalFrame* frame);
  virtual void willInsertBody(blink::WebLocalFrame* frame);
  virtual void reportFindInPageMatchCount(int request_id,
                                          int count,
                                          bool final_update);
  virtual void reportFindInPageSelection(int request_id,
                                         int active_match_ordinal,
                                         const blink::WebRect& sel);
  virtual void requestStorageQuota(blink::WebLocalFrame* frame,
                                   blink::WebStorageQuotaType type,
                                   unsigned long long requested_size,
                                   blink::WebStorageQuotaCallbacks callbacks);
  virtual void willOpenWebSocket(blink::WebSocketHandle* handle);
  virtual blink::WebGeolocationClient* geolocationClient();
  virtual blink::WebPushClient* pushClient();
  virtual blink::WebPresentationClient* presentationClient();
  virtual void willStartUsingPeerConnectionHandler(
      blink::WebLocalFrame* frame,
      blink::WebRTCPeerConnectionHandler* handler);
  virtual blink::WebUserMediaClient* userMediaClient();
  virtual blink::WebEncryptedMediaClient* encryptedMediaClient();
  virtual blink::WebMIDIClient* webMIDIClient();
  virtual bool willCheckAndDispatchMessageEvent(
      blink::WebLocalFrame* source_frame,
      blink::WebFrame* target_frame,
      blink::WebSecurityOrigin target_origin,
      blink::WebDOMMessageEvent event);
  virtual blink::WebString userAgentOverride(blink::WebLocalFrame* frame,
                                             const blink::WebURL& url);
  virtual blink::WebString doNotTrackValue(blink::WebLocalFrame* frame);
  virtual bool allowWebGL(blink::WebLocalFrame* frame, bool default_value);
  virtual void didLoseWebGLContext(blink::WebLocalFrame* frame,
                                   int arb_robustness_status_code);
  virtual blink::WebScreenOrientationClient* webScreenOrientationClient();
  virtual bool isControlledByServiceWorker(blink::WebDataSource& data_source);
  virtual int64_t serviceWorkerID(blink::WebDataSource& data_source);
  virtual void postAccessibilityEvent(const blink::WebAXObject& obj,
                                      blink::WebAXEvent event);
  virtual void handleAccessibilityFindInPageResult(
        int identifier,
        int match_index,
        const blink::WebAXObject& start_object,
        int start_offset,
        const blink::WebAXObject& end_object,
        int end_offset);
  virtual void didChangeManifest(blink::WebLocalFrame*);
  virtual bool enterFullscreen();
  virtual bool exitFullscreen();
  virtual blink::WebPermissionClient* permissionClient();
  virtual blink::WebAppBannerClient* appBannerClient();
  virtual void registerProtocolHandler(const blink::WebString& scheme,
                                       const blink::WebURL& url,
                                       const blink::WebString& title);
  virtual void unregisterProtocolHandler(const blink::WebString& scheme,
                                         const blink::WebURL& url);
  virtual blink::WebBluetooth* bluetooth();
  virtual blink::WebUSBClient* usbClient();

#if defined(ENABLE_WEBVR)
  blink::WebVRClient* webVRClient() override;
#endif

  // WebMediaPlayerDelegate implementation:
  void DidPlay(blink::WebMediaPlayer* player) override;
  void DidPause(blink::WebMediaPlayer* player) override;
  void PlayerGone(blink::WebMediaPlayer* player) override;

  // Make this frame show an empty, unscriptable page.
  // TODO(nasko): Remove this method once swapped out state is no longer used.
  void NavigateToSwappedOutURL();

  // Binds this render frame's service registry.
  void BindServiceRegistry(
      mojo::InterfaceRequest<mojo::ServiceProvider> services,
      mojo::ServiceProviderPtr exposed_services);

  ManifestManager* manifest_manager();

  // TODO(creis): Remove when the only caller, the HistoryController, is no
  // more.
  void SetPendingNavigationParams(
      scoped_ptr<NavigationParams> navigation_params);

 protected:
  explicit RenderFrameImpl(const CreateParams& params);

 private:
  friend class RenderFrameImplTest;
  friend class RenderFrameObserver;
  friend class RendererAccessibilityTest;
  friend class TestRenderFrame;
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuDisplayNoneTest, SelectItem);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuRemoveTest, RemoveOnChange);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuTest, NormalCase);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuTest, ShowPopupThenNavigate);
  FRIEND_TEST_ALL_PREFIXES(RendererAccessibilityTest,
                           AccessibilityMessagesQueueWhileSwappedOut);

  // A wrapper class used as the callback for JavaScript executed
  // in an isolated world.
  class JavaScriptIsolatedWorldRequest
      : public blink::WebScriptExecutionCallback {
   public:
    JavaScriptIsolatedWorldRequest(
        int id,
        bool notify_result,
        int routing_id,
        base::WeakPtr<RenderFrameImpl> render_frame_impl);
    void completed(
        const blink::WebVector<v8::Local<v8::Value>>& result) override;

   private:
    ~JavaScriptIsolatedWorldRequest();

    int id_;
    bool notify_result_;
    int routing_id_;
    base::WeakPtr<RenderFrameImpl> render_frame_impl_;

    DISALLOW_COPY_AND_ASSIGN(JavaScriptIsolatedWorldRequest);
  };

  typedef std::map<GURL, double> HostZoomLevels;

  // Functions to add and remove observers for this object.
  void AddObserver(RenderFrameObserver* observer);
  void RemoveObserver(RenderFrameObserver* observer);

  // Builds and sends DidCommitProvisionalLoad to the host.
  void SendDidCommitProvisionalLoad(blink::WebFrame* frame,
                                    blink::WebHistoryCommitType commit_type,
                                    const blink::WebHistoryItem& item);

  // IPC message handlers ------------------------------------------------------
  //
  // The documentation for these functions should be in
  // content/common/*_messages.h for the message that the function is handling.
  void OnNavigate(const CommonNavigationParams& common_params,
                  const StartNavigationParams& start_params,
                  const RequestNavigationParams& request_params);
  void OnBeforeUnload();
  void OnSwapOut(int proxy_routing_id,
                 bool is_loading,
                 const FrameReplicationState& replicated_frame_state);
  void OnStop();
  void OnShowContextMenu(const gfx::Point& location);
  void OnContextMenuClosed(const CustomContextMenuContext& custom_context);
  void OnCustomContextMenuAction(const CustomContextMenuContext& custom_context,
                                 unsigned action);
  void OnUndo();
  void OnRedo();
  void OnCut();
  void OnCopy();
  void OnPaste();
  void OnPasteAndMatchStyle();
  void OnDelete();
  void OnSelectAll();
  void OnSelectRange(const gfx::Point& base, const gfx::Point& extent);
  void OnAdjustSelectionByCharacterOffset(int start_adjust, int end_adjust);
  void OnUnselect();
  void OnMoveRangeSelectionExtent(const gfx::Point& point);
  void OnReplace(const base::string16& text);
  void OnReplaceMisspelling(const base::string16& text);
  void OnCSSInsertRequest(const std::string& css);
  void OnAddMessageToConsole(ConsoleMessageLevel level,
                             const std::string& message);
  void OnJavaScriptExecuteRequest(const base::string16& javascript,
                                  int id,
                                  bool notify_result);
  void OnJavaScriptExecuteRequestForTests(const base::string16& javascript,
                                          int id,
                                          bool notify_result,
                                          bool has_user_gesture);
  void OnJavaScriptExecuteRequestInIsolatedWorld(const base::string16& jscript,
                                                 int id,
                                                 bool notify_result,
                                                 int world_id);
  void OnVisualStateRequest(uint64 key);
  void OnSetEditableSelectionOffsets(int start, int end);
  void OnSetCompositionFromExistingText(
      int start, int end,
      const std::vector<blink::WebCompositionUnderline>& underlines);
  void OnExecuteNoValueEditCommand(const std::string& name);
  void OnExtendSelectionAndDelete(int before, int after);
  void OnReload(bool ignore_cache);
  void OnTextSurroundingSelectionRequest(size_t max_length);
  void OnSetAccessibilityMode(AccessibilityMode new_mode);
  void OnSnapshotAccessibilityTree(int callback_id);
  void OnDisownOpener();
  void OnDidUpdateSandboxFlags(blink::WebSandboxFlags flags);
  void OnTextTrackSettingsChanged(
      const FrameMsg_TextTrackSettings_Params& params);
  void OnPostMessageEvent(const FrameMsg_PostMessage_Params& params);
#if defined(OS_ANDROID)
  void OnSelectPopupMenuItems(bool canceled,
                              const std::vector<int>& selected_indices);
#elif defined(OS_MACOSX)
  void OnSelectPopupMenuItem(int selected_index);
  void OnCopyToFindPboard();
#endif

  void OnCommitNavigation(const ResourceResponseHead& response,
                          const GURL& stream_url,
                          const CommonNavigationParams& common_params,
                          const RequestNavigationParams& request_params);
  void OnFailedNavigation(const CommonNavigationParams& common_params,
                          const RequestNavigationParams& request_params,
                          bool has_stale_copy_in_cache,
                          int error_code);

  // Virtual since overridden by WebTestProxy for layout tests.
  virtual blink::WebNavigationPolicy DecidePolicyForNavigation(
      RenderFrame* render_frame,
      const NavigationPolicyInfo& info);
  void OpenURL(blink::WebFrame* frame,
               const GURL& url,
               const Referrer& referrer,
               blink::WebNavigationPolicy policy);

  // Performs a navigation in the frame. This provides a unified function for
  // the current code path and the browser-side navigation path (in
  // development). Currently used by OnNavigate, with all *NavigationParams
  // provided by the browser. |stream_params| should be null.
  // PlzNavigate: used by OnCommitNavigation, with |common_params| and
  // |request_params| received by the browser. |stream_params| should be non
  // null and created from the information provided by the browser.
  // |start_params| is not used.
  void NavigateInternal(const CommonNavigationParams& common_params,
                        const StartNavigationParams& start_params,
                        const RequestNavigationParams& request_params,
                        scoped_ptr<StreamOverrideParameters> stream_params);

  // Update current main frame's encoding and send it to browser window.
  // Since we want to let users see the right encoding info from menu
  // before finishing loading, we call the UpdateEncoding in
  // a) function:DidCommitLoadForFrame. When this function is called,
  // that means we have got first data. In here we try to get encoding
  // of page if it has been specified in http header.
  // b) function:DidReceiveTitle. When this function is called,
  // that means we have got specified title. Because in most of webpages,
  // title tags will follow meta tags. In here we try to get encoding of
  // page if it has been specified in meta tag.
  // c) function:DidFinishDocumentLoadForFrame. When this function is
  // called, that means we have got whole html page. In here we should
  // finally get right encoding of page.
  void UpdateEncoding(blink::WebFrame* frame,
                      const std::string& encoding_name);

  // Dispatches the current state of selection on the webpage to the browser if
  // it has changed.
  // TODO(varunjain): delete this method once we figure out how to keep
  // selection handles in sync with the webpage.
  void SyncSelectionIfRequired();

  bool RunJavaScriptMessage(JavaScriptMessageType type,
                            const base::string16& message,
                            const base::string16& default_value,
                            const GURL& frame_url,
                            base::string16* result);

  // Loads the appropriate error page for the specified failure into the frame.
  void LoadNavigationErrorPage(const blink::WebURLRequest& failed_request,
                               const blink::WebURLError& error,
                               bool replace);

  void HandleJavascriptExecutionResult(const base::string16& javascript,
                                       int id,
                                       bool notify_result,
                                       v8::Local<v8::Value> result);

  // Initializes |web_user_media_client_|. If this fails, because it wasn't
  // possible to create a MediaStreamClient (e.g., WebRTC is disabled), then
  // |web_user_media_client_| will remain NULL.
  void InitializeUserMediaClient();

  blink::WebMediaPlayer* CreateWebMediaPlayerForMediaStream(
      blink::WebMediaPlayerClient* client);

  // Creates a factory object used for creating audio and video renderers.
  scoped_ptr<MediaStreamRendererFactory> CreateRendererFactory();

  // Does preparation for the navigation to |url|.
  void PrepareRenderViewForNavigation(
      const GURL& url,
      const RequestNavigationParams& request_params,
      bool* is_reload,
      blink::WebURLRequest::CachePolicy* cache_policy);

  // PlzNavigate
  // Sends a FrameHostMsg_BeginNavigation to the browser based on the contents
  // of the WebURLRequest.
  void BeginNavigation(blink::WebURLRequest* request);

  // Loads a data url.
  void LoadDataURL(const CommonNavigationParams& params,
                   blink::WebFrame* frame);

  // Sends a proper FrameHostMsg_DidFailProvisionalLoadWithError_Params IPC for
  // the failed request |request|.
  void SendFailedProvisionalLoad(const blink::WebURLRequest& request,
                                 const blink::WebURLError& error,
                                 blink::WebLocalFrame* frame);

  bool ShouldDisplayErrorPageForFailedLoad(int error_code,
                                           const GURL& unreachable_url);

  // Returns the URL being loaded by the |frame_|'s request.
  GURL GetLoadingUrl() const;

  // If we initiated a navigation, this function will populate |document_state|
  // with the navigation information saved in OnNavigate().
  void PopulateDocumentStateFromPending(DocumentState* document_state);

  // Returns a new NavigationState populated with the navigation information
  // saved in OnNavigate().
  NavigationState* CreateNavigationStateFromPending();

#if defined(OS_ANDROID)
  blink::WebMediaPlayer* CreateAndroidWebMediaPlayer(
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      const media::WebMediaPlayerParams& params);

  RendererMediaPlayerManager* GetMediaPlayerManager();
#endif

  bool AreSecureCodecsSupported();

  media::MediaPermission* GetMediaPermission();

#if defined(ENABLE_MOJO_MEDIA)
  media::interfaces::ServiceFactory* GetMediaServiceFactory();

  // Called when a connection error happened on |media_service_factory_|.
  void OnMediaServiceFactoryConnectionError();
#endif

  media::CdmFactory* GetCdmFactory();

  void RegisterMojoServices();

  // Connects to a Mojo application and returns a proxy to its exposed
  // ServiceProvider.
  mojo::ServiceProviderPtr ConnectToApplication(const GURL& url);

  // Stores the WebLocalFrame we are associated with.  This is null from the
  // constructor until SetWebFrame is called, and it is null after
  // frameDetached is called until destruction (which is asynchronous in the
  // case of the main frame, but not subframes).
  blink::WebLocalFrame* frame_;

  // Boolean value indicating whether this RenderFrameImpl object is for a
  // subframe or not. It remains accurate during destruction, even when |frame_|
  // has been invalidated.
  bool is_subframe_;

  // Frame is a local root if it is rendered in a process different than parent
  // or it is a main frame.
  bool is_local_root_;

  base::WeakPtr<RenderViewImpl> render_view_;
  int routing_id_;
  bool is_swapped_out_;

  // RenderFrameProxy exists only when is_swapped_out_ is true.
  // TODO(nasko): This can be removed once we don't have a swapped out state on
  // RenderFrame. See https://crbug.com/357747.
  RenderFrameProxy* render_frame_proxy_;
  bool is_detaching_;

  // If this frame was created to replace a proxy, this will store the routing
  // id of the proxy to replace at commit-time, at which time it will be
  // cleared.
  // TODO(creis): Remove this after switching to PlzNavigate.
  int proxy_routing_id_;

  // Used when the RenderFrame is a local root. For now, RenderWidgets are
  // added only when a child frame is in a different process from its parent
  // frame, but eventually this will also apply to top-level frames.
  // TODO(kenrb): Correct the above statement when top-level frames have their
  // own RenderWidgets.
  scoped_refptr<RenderWidget> render_widget_;

  // Temporarily holds state pertaining to a navigation that has been initiated
  // until the NavigationState corresponding to the new navigation is created in
  // didCreateDataSource().
  scoped_ptr<NavigationParams> pending_navigation_params_;

#if defined(ENABLE_PLUGINS)
  // Current text input composition text. Empty if no composition is in
  // progress.
  base::string16 pepper_composition_text_;

  PluginPowerSaverHelper* plugin_power_saver_helper_;
#endif

  RendererWebCookieJarImpl cookie_jar_;

  // All the registered observers.
  base::ObserverList<RenderFrameObserver> observers_;

  scoped_refptr<ChildFrameCompositingHelper> compositing_helper_;

  // The node that the context menu was pressed over.
  blink::WebNode context_menu_node_;

  // External context menu requests we're waiting for. "Internal"
  // (WebKit-originated) context menu events will have an ID of 0 and will not
  // be in this map.
  //
  // We don't want to add internal ones since some of the "special" page
  // handlers in the browser process just ignore the context menu requests so
  // avoid showing context menus, and so this will cause right clicks to leak
  // entries in this map. Most users of the custom context menu (e.g. Pepper
  // plugins) are normally only on "regular" pages and the regular pages will
  // always respond properly to the request, so we don't have to worry so
  // much about leaks.
  IDMap<ContextMenuClient, IDMapExternalPointer> pending_context_menus_;

  // The text selection the last time DidChangeSelection got called. May contain
  // additional characters before and after the selected text, for IMEs. The
  // portion of this string that is the actual selected text starts at index
  // |selection_range_.GetMin() - selection_text_offset_| and has length
  // |selection_range_.length()|.
  base::string16 selection_text_;
  // The offset corresponding to the start of |selection_text_| in the document.
  size_t selection_text_offset_;
  // Range over the document corresponding to the actual selected text (which
  // could correspond to a substring of |selection_text_|; see above).
  gfx::Range selection_range_;
  // Used to inform didChangeSelection() when it is called in the context
  // of handling a InputMsg_SelectRange IPC.
  bool handling_select_range_;

  // The next group of objects all implement RenderFrameObserver, so are deleted
  // along with the RenderFrame automatically.  This is why we just store weak
  // references.

  // Dispatches permission requests for Web Notifications.
  NotificationPermissionDispatcher* notification_permission_dispatcher_;

  // Destroyed via the RenderFrameObserver::OnDestruct() mechanism.
  UserMediaClientImpl* web_user_media_client_;

  // EncryptedMediaClient attached to this frame; lazily initialized.
  scoped_ptr<media::WebEncryptedMediaClientImpl> web_encrypted_media_client_;

  // The media permission dispatcher attached to this frame, lazily initialized.
  MediaPermissionDispatcher* media_permission_dispatcher_;

#if defined(ENABLE_MOJO_MEDIA)
  // The media factory attached to this frame, lazily initialized.
  media::interfaces::ServiceFactoryPtr media_service_factory_;
#endif

  // MidiClient attached to this frame; lazily initialized.
  MidiDispatcher* midi_dispatcher_;

#if defined(OS_ANDROID)
  // Manages all media players in this render frame for communicating with the
  // real media player in the browser process. It's okay to use a raw pointer
  // since it's a RenderFrameObserver.
  RendererMediaPlayerManager* media_player_manager_;
#endif

#if defined(ENABLE_BROWSER_CDMS)
  // Manage all CDMs in this render frame for communicating with the real CDM in
  // the browser process. It's okay to use a raw pointer since it's a
  // RenderFrameObserver.
  RendererCdmManager* cdm_manager_;
#endif

  // The CDM factory attached to this frame, lazily initialized.
  scoped_ptr<media::CdmFactory> cdm_factory_;

#if defined(VIDEO_HOLE)
  // Whether or not this RenderFrameImpl contains a media player. Used to
  // register as an observer for video-hole-specific events.
  bool contains_media_player_;
#endif

  // True if this RenderFrame has ever played media.
  bool has_played_media_;

  // The devtools agent for this frame; only created for main frame and
  // local roots.
  DevToolsAgent* devtools_agent_;

  // The geolocation dispatcher attached to this frame, lazily initialized.
  GeolocationDispatcher* geolocation_dispatcher_;

  // The push messaging dispatcher attached to this frame, lazily initialized.
  PushMessagingDispatcher* push_messaging_dispatcher_;

  // The presentation dispatcher implementation attached to this frame, lazily
  // initialized.
  PresentationDispatcher* presentation_dispatcher_;

  ServiceRegistryImpl service_registry_;

  // The shell proxy used to connect to Mojo applications.
  mojo::ShellPtr mojo_shell_;

  // The screen orientation dispatcher attached to the frame, lazily
  // initialized.
  ScreenOrientationDispatcher* screen_orientation_dispatcher_;

  // The Manifest Manager handles the manifest requests from the browser
  // process.
  ManifestManager* manifest_manager_;

  // The current accessibility mode.
  AccessibilityMode accessibility_mode_;

  // Only valid if |accessibility_mode_| is anything other than
  // AccessibilityModeOff.
  RendererAccessibility* renderer_accessibility_;

  scoped_ptr<PermissionDispatcher> permission_client_;

  scoped_ptr<blink::WebAppBannerClient> app_banner_client_;

  scoped_ptr<blink::WebBluetooth> bluetooth_;

  scoped_ptr<blink::WebUSBClient> usb_client_;

#if defined(ENABLE_WEBVR)
  // The VR dispatcher attached to the frame, lazily initialized.
  scoped_ptr<VRDispatcher> vr_dispatcher_;
#endif

#if defined(OS_MACOSX) || defined(OS_ANDROID)
  // The external popup for the currently showing select popup.
  scoped_ptr<ExternalPopupMenu> external_popup_menu_;
#endif

  base::WeakPtrFactory<RenderFrameImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
