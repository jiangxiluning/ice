// **********************************************************************
//
// Copyright (c) 2003-2016 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#import <objc/Ice.h>
#import <ami/TestI.h>
#import <TestCommon.h>
 
static int
run(id<ICECommunicator> communicator)
{
    [[communicator getProperties] setProperty:@"TestAMIAdapter.Endpoints" value:@"default -p 12010:udp"];
    [[communicator getProperties] setProperty:@"ControllerAdapter.Endpoints" value:@"tcp -p 12011"];
    [[communicator getProperties] setProperty:@"ControllerAdapter.ThreadPool.Size" value:@"1"];

    id<ICEObjectAdapter> adapter = [communicator createObjectAdapter:@"TestAMIAdapter"];
    id<ICEObjectAdapter> adapter2 = [communicator createObjectAdapter:@"ControllerAdapter"];

    TestAMITestIntfControllerI* testController 
        = ICE_AUTORELEASE([[TestAMITestIntfControllerI alloc] initWithAdapter:adapter]);

    [adapter add:[TestAMITestIntfI testIntf] identity:[communicator stringToIdentity:@"test"]];
    [adapter activate];

    [adapter2 add:testController identity:[communicator stringToIdentity:@"testController"]];
    [adapter2 activate];

    serverReady(communicator);

    [communicator waitForShutdown];
    return EXIT_SUCCESS;
}

#if TARGET_OS_IPHONE
#  define main amiServer
#endif

int
main(int argc, char* argv[])
{
    int status;
    @autoreleasepool
    {
        id<ICECommunicator> communicator = nil;

        @try
        {
            ICEInitializationData* initData = [ICEInitializationData initializationData];
            initData.properties = defaultServerProperties(&argc, argv);

            //
            // Its possible to have batch oneway requests dispatched after
            // the adapter is deactivated due to thread scheduling so we
            // supress this warning.
            //
            [initData.properties setProperty:@"Ice.Warn.Dispatch" value:@"0"];
#if TARGET_OS_IPHONE
        initData.prefixTable__ = [NSDictionary dictionaryWithObjectsAndKeys:
                                  @"TestAMI", @"::Test", 
                                  nil];
#endif
            communicator = [ICEUtil createCommunicator:&argc argv:argv initData:initData];
            status = run(communicator);
        }
        @catch(ICEException* ex)
        {
            tprintf("%@\n", ex);
            status = EXIT_FAILURE;
        }

        if(communicator)
        {
            @try
            {
                [communicator destroy];
            }
            @catch(ICEException* ex)
            {
    	    tprintf("%@\n", ex);
                status = EXIT_FAILURE;
            }
        }
    }
    return status;
}
