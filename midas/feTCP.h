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

/** \brief Generic frontend for communicating with a TPC/IP server
 * using KO's KOtcpConnection class
 */
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
   string fPortnum ="8888";
   //int fPortnum = 4711;
   int fSockFd;

   feTCP(TMFE* mfe /**< [in]  Midas frontend interfacing class. */,
         TMFeEquipment* eq /**< [in]  Midas equipment class. */) // ctor
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

   /** \brief Midas event creation. */
   void SendEvent(double dvalue)
   {
      fEq->ComposeEvent(fEventBuf, fEventSize);
      fEq->BkInit(fEventBuf, fEventSize);

      double* ptr = (double*)fEq->BkOpen(fEventBuf, "test", TID_DOUBLE);
      *ptr = dvalue;
      fEq->BkClose(fEventBuf, ptr+1);

      fEq->SendEvent(fEventBuf);
   }

   /** \brief JSON rpc interface <b>CURRENTLY UNUSED</b>. */
   std::string HandleRpc(const char* cmd, const char* args)
      {
         fMfe->Msg(MINFO, "HandleRpc", "RPC cmd [%s], args [%s]", cmd, args);
         return "OK";
      }


   void HandleBeginRun()
   {
      fMfe->Msg(MINFO, "HandleBeginRun", "Begin run!");
      /* fEq->SetStatus("Running", "#00FF00"); */
   }

   void HandleEndRun()
   {
      fMfe->Msg(MINFO, "HandleEndRun", "End run!");
      /* fEq->SetStatus("Stopped", "#00FF00"); */
   }

   void HandlePeriodic()
   {
      printf("periodic!\n");
      //char buf[256];
      //sprintf(buf, "buffered %d (max %d), dropped %d, unknown %d, max flushed %d", gUdpPacketBufSize, fMaxBuffered, fCountDroppedPackets, fCountUnknownPackets, fMaxFlushed);
      //fEq->SetStatus(buf, "#00FF00");
      //fEq->WriteStatistics();
   }

   /** \brief Open TCP connection and set parameters. */
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
      tcp->fConnectTimeoutMilliSec = 500;
      tcp->fReadTimeoutMilliSec = 2000;
      tcp->fWriteTimeoutMilliSec = 500;
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

   /** \brief Send string over TCP, optionally receive reply.
    *
    * \param message text to be sent to server
    * \param expect_reply try to receive a response
    */
   string Exchange(string message, bool expect_reply = true, string expected = ""){
      string resp;
      if(tcp){
         if(tcp->fConnected){
            KOtcpError err;
            err = tcp->WriteString(message);
            if(err.error){
               cerr << err.message << endl;
               return resp;
            }
            if(expect_reply){
               err = tcp->ReadString(&resp,4096);
               if(!resp.size()){
                  usleep(100000);
                  err = tcp->ReadString(&resp,4096);
                  if(!resp.size()){
                     cerr << "Response empty even on second try" << endl;
                     return resp;
                  }
               }
               if(err.error){
                  cerr << err.message << endl;
                  return resp;
               }
               if(expected.size()){
                  if(resp.find(expected) != 0){
                     cerr << "Did not receive expected string \"" << expected << "\" at beginning of response, response was " << resp << endl;
                     return "";
                  }
               }
            }
         }
      }
      return resp;
   }
};

#endif

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
