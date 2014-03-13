#pragma once
#include <functional>

#include <msg_robosub.h>
#include <msg_regul.h>
#include <msg_navig.h>

#include <ipc.h>

#include <gazebo/msgs/msgs.hh>
#include <gazebo/transport/transport.hh>

#include <boost/shared_ptr.hpp>

#include "common.h"

template<typename MsgType>
using Callback = std::function<void (const MsgType&)>;

typedef boost::shared_ptr<const gazebo::msgs::Image> ImageMsgPtr;


template<typename MsgType>
class Reciever {
public:
    explicit Reciever(Callback<MsgType> callback)
        : callback_(callback) {}

    virtual ~Reciever() {}

protected:
    Callback<MsgType> callback_;
};

template<typename MsgType, typename MsgConsts>
class IPCReciever: public Reciever<MsgType> {
public:
    explicit IPCReciever(Callback<MsgType> callback, gazebo::transport::NodePtr node = nullptr)
            : Reciever<MsgType>(callback)
    {
        IPC_defineMsg(consts_.IPC_NAME, IPC_VARIABLE_LENGTH, consts_.IPC_FORMAT);
        msg_format = IPC_parseFormat(consts_.IPC_FORMAT);

        INFO() << "Subscribing to ipc message: " << consts_.IPC_NAME;
        IPC_subscribeData(MSG_REGUL_NAME, recieve_msg, this);
    }

    static void recieve_msg(MSG_INSTANCE msgRef, void *callData, void* clientData) {
        auto client = static_cast<const IPCReciever<MsgType, MsgConsts>*>(clientData);

        MsgType *m;
        IPC_unmarshall(client->msg_format, callData, (void **)&m);

        client->callback_(*m);

        IPC_freeByteArray(callData);
        IPC_freeData(client->msg_format, m);
    }

    FORMATTER_PTR msg_format;
private:
    MsgConsts consts_;
};

template<typename MsgType, typename MsgConsts>
class GazeboReciever: public Reciever<MsgType> {
public:
    explicit GazeboReciever(Callback<MsgType> callback, gazebo::transport::NodePtr node)
            : Reciever<MsgType>(callback), node_(node)
    {
        INFO() << "Subscribing to gazebo topic: " << consts_.TOPIC;
        gazebo::transport::SubscriberPtr sub = node->Subscribe(consts_.TOPIC, &recieve_msg, this);
    }

    void recieve_msg(MsgType msg) {
        callback_(msg);
    }
private:
    gazebo::transport::NodePtr node_;
    MsgConsts consts_;
};

template<typename MsgType, typename MsgConsts>
class GazeboForwarder {
public:
    GazeboForwarder(gazebo::transport::NodePtr node) {
        publisher_ = node->Advertise<MsgType>(consts_.TOPIC);
        publisher_->WaitForConnection();
    }

    void publish(const MsgType& msg) {
        publisher_->Publish(msg);
    }

private:
    gazebo::transport::PublisherPtr publisher_;
    MsgConsts consts_;
};

template<typename MsgType, typename MsgConsts>
class IPCForwarder {
public:
    IPCForwarder() {
        IPC_defineMsg(consts_.IPC_NAME, IPC_VARIABLE_LENGTH, consts_.IPC_FORMAT);
    }

    void publish(const MsgType& msg) {
        IPC_publishData(MsgConsts().IPC_NAME, &msg);
    }
private:
    MsgConsts consts_;
};


struct RegulPipeConsts {
    const char* IPC_NAME = MSG_REGUL_NAME;
    const char* IPC_FORMAT = MSG_REGUL_FORMAT;
    const std::string TOPIC = "~/robosub_auv/regul";
};

struct RegulPipeTag {
    typedef MSG_REGUL_TYPE RecieveMsg;
    typedef IPCReciever<RecieveMsg, RegulPipeConsts> RecieverClass;

    typedef gazebo::msgs::Vector3d ForwardMsg;
    typedef GazeboForwarder<ForwardMsg, RegulPipeConsts> ForwarderClass;
};

template<typename PipeTag>
class TransportPipe {
public:
    TransportPipe(gazebo::transport::NodePtr node)
        : reciever_([&](const typename PipeTag::RecieveMsg& msg) {this->on_recieve(msg);}, node)
        , forwarder_(node)
    { }

    void on_recieve(const typename PipeTag::RecieveMsg& msg) {

    }

private:
    typename PipeTag::RecieverClass reciever_;
    typename PipeTag::ForwarderClass forwarder_;
};