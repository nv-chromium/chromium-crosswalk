// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/socket/socket_api.h"

#include <vector>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/resource_context.h"
#include "extensions/browser/api/dns/host_resolver_wrapper.h"
#include "extensions/browser/api/socket/socket.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/api/socket/tls_socket.h"
#include "extensions/browser/api/socket/udp_socket.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/log/net_log.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_CHROMEOS)
#include "content/public/browser/browser_thread.h"
#endif  // OS_CHROMEOS

namespace extensions {

using content::BrowserThread;
using content::SocketPermissionRequest;

const char kAddressKey[] = "address";
const char kPortKey[] = "port";
const char kBytesWrittenKey[] = "bytesWritten";
const char kDataKey[] = "data";
const char kResultCodeKey[] = "resultCode";
const char kSocketIdKey[] = "socketId";

const char kSocketNotFoundError[] = "Socket not found";
const char kDnsLookupFailedError[] = "DNS resolution failed";
const char kPermissionError[] = "App does not have permission";
const char kNetworkListError[] = "Network lookup failed or unsupported";
const char kTCPSocketBindError[] =
    "TCP socket does not support bind. For TCP server please use listen.";
const char kMulticastSocketTypeError[] = "Only UDP socket supports multicast.";
const char kSecureSocketTypeError[] = "Only TCP sockets are supported for TLS.";
const char kSocketNotConnectedError[] = "Socket not connected";
const char kWildcardAddress[] = "*";
const uint16 kWildcardPort = 0;

#if defined(OS_CHROMEOS)
const char kFirewallFailure[] = "Failed to open firewall port";
#endif  // OS_CHROMEOS

SocketAsyncApiFunction::SocketAsyncApiFunction() {}

SocketAsyncApiFunction::~SocketAsyncApiFunction() {}

bool SocketAsyncApiFunction::PrePrepare() {
  manager_ = CreateSocketResourceManager();
  return manager_->SetBrowserContext(browser_context());
}

bool SocketAsyncApiFunction::Respond() { return error_.empty(); }

scoped_ptr<SocketResourceManagerInterface>
SocketAsyncApiFunction::CreateSocketResourceManager() {
  return scoped_ptr<SocketResourceManagerInterface>(
             new SocketResourceManager<Socket>()).Pass();
}

int SocketAsyncApiFunction::AddSocket(Socket* socket) {
  return manager_->Add(socket);
}

Socket* SocketAsyncApiFunction::GetSocket(int api_resource_id) {
  return manager_->Get(extension_->id(), api_resource_id);
}

void SocketAsyncApiFunction::ReplaceSocket(int api_resource_id,
                                           Socket* socket) {
  manager_->Replace(extension_->id(), api_resource_id, socket);
}

base::hash_set<int>* SocketAsyncApiFunction::GetSocketIds() {
  return manager_->GetResourceIds(extension_->id());
}

void SocketAsyncApiFunction::RemoveSocket(int api_resource_id) {
  manager_->Remove(extension_->id(), api_resource_id);
}

void SocketAsyncApiFunction::OpenFirewallHole(const std::string& address,
                                              int socket_id,
                                              Socket* socket) {
#if defined(OS_CHROMEOS)
  if (!net::IsLocalhost(address)) {
    net::IPEndPoint local_address;
    if (!socket->GetLocalAddress(&local_address)) {
      NOTREACHED() << "Cannot get address of recently bound socket.";
      error_ = kFirewallFailure;
      SetResult(new base::FundamentalValue(-1));
      AsyncWorkCompleted();
      return;
    }

    AppFirewallHole::PortType type = socket->GetSocketType() == Socket::TYPE_TCP
                                         ? AppFirewallHole::PortType::TCP
                                         : AppFirewallHole::PortType::UDP;

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SocketAsyncApiFunction::OpenFirewallHoleOnUIThread, this,
                   type, local_address.port(), socket_id));
    return;
  }
#endif
  AsyncWorkCompleted();
}

#if defined(OS_CHROMEOS)

