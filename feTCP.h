#ifndef FeTCP_H
#define FeTCP_H

#include <stdio.h>
#include <signal.h> // SIGPIPE
#include <assert.h> // assert()
#include <stdlib.h> // malloc()
#include <string>
#include <iostream>
/// replace these with midas tcpip.o?
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "midas.h"
#include "tmfe.h"

#include "KOtcp.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;

class feTCP :
public TMFeRpcHandlerInterface,
    public  TMFePeriodicHandlerInterface
{
 private:
    KOtcpConnection *tcp = NULL;
 public:
    TMFE* fMfe;
    TMFeEquipment* fEq;

    int fEventSize;
    char* fEventBuf;
    string fHostname = "localhost";
    string fPortnum ="port";
    //int fPortnum = 4711;
    int fSockFd;

    feTCP(TMFE* mfe, TMFeEquipment* eq) // ctor
        {
            fMfe = mfe;
            fEq  = eq;
            fEventSize = 0;
            fEventBuf  = NULL;
        }

    ~feTCP() // dtor
        {
            if (fEventBuf) {
                free(fEventBuf);
                fEventBuf = NULL;
            }
            fEq->SetStatus("Stopped", "#FF0000");
        }

    void Init()
    {
        fEventSize = 100;
        fEq->fOdbEqSettings->RI("event_size", &fEventSize, true);
        if (fEventBuf) {
            free(fEventBuf);
        }
        fEventBuf = (char*)malloc(fEventSize);
    }

    void SendEvent(double dvalue)
    {
        fEq->ComposeEvent(fEventBuf, fEventSize);
        fEq->BkInit(fEventBuf, fEventSize);

        double* ptr = (double*)fEq->BkOpen(fEventBuf, "test", TID_DOUBLE);
        *ptr = dvalue;
        fEq->BkClose(fEventBuf, ptr+1);

        fEq->SendEvent(fEventBuf);
    }

    std::string HandleRpc(const char* cmd, const char* args)
        {
            fMfe->Msg(MINFO, "HandleRpc", "RPC cmd [%s], args [%s]", cmd, args);
            return "OK";
        }

    void HandleBeginRun()
    {
        fMfe->Msg(MINFO, "HandleBeginRun", "Begin run!");
        fEq->SetStatus("Running", "#00FF00");
    }

    void HandleEndRun()
    {
        fMfe->Msg(MINFO, "HandleEndRun", "End run!");
        fEq->SetStatus("Stopped", "#00FF00");
    }

    void HandlePeriodic()
    {
        printf("periodic!\n");
        //char buf[256];
        //sprintf(buf, "buffered %d (max %d), dropped %d, unknown %d, max flushed %d", gUdpPacketBufSize, fMaxBuffered, fCountDroppedPackets, fCountUnknownPackets, fMaxFlushed);
        //fEq->SetStatus(buf, "#00FF00");
        //fEq->WriteStatistics();
    }

    bool TCPConnect()
    {
        fEq->fOdbEqSettings->RS("hostname", &fHostname, true);
        //fEq->fOdbEqSettings->RI("port", &fPortnum, true);
        fEq->fOdbEqSettings->RS("port", &fPortnum, true);
        cout << "Hostname: " << fHostname << ", port " << fPortnum << endl;
        if(tcp){
            delete tcp;
        }
        //char portstr[8];
        //sprintf(portstr,"%d",fPortnum);
        tcp = new KOtcpConnection(fHostname.c_str(),fPortnum.c_str());
        //tcp = new KOtcpConnection(fHostname.c_str(), portstr);
        KOtcpError err = tcp->Connect();
        if(err.error){
            cerr << err.message << endl;
            return false;
        } else {
            cout << "Connected." << endl;
            return true;
        }
    }

    string Exchange(string message){
        string resp;
        if(tcp){
            if(tcp->fConnected){
                KOtcpError err = tcp->WriteString(message);
                if(err.error){
                    cerr << err.message << endl;
                    return resp;
                }
                err = tcp->ReadString(&resp,4096);
                if(err.error){
                    cerr << err.message << endl;
                    return resp;
                }
            }
        }
        return resp;
    }
};

#endif
