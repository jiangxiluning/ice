// **********************************************************************
//
// Copyright (c) 2003-2005 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#ifndef SESSION_I_H
#define SESSION_I_H

#include <Session.h>
#include <list>

class SessionI : public ::Demo::Session, public IceUtil::Mutex
{
public:

    SessionI(const Ice::ObjectAdapterPtr&, const IceUtil::Time&);
    virtual ~SessionI();

    virtual ::Demo::HelloPrx createHello(const Ice::Current&);
    virtual void refresh(const Ice::Current&);
    virtual void destroy(const Ice::Current&);

    // Return true if the session is destroyed, false otherwise.
    bool destroyed() const;

    // Called when the session is destroyed. This should release any
    // per-client allocated resources.
    void destroyCallback();

private:

    const Ice::ObjectAdapterPtr _adapter;
    const IceUtil::Time _timeout;
    int _nextId;
    bool _destroy;
    IceUtil::Time _refreshTime;
    std::list< ::Demo::HelloPrx> _objs;
};
typedef IceUtil::Handle<SessionI> SessionIPtr;

#endif