void SocketAsyncApiFunction::OpenFirewallHoleOnUIThread(
    AppFirewallHole::PortType type,
    uint16_t port,
    int socket_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AppFirewallHoleManager* manager =
      AppFirewallHoleManager::Get(browser_context());
  scoped_ptr<AppFirewallHole, BrowserThread::DeleteOnUIThread> hole(
      manager->Open(type, port, extension_id()).release());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SocketAsyncApiFunction::OnFirewallHoleOpened, this, socket_id,
                 base::Passed(&hole)));
}

void SocketAsyncApiFunction::OnFirewallHoleOpened(
    int socket_id,
    scoped_ptr<AppFirewallHole, BrowserThread::DeleteOnUIThread> hole) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!hole) {
    error_ = kFirewallFailure;
    SetResult(new base::FundamentalValue(-1));
    AsyncWorkCompleted();
    return;
  }

  Socket* socket = GetSocket(socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(new base::FundamentalValue(-1));
    AsyncWorkCompleted();
    return;
  }

  socket->set_firewall_hole(hole.Pass());
  AsyncWorkCompleted();
}

#endif  // OS_CHROMEOS

SocketExtensionWithDnsLookupFunction::SocketExtensionWithDnsLookupFunction()
    : resource_context_(NULL) {
}

SocketExtensionWithDnsLookupFunction::~SocketExtensionWithDnsLookupFunction() {
}

bool SocketExtensionWithDnsLookupFunction::PrePrepare() {
  if (!SocketAsyncApiFunction::PrePrepare())
    return false;
  resource_context_ = browser_context()->GetResourceContext();
  return resource_context_ != NULL;
}

void SocketExtensionWithDnsLookupFunction::StartDnsLookup(
    const net::HostPortPair& host_port_pair) {
  net::HostResolver* host_resolver =
      HostResolverWrapper::GetInstance()->GetHostResolver(resource_context_);
  DCHECK(host_resolver);

  // RequestHandle is not needed because we never need to cancel requests.
  net::HostResolver::RequestHandle request_handle;

  net::HostResolver::RequestInfo request_info(host_port_pair);
  int resolve_result = host_resolver->Resolve(
      request_info, net::DEFAULT_PRIORITY, &addresses_,
      base::Bind(&SocketExtensionWithDnsLookupFunction::OnDnsLookup, this),
      &request_handle, net::BoundNetLog());

  if (resolve_result != net::ERR_IO_PENDING)
    OnDnsLookup(resolve_result);
}

void SocketExtensionWithDnsLookupFunction::OnDnsLookup(int resolve_result) {
  if (resolve_result == net::OK) {
    DCHECK(!addresses_.empty());
  } else {
    error_ = kDnsLookupFailedError;
  }
  AfterDnsLookup(resolve_result);
}

SocketCreateFunction::SocketCreateFunction()
    : socket_type_(kSocketTypeInvalid) {}

SocketCreateFunction::~SocketCreateFunction() {}

bool SocketCreateFunction::Prepare() {
  params_ = api::socket::Create::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  switch (params_->type) {
    case extensions::api::socket::SOCKET_TYPE_TCP:
      socket_type_ = kSocketTypeTCP;
      break;
    case extensions::api::socket::SOCKET_TYPE_UDP:
      socket_type_ = kSocketTypeUDP;
      break;
    case extensions::api::socket::SOCKET_TYPE_NONE:
      NOTREACHED();
      break;
  }

  return true;
}

void SocketCreateFunction::Work() {
  Socket* socket = NULL;
  if (socket_type_ == kSocketTypeTCP) {
    socket = new TCPSocket(extension_->id());
  } else if (socket_type_ == kSocketTypeUDP) {
    socket = new UDPSocket(extension_->id());
  }
  DCHECK(socket);

  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetInteger(kSocketIdKey, AddSocket(socket));
  SetResult(result);
}

bool SocketDestroyFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  return true;
}

void SocketDestroyFunction::Work() { RemoveSocket(socket_id_); }

SocketConnectFunction::SocketConnectFunction()
    : socket_id_(0), hostname_(), port_(0) {
}

SocketConnectFunction::~SocketConnectFunction() {}

bool SocketConnectFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &hostname_));
  int port;
  EXTENSION_FUNCTION_VALIDATE(
      args_->GetInteger(2, &port) && port >= 0 && port <= 65535);
  port_ = static_cast<uint16>(port);
  return true;
}

void SocketConnectFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(new base::FundamentalValue(-1));
    AsyncWorkCompleted();
    return;
  }

  socket->set_hostname(hostname_);

  SocketPermissionRequest::OperationType operation_type;
  switch (socket->GetSocketType()) {
    case Socket::TYPE_TCP:
      operation_type = SocketPermissionRequest::TCP_CONNECT;
      break;
    case Socket::TYPE_UDP:
      operation_type = SocketPermissionRequest::UDP_SEND_TO;
      break;
    default:
      NOTREACHED() << "Unknown socket type.";
      operation_type = SocketPermissionRequest::NONE;
      break;
  }

  SocketPermission::CheckParam param(operation_type, hostname_, port_);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kSocket, &param)) {
    error_ = kPermissionError;
    SetResult(new base::FundamentalValue(-1));
    AsyncWorkCompleted();
    return;
  }

  StartDnsLookup(net::HostPortPair(hostname_, port_));
}

void SocketConnectFunction::AfterDnsLookup(int lookup_result) {
  if (lookup_result == net::OK) {
    StartConnect();
  } else {
    SetResult(new base::FundamentalValue(lookup_result));
    AsyncWorkCompleted();
  }
}

void SocketConnectFunction::StartConnect() {
  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(new base::FundamentalValue(-1));
    AsyncWorkCompleted();
    return;
  }

  socket->Connect(addresses_,
                  base::Bind(&SocketConnectFunction::OnConnect, this));
}

void SocketConnectFunction::OnConnect(int result) {
  SetResult(new base::FundamentalValue(result));
  AsyncWorkCompleted();
}

bool SocketDisconnectFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  return true;
}

void SocketDisconnectFunction::Work() {
  Socket* socket = GetSocket(socket_id_);
  if (socket)
    socket->Disconnect();
  else
    error_ = kSocketNotFoundError;
  SetResult(base::Value::CreateNullValue());
}

bool SocketBindFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &address_));
  int port;
  EXTENSION_FUNCTION_VALIDATE(
      args_->GetInteger(2, &port) && port >= 0 && port <= 65535);
  port_ = static_cast<uint16>(port);
  return true;
}

void SocketBindFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(new base::FundamentalValue(-1));
    AsyncWorkCompleted();
    return;
  }

  if (socket->GetSocketType() == Socket::TYPE_TCP) {
    error_ = kTCPSocketBindError;
    SetResult(new base::FundamentalValue(-1));
    AsyncWorkCompleted();
    return;
  }

  CHECK(socket->GetSocketType() == Socket::TYPE_UDP);
  SocketPermission::CheckParam param(SocketPermissionRequest::UDP_BIND,
                                     address_, port_);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kSocket, &param)) {
    error_ = kPermissionError;
    SetResult(new base::FundamentalValue(-1));
    AsyncWorkCompleted();
    return;
  }

  int result = socket->Bind(address_, port_);
  SetResult(new base::FundamentalValue(result));
  if (result != net::OK) {
    AsyncWorkCompleted();
    return;
  }

  OpenFirewallHole(address_, socket_id_, socket);
}

SocketListenFunction::SocketListenFunction() {}

SocketListenFunction::~SocketListenFunction() {}

bool SocketListenFunction::Prepare() {
  params_ = api::socket::Listen::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketListenFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(new base::FundamentalValue(-1));
    AsyncWorkCompleted();
    return;
  }

  SocketPermission::CheckParam param(SocketPermissionRequest::TCP_LISTEN,
                                     params_->address, params_->port);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kSocket, &param)) {
    error_ = kPermissionError;
    SetResult(new base::FundamentalValue(-1));
    AsyncWorkCompleted();
    return;
  }

  int result = socket->Listen(
      params_->address, params_->port,
      params_->backlog.get() ? *params_->backlog.get() : 5, &error_);
  SetResult(new base::FundamentalValue(result));
  if (result != net::OK) {
    AsyncWorkCompleted();
    return;
  }

  OpenFirewallHole(params_->address, params_->socket_id, socket);
}

SocketAcceptFunction::SocketAcceptFunction() {}

