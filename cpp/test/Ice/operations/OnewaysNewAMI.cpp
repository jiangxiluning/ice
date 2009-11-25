// **********************************************************************
//
// Copyright (c) 2003-2009 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <Ice/Ice.h>
#include <TestCommon.h>
#include <Test.h>

using namespace std;

namespace
{

class CallbackBase : public Ice::LocalObject
{
public:

    CallbackBase() :
        _called(false)
    {
    }

    virtual ~CallbackBase()
    {
    }

    void check()
    {
        IceUtil::Monitor<IceUtil::Mutex>::Lock sync(_m);
        while(!_called)
        {
            _m.wait();
        }
        _called = false;
    }

protected:

    void called()
    {
        IceUtil::Monitor<IceUtil::Mutex>::Lock sync(_m);
        assert(!_called);
        _called = true;
        _m.notify();
    }

private:

    IceUtil::Monitor<IceUtil::Mutex> _m;
    bool _called;
};

typedef IceUtil::Handle<CallbackBase> CallbackBasePtr;

class Callback : public CallbackBase
{
public:

    Callback()
    {
    }

    void sent(bool)
    {
        called();
    }

    void noException(const Ice::Exception&)
    {
        test(false);
    }

    void twowayOnlyException(const Ice::Exception& ex)
    {
        try
        {
            ex.ice_throw();
        }
        catch(const Ice::TwowayOnlyException&)
        {
            called();
        }
        catch(const Ice::Exception&)
        {
            test(false);
        }
    }
};
typedef IceUtil::Handle<Callback> CallbackPtr;

}

void
onewaysNewAMI(const Ice::CommunicatorPtr& communicator, const Test::MyClassPrx& proxy)
{
    Test::MyClassPrx p = Test::MyClassPrx::uncheckedCast(proxy->ice_oneway());

    {
	CallbackPtr cb = new Callback;
        Ice::CallbackPtr callback = Ice::newCallback(cb, &Callback::noException, &Callback::sent);
        p->begin_ice_ping(callback);
        cb->check();
    }

    {
	CallbackPtr cb = new Callback;
        Ice::CallbackPtr callback = Ice::newCallback(cb, &Callback::twowayOnlyException);
        p->begin_ice_isA(Test::MyClass::ice_staticId(), callback);
        cb->check();
    }
    
    {
	CallbackPtr cb = new Callback;
        Ice::CallbackPtr callback = Ice::newCallback(cb, &Callback::twowayOnlyException);
        p->begin_ice_id(callback);
        cb->check();
    }
    
    {
	CallbackPtr cb = new Callback;
        Ice::CallbackPtr callback = Ice::newCallback(cb, &Callback::twowayOnlyException);
        p->begin_ice_ids(callback);
        cb->check();
    }

    {
        {
            CallbackPtr cb = new Callback;
            Ice::CallbackPtr callback = Ice::newCallback(cb, &Callback::noException, &Callback::sent);
            p->begin_opVoid(callback);
            cb->check();
        }
    }

    //
    // Test that calling a twoway operation with a oneway proxy raises TwowayOnlyException.
    //
    {
        CallbackPtr cb = new Callback;
        Ice::CallbackPtr callback = Ice::newCallback(cb, &Callback::twowayOnlyException);
        p->begin_opByte(Ice::Byte(0xff), Ice::Byte(0x0f), callback);
        cb->check();
    }
}