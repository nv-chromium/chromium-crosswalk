// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>
#include <sstream>

#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "ipc/ipc_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

// IPC messages for testing ----------------------------------------------------

#define IPC_MESSAGE_IMPL
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START TestMsgStart

// Generic message class that is an int followed by a string16.
IPC_MESSAGE_CONTROL2(MsgClassIS, int, base::string16)

// Generic message class that is a string16 followed by an int.
IPC_MESSAGE_CONTROL2(MsgClassSI, base::string16, int)

// Message to create a mutex in the IPC server, using the received name.
IPC_MESSAGE_CONTROL2(MsgDoMutex, base::string16, int)

// Used to generate an ID for a message that should not exist.
IPC_MESSAGE_CONTROL0(MsgUnhandled)

// -----------------------------------------------------------------------------

namespace {

TEST(IPCMessageIntegrity, ReadBeyondBufferStr) {
  // This was BUG 984408.
  uint32 v1 = kuint32max - 1;
  int v2 = 666;
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(v1));
  EXPECT_TRUE(m.WriteInt(v2));

  base::PickleIterator iter(m);
  std::string vs;
  EXPECT_FALSE(iter.ReadString(&vs));
}

TEST(IPCMessageIntegrity, ReadBeyondBufferStr16) {
  // This was BUG 984408.
  uint32 v1 = kuint32max - 1;
  int v2 = 777;
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(v1));
  EXPECT_TRUE(m.WriteInt(v2));

  base::PickleIterator iter(m);
  base::string16 vs;
  EXPECT_FALSE(iter.ReadString16(&vs));
}

TEST(IPCMessageIntegrity, ReadBytesBadIterator) {
  // This was BUG 1035467.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(1));
  EXPECT_TRUE(m.WriteInt(2));

  base::PickleIterator iter(m);
  const char* data = NULL;
  EXPECT_TRUE(iter.ReadBytes(&data, sizeof(int)));
}

TEST(IPCMessageIntegrity, ReadVectorNegativeSize) {
  // A slight variation of BUG 984408. Note that the pickling of vector<char>
  // has a specialized template which is not vulnerable to this bug. So here
  // try to hit the non-specialized case vector<P>.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(-1));   // This is the count of elements.
  EXPECT_TRUE(m.WriteInt(1));
  EXPECT_TRUE(m.WriteInt(2));
  EXPECT_TRUE(m.WriteInt(3));

  std::vector<double> vec;
  base::PickleIterator iter(m);
  EXPECT_FALSE(ReadParam(&m, &iter, &vec));
}

#if defined(OS_ANDROID)
#define MAYBE_ReadVectorTooLarge1 DISABLED_ReadVectorTooLarge1
#else
#define MAYBE_ReadVectorTooLarge1 ReadVectorTooLarge1
#endif
TEST(IPCMessageIntegrity, MAYBE_ReadVectorTooLarge1) {
  // This was BUG 1006367. This is the large but positive length case. Again
  // we try to hit the non-specialized case vector<P>.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(0x21000003));   // This is the count of elements.
  EXPECT_TRUE(m.WriteInt64(1));
  EXPECT_TRUE(m.WriteInt64(2));

  std::vector<int64> vec;
  base::PickleIterator iter(m);
  EXPECT_FALSE(ReadParam(&m, &iter, &vec));
}

TEST(IPCMessageIntegrity, ReadVectorTooLarge2) {
  // This was BUG 1006367. This is the large but positive with an additional
  // integer overflow when computing the actual byte size. Again we try to hit
  // the non-specialized case vector<P>.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(0x71000000));   // This is the count of elements.
  EXPECT_TRUE(m.WriteInt64(1));
  EXPECT_TRUE(m.WriteInt64(2));

  std::vector<int64> vec;
  base::PickleIterator iter(m);
  EXPECT_FALSE(ReadParam(&m, &iter, &vec));
}

class SimpleListener : public IPC::Listener {
 public:
  SimpleListener() : other_(NULL) {
  }
  void Init(IPC::Sender* s) {
    other_ = s;
  }
 protected:
  IPC::Sender* other_;
};