SocketAcceptFunction::~SocketAcceptFunction() {}

bool SocketAcceptFunction::Prepare() {
  params_ = api::socket::Accept::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketAcceptFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(params_->socket_id);
  if (socket) {
    socket->Accept(base::Bind(&SocketAcceptFunction::OnAccept, this));
  } else {
    error_ = kSocketNotFoundError;
    OnAccept(-1, NULL);
  }
}

void SocketAcceptFunction::OnAccept(int result_code,
                                    net::TCPClientSocket* socket) {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetInteger(kResultCodeKey, result_code);
  if (socket) {
    Socket* client_socket = new TCPSocket(socket, extension_id(), true);
    result->SetInteger(kSocketIdKey, AddSocket(client_socket));
  }
  SetResult(result);

  AsyncWorkCompleted();
}

SocketReadFunction::SocketReadFunction() {}

SocketReadFunction::~SocketReadFunction() {}

bool SocketReadFunction::Prepare() {
  params_ = api::socket::Read::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketReadFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    OnCompleted(-1, NULL);
    return;
  }

  socket->Read(params_->buffer_size.get() ? *params_->buffer_size.get() : 4096,
               base::Bind(&SocketReadFunction::OnCompleted, this));
}

void SocketReadFunction::OnCompleted(int bytes_read,
                                     scoped_refptr<net::IOBuffer> io_buffer) {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetInteger(kResultCodeKey, bytes_read);
  if (bytes_read > 0) {
    result->Set(kDataKey,
                base::BinaryValue::CreateWithCopiedBuffer(io_buffer->data(),
                                                          bytes_read));
  } else {
    result->Set(kDataKey, new base::BinaryValue());
  }
  SetResult(result);

  AsyncWorkCompleted();
}

SocketWriteFunction::SocketWriteFunction()
    : socket_id_(0), io_buffer_(NULL), io_buffer_size_(0) {}

SocketWriteFunction::~SocketWriteFunction() {}

bool SocketWriteFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  base::BinaryValue* data = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBinary(1, &data));

  io_buffer_size_ = data->GetSize();
  io_buffer_ = new net::WrappedIOBuffer(data->GetBuffer());
  return true;
}

void SocketWriteFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(socket_id_);

  if (!socket) {
    error_ = kSocketNotFoundError;
    OnCompleted(-1);
    return;
  }

  socket->Write(io_buffer_,
                io_buffer_size_,
                base::Bind(&SocketWriteFunction::OnCompleted, this));
}

void SocketWriteFunction::OnCompleted(int bytes_written) {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetInteger(kBytesWrittenKey, bytes_written);
  SetResult(result);

  AsyncWorkCompleted();
}

SocketRecvFromFunction::SocketRecvFromFunction() {}

SocketRecvFromFunction::~SocketRecvFromFunction() {}

bool SocketRecvFromFunction::Prepare() {
  params_ = api::socket::RecvFrom::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketRecvFromFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket || socket->GetSocketType() != Socket::TYPE_UDP) {
    error_ = kSocketNotFoundError;
    OnCompleted(-1, NULL, std::string(), 0);
    return;
  }

  socket->RecvFrom(params_->buffer_size.get() ? *params_->buffer_size : 4096,
                   base::Bind(&SocketRecvFromFunction::OnCompleted, this));
}

void SocketRecvFromFunction::OnCompleted(int bytes_read,
                                         scoped_refptr<net::IOBuffer> io_buffer,
                                         const std::string& address,
                                         uint16 port) {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetInteger(kResultCodeKey, bytes_read);
  if (bytes_read > 0) {
    result->Set(kDataKey,
                base::BinaryValue::CreateWithCopiedBuffer(io_buffer->data(),
                                                          bytes_read));
  } else {
    result->Set(kDataKey, new base::BinaryValue());
  }
  result->SetString(kAddressKey, address);
  result->SetInteger(kPortKey, port);
  SetResult(result);

  AsyncWorkCompleted();
}

SocketSendToFunction::SocketSendToFunction()
    : socket_id_(0), io_buffer_(NULL), io_buffer_size_(0), port_(0) {
}

SocketSendToFunction::~SocketSendToFunction() {}

bool SocketSendToFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  base::BinaryValue* data = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBinary(1, &data));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &hostname_));
  int port;
  EXTENSION_FUNCTION_VALIDATE(
      args_->GetInteger(3, &port) && port >= 0 && port <= 65535);
  port_ = static_cast<uint16>(port);

  io_buffer_size_ = data->GetSize();
  io_buffer_ = new net::WrappedIOBuffer(data->GetBuffer());
  return true;
}

void SocketSendToFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(new base::FundamentalValue(-1));
    AsyncWorkCompleted();
    return;
  }

  if (socket->GetSocketType() == Socket::TYPE_UDP) {
    SocketPermission::CheckParam param(
        SocketPermissionRequest::UDP_SEND_TO, hostname_, port_);
    if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
            APIPermission::kSocket, &param)) {
      error_ = kPermissionError;
      SetResult(new base::FundamentalValue(-1));
      AsyncWorkCompleted();
      return;
    }
  }

  StartDnsLookup(net::HostPortPair(hostname_, port_));
}

void SocketSendToFunction::AfterDnsLookup(int lookup_result) {
  if (lookup_result == net::OK) {
    StartSendTo();
  } else {
    SetResult(new base::FundamentalValue(lookup_result));
    AsyncWorkCompleted();
  }
}

void SocketSendToFunction::StartSendTo() {
  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(new base::FundamentalValue(-1));
    AsyncWorkCompleted();
    return;
  }

  socket->SendTo(io_buffer_, io_buffer_size_, addresses_.front(),
                 base::Bind(&SocketSendToFunction::OnCompleted, this));
}

void SocketSendToFunction::OnCompleted(int bytes_written) {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetInteger(kBytesWrittenKey, bytes_written);
  SetResult(result);

  AsyncWorkCompleted();
}

SocketSetKeepAliveFunction::SocketSetKeepAliveFunction() {}

SocketSetKeepAliveFunction::~SocketSetKeepAliveFunction() {}

bool SocketSetKeepAliveFunction::Prepare() {
  params_ = api::socket::SetKeepAlive::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketSetKeepAliveFunction::Work() {
  bool result = false;
  Socket* socket = GetSocket(params_->socket_id);
  if (socket) {
    int delay = 0;
    if (params_->delay.get())
      delay = *params_->delay;
    result = socket->SetKeepAlive(params_->enable, delay);
  } else {
    error_ = kSocketNotFoundError;
  }
  SetResult(new base::FundamentalValue(result));
}

SocketSetNoDelayFunction::SocketSetNoDelayFunction() {}

SocketSetNoDelayFunction::~SocketSetNoDelayFunction() {}

bool SocketSetNoDelayFunction::Prepare() {
  params_ = api::socket::SetNoDelay::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketSetNoDelayFunction::Work() {
  bool result = false;
  Socket* socket = GetSocket(params_->socket_id);
  if (socket)
    result = socket->SetNoDelay(params_->no_delay);
  else
    error_ = kSocketNotFoundError;
  SetResult(new base::FundamentalValue(result));
}

SocketGetInfoFunction::SocketGetInfoFunction() {}

SocketGetInfoFunction::~SocketGetInfoFunction() {}

bool SocketGetInfoFunction::Prepare() {
  params_ = api::socket::GetInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketGetInfoFunction::Work() {
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  api::socket::SocketInfo info;
  // This represents what we know about the socket, and does not call through
  // to the system.
  if (socket->GetSocketType() == Socket::TYPE_TCP)
    info.socket_type = extensions::api::socket::SOCKET_TYPE_TCP;
  else
    info.socket_type = extensions::api::socket::SOCKET_TYPE_UDP;
  info.connected = socket->IsConnected();

  // Grab the peer address as known by the OS. This and the call below will
  // always succeed while the socket is connected, even if the socket has
  // been remotely closed by the peer; only reading the socket will reveal
  // that it should be closed locally.
  net::IPEndPoint peerAddress;
  if (socket->GetPeerAddress(&peerAddress)) {
    info.peer_address.reset(new std::string(peerAddress.ToStringWithoutPort()));
    info.peer_port.reset(new int(peerAddress.port()));
  }

  // Grab the local address as known by the OS.
  net::IPEndPoint localAddress;
  if (socket->GetLocalAddress(&localAddress)) {
    info.local_address.reset(
        new std::string(localAddress.ToStringWithoutPort()));
    info.local_port.reset(new int(localAddress.port()));
  }

  SetResult(info.ToValue().release());
}

bool SocketGetNetworkListFunction::RunAsync() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SocketGetNetworkListFunction::GetNetworkListOnFileThread,
                 this));
  return true;
}