enum {
  FUZZER_ROUTING_ID = 5
};

// The fuzzer server class. It runs in a child process and expects
// only two IPC calls; after that it exits the message loop which
// terminates the child process.
class FuzzerServerListener : public SimpleListener {
 public:
  FuzzerServerListener() : message_count_(2), pending_messages_(0) {
  }
  bool OnMessageReceived(const IPC::Message& msg) override {
    if (msg.routing_id() == MSG_ROUTING_CONTROL) {
      ++pending_messages_;
      IPC_BEGIN_MESSAGE_MAP(FuzzerServerListener, msg)
        IPC_MESSAGE_HANDLER(MsgClassIS, OnMsgClassISMessage)
        IPC_MESSAGE_HANDLER(MsgClassSI, OnMsgClassSIMessage)
      IPC_END_MESSAGE_MAP()
      if (pending_messages_) {
        // Probably a problem de-serializing the message.
        ReplyMsgNotHandled(msg.type());
      }
    }
    return true;
  }

 private:
  void OnMsgClassISMessage(int value, const base::string16& text) {
    UseData(MsgClassIS::ID, value, text);
    RoundtripAckReply(FUZZER_ROUTING_ID, MsgClassIS::ID, value);
    Cleanup();
  }

  void OnMsgClassSIMessage(const base::string16& text, int value) {
    UseData(MsgClassSI::ID, value, text);
    RoundtripAckReply(FUZZER_ROUTING_ID, MsgClassSI::ID, value);
    Cleanup();
  }

  bool RoundtripAckReply(int routing, uint32 type_id, int reply) {
    IPC::Message* message = new IPC::Message(routing, type_id,
                                             IPC::Message::PRIORITY_NORMAL);
    message->WriteInt(reply + 1);
    message->WriteInt(reply);
    return other_->Send(message);
  }

  void Cleanup() {
    --message_count_;
    --pending_messages_;
    if (0 == message_count_)
      base::MessageLoop::current()->Quit();
  }

  void ReplyMsgNotHandled(uint32 type_id) {
    RoundtripAckReply(FUZZER_ROUTING_ID, MsgUnhandled::ID, type_id);
    Cleanup();
  }

  void UseData(int caller, int value, const base::string16& text) {
    std::ostringstream os;
    os << "IPC fuzzer:" << caller << " [" << value << " "
       << base::UTF16ToUTF8(text) << "]\n";
    std::string output = os.str();
    LOG(WARNING) << output;
  }

  int message_count_;
  int pending_messages_;
};

class FuzzerClientListener : public SimpleListener {
 public:
  FuzzerClientListener() : last_msg_(NULL) {
  }

  bool OnMessageReceived(const IPC::Message& msg) override {
    last_msg_ = new IPC::Message(msg);
    base::MessageLoop::current()->Quit();
    return true;
  }

  bool ExpectMessage(int value, uint32 type_id) {
    if (!MsgHandlerInternal(type_id))
      return false;
    int msg_value1 = 0;
    int msg_value2 = 0;
    base::PickleIterator iter(*last_msg_);
    if (!iter.ReadInt(&msg_value1))
      return false;
    if (!iter.ReadInt(&msg_value2))
      return false;
    if ((msg_value2 + 1) != msg_value1)
      return false;
    if (msg_value2 != value)
      return false;

    delete last_msg_;
    last_msg_ = NULL;
    return true;
  }

  bool ExpectMsgNotHandled(uint32 type_id) {
    return ExpectMessage(type_id, MsgUnhandled::ID);
  }

 private:
  bool MsgHandlerInternal(uint32 type_id) {
    base::MessageLoop::current()->Run();
    if (NULL == last_msg_)
      return false;
    if (FUZZER_ROUTING_ID != last_msg_->routing_id())
      return false;
    return (type_id == last_msg_->type());
  }

  IPC::Message* last_msg_;
};