void SocketGetNetworkListFunction::GetNetworkListOnFileThread() {
  net::NetworkInterfaceList interface_list;
  if (GetNetworkList(&interface_list,
                     net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SocketGetNetworkListFunction::SendResponseOnUIThread, this,
                   interface_list));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SocketGetNetworkListFunction::HandleGetNetworkListError,
                 this));
}

void SocketGetNetworkListFunction::HandleGetNetworkListError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  error_ = kNetworkListError;
  SendResponse(false);
}

void SocketGetNetworkListFunction::SendResponseOnUIThread(
    const net::NetworkInterfaceList& interface_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<linked_ptr<api::socket::NetworkInterface>> create_arg;
  create_arg.reserve(interface_list.size());
  for (net::NetworkInterfaceList::const_iterator i = interface_list.begin();
       i != interface_list.end();
       ++i) {
    linked_ptr<api::socket::NetworkInterface> info =
        make_linked_ptr(new api::socket::NetworkInterface);
    info->name = i->name;
    info->address = net::IPAddressToString(i->address);
    info->prefix_length = i->prefix_length;
    create_arg.push_back(info);
  }

  results_ = api::socket::GetNetworkList::Results::Create(create_arg);
  SendResponse(true);
}

SocketJoinGroupFunction::SocketJoinGroupFunction() {}

SocketJoinGroupFunction::~SocketJoinGroupFunction() {}

bool SocketJoinGroupFunction::Prepare() {
  params_ = api::socket::JoinGroup::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketJoinGroupFunction::Work() {
  int result = -1;
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(new base::FundamentalValue(result));
    return;
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    error_ = kMulticastSocketTypeError;
    SetResult(new base::FundamentalValue(result));
    return;
  }

  SocketPermission::CheckParam param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);

  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kSocket, &param)) {
    error_ = kPermissionError;
    SetResult(new base::FundamentalValue(result));
    return;
  }

  result = static_cast<UDPSocket*>(socket)->JoinGroup(params_->address);
  if (result != 0) {
    error_ = net::ErrorToString(result);
  }
  SetResult(new base::FundamentalValue(result));
}

SocketLeaveGroupFunction::SocketLeaveGroupFunction() {}

SocketLeaveGroupFunction::~SocketLeaveGroupFunction() {}

bool SocketLeaveGroupFunction::Prepare() {
  params_ = api::socket::LeaveGroup::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketLeaveGroupFunction::Work() {
  int result = -1;
  Socket* socket = GetSocket(params_->socket_id);

  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(new base::FundamentalValue(result));
    return;
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    error_ = kMulticastSocketTypeError;
    SetResult(new base::FundamentalValue(result));
    return;
  }

  SocketPermission::CheckParam param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kSocket, &param)) {
    error_ = kPermissionError;
    SetResult(new base::FundamentalValue(result));
    return;
  }

  result = static_cast<UDPSocket*>(socket)->LeaveGroup(params_->address);
  if (result != 0)
    error_ = net::ErrorToString(result);
  SetResult(new base::FundamentalValue(result));
}

SocketSetMulticastTimeToLiveFunction::SocketSetMulticastTimeToLiveFunction() {}

SocketSetMulticastTimeToLiveFunction::~SocketSetMulticastTimeToLiveFunction() {}

bool SocketSetMulticastTimeToLiveFunction::Prepare() {
  params_ = api::socket::SetMulticastTimeToLive::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}
void SocketSetMulticastTimeToLiveFunction::Work() {
  int result = -1;
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(new base::FundamentalValue(result));
    return;
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    error_ = kMulticastSocketTypeError;
    SetResult(new base::FundamentalValue(result));
    return;
  }

  result =
      static_cast<UDPSocket*>(socket)->SetMulticastTimeToLive(params_->ttl);
  if (result != 0)
    error_ = net::ErrorToString(result);
  SetResult(new base::FundamentalValue(result));
}

SocketSetMulticastLoopbackModeFunction::
    SocketSetMulticastLoopbackModeFunction() {}

SocketSetMulticastLoopbackModeFunction::
    ~SocketSetMulticastLoopbackModeFunction() {}

bool SocketSetMulticastLoopbackModeFunction::Prepare() {
  params_ = api::socket::SetMulticastLoopbackMode::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketSetMulticastLoopbackModeFunction::Work() {
  int result = -1;
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(new base::FundamentalValue(result));
    return;
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    error_ = kMulticastSocketTypeError;
    SetResult(new base::FundamentalValue(result));
    return;
  }

  result = static_cast<UDPSocket*>(socket)
               ->SetMulticastLoopbackMode(params_->enabled);
  if (result != 0)
    error_ = net::ErrorToString(result);
  SetResult(new base::FundamentalValue(result));
}

SocketGetJoinedGroupsFunction::SocketGetJoinedGroupsFunction() {}

SocketGetJoinedGroupsFunction::~SocketGetJoinedGroupsFunction() {}

bool SocketGetJoinedGroupsFunction::Prepare() {
  params_ = api::socket::GetJoinedGroups::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketGetJoinedGroupsFunction::Work() {
  int result = -1;
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(new base::FundamentalValue(result));
    return;
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    error_ = kMulticastSocketTypeError;
    SetResult(new base::FundamentalValue(result));
    return;
  }

  SocketPermission::CheckParam param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kSocket, &param)) {
    error_ = kPermissionError;
    SetResult(new base::FundamentalValue(result));
    return;
  }

  base::ListValue* values = new base::ListValue();
  values->AppendStrings((std::vector<std::string>&)static_cast<UDPSocket*>(
                            socket)->GetJoinedGroups());
  SetResult(values);
}

SocketSecureFunction::SocketSecureFunction() {
}

SocketSecureFunction::~SocketSecureFunction() {
}

bool SocketSecureFunction::Prepare() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  params_ = api::socket::Secure::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  url_request_getter_ = browser_context()->GetRequestContext();
  return true;
}

// Override the regular implementation, which would call AsyncWorkCompleted
// immediately after Work().
void SocketSecureFunction::AsyncWorkStart() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    SetResult(new base::FundamentalValue(net::ERR_INVALID_ARGUMENT));
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }

  // Make sure that the socket is a TCP client socket.
  if (socket->GetSocketType() != Socket::TYPE_TCP ||
      static_cast<TCPSocket*>(socket)->ClientStream() == NULL) {
    SetResult(new base::FundamentalValue(net::ERR_INVALID_ARGUMENT));
    error_ = kSecureSocketTypeError;
    AsyncWorkCompleted();
    return;
  }

  if (!socket->IsConnected()) {
    SetResult(new base::FundamentalValue(net::ERR_INVALID_ARGUMENT));
    error_ = kSocketNotConnectedError;
    AsyncWorkCompleted();
    return;
  }

  net::URLRequestContext* url_request_context =
      url_request_getter_->GetURLRequestContext();

  TLSSocket::UpgradeSocketToTLS(
      socket,
      url_request_context->ssl_config_service(),
      url_request_context->cert_verifier(),
      url_request_context->transport_security_state(),
      extension_id(),
      params_->options.get(),
      base::Bind(&SocketSecureFunction::TlsConnectDone, this));
}

void SocketSecureFunction::TlsConnectDone(scoped_ptr<TLSSocket> socket,
                                          int result) {
  // if an error occurred, socket MUST be NULL.
  DCHECK(result == net::OK || socket == NULL);

  if (socket && result == net::OK) {
    ReplaceSocket(params_->socket_id, socket.release());
  } else {
    RemoveSocket(params_->socket_id);
    error_ = net::ErrorToString(result);
  }

  results_ = api::socket::Secure::Results::Create(result);
  AsyncWorkCompleted();
}

}  // namespace extensions