// Runs the fuzzing server child mode. Returns when the preset number of
// messages have been received.
MULTIPROCESS_IPC_TEST_CLIENT_MAIN(FuzzServerClient) {
  base::MessageLoopForIO main_message_loop;
  FuzzerServerListener listener;
  scoped_ptr<IPC::Channel> channel(IPC::Channel::CreateClient(
      IPCTestBase::GetChannelName("FuzzServerClient"), &listener, nullptr));
  CHECK(channel->Connect());
  listener.Init(channel.get());
  base::MessageLoop::current()->Run();
  return 0;
}

class IPCFuzzingTest : public IPCTestBase {
};

#if defined(OS_ANDROID)
#define MAYBE_SanityTest DISABLED_SanityTest
#else
#define MAYBE_SanityTest SanityTest
#endif
// This test makes sure that the FuzzerClientListener and FuzzerServerListener
// are working properly by generating two well formed IPC calls.
TEST_F(IPCFuzzingTest, MAYBE_SanityTest) {
  Init("FuzzServerClient");

  FuzzerClientListener listener;
  CreateChannel(&listener);
  listener.Init(channel());
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  IPC::Message* msg = NULL;
  int value = 43;
  msg = new MsgClassIS(value, base::ASCIIToUTF16("expect 43"));
  sender()->Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(value, MsgClassIS::ID));

  msg = new MsgClassSI(base::ASCIIToUTF16("expect 44"), ++value);
  sender()->Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(value, MsgClassSI::ID));

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

#if defined(OS_ANDROID)
#define MAYBE_MsgBadPayloadShort DISABLED_MsgBadPayloadShort
#else
#define MAYBE_MsgBadPayloadShort MsgBadPayloadShort
#endif
// This test uses a payload that is smaller than expected. This generates an
// error while unpacking the IPC buffer which in debug trigger an assertion and
// in release is ignored (!). Right after we generate another valid IPC to make
// sure framing is working properly.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
TEST_F(IPCFuzzingTest, MAYBE_MsgBadPayloadShort) {
  Init("FuzzServerClient");

  FuzzerClientListener listener;
  CreateChannel(&listener);
  listener.Init(channel());
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  IPC::Message* msg = new IPC::Message(MSG_ROUTING_CONTROL, MsgClassIS::ID,
                                       IPC::Message::PRIORITY_NORMAL);
  msg->WriteInt(666);
  sender()->Send(msg);
  EXPECT_TRUE(listener.ExpectMsgNotHandled(MsgClassIS::ID));

  msg = new MsgClassSI(base::ASCIIToUTF16("expect one"), 1);
  sender()->Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(1, MsgClassSI::ID));

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}
#endif

#if defined(OS_ANDROID)
#define MAYBE_MsgBadPayloadArgs DISABLED_MsgBadPayloadArgs
#else
#define MAYBE_MsgBadPayloadArgs MsgBadPayloadArgs
#endif
// This test uses a payload that has too many arguments, but so the payload size
// is big enough so the unpacking routine does not generate an error as in the
// case of MsgBadPayloadShort test. This test does not pinpoint a flaw (per se)
// as by design we don't carry type information on the IPC message.
TEST_F(IPCFuzzingTest, MAYBE_MsgBadPayloadArgs) {
  Init("FuzzServerClient");

  FuzzerClientListener listener;
  CreateChannel(&listener);
  listener.Init(channel());
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  IPC::Message* msg = new IPC::Message(MSG_ROUTING_CONTROL, MsgClassSI::ID,
                                       IPC::Message::PRIORITY_NORMAL);
  msg->WriteString16(base::ASCIIToUTF16("d"));
  msg->WriteInt(0);
  msg->WriteInt(0x65);  // Extra argument.

  sender()->Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(0, MsgClassSI::ID));

  // Now send a well formed message to make sure the receiver wasn't
  // thrown out of sync by the extra argument.
  msg = new MsgClassIS(3, base::ASCIIToUTF16("expect three"));
  sender()->Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(3, MsgClassIS::ID));

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

}  // namespace
