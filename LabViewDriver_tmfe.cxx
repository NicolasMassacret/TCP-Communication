//
// fetest_tmfe.cxx
//
// Frontend for test and example of tmfe c++ frontend
//

#include <stdio.h>
#include <signal.h> // SIGPIPE
#include <assert.h> // assert()
#include <stdlib.h> // malloc()
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

#include "midas.h"
#include "tmfe.h"

#include "KOtcp.h"
#include "feTCP.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;

#define NCH 12

class feLabview :
   public feTCP
{
public:
   TMFE* fMfe;
   TMFeEquipment* fEq;

   int fEventSize;
   char* fEventBuf;

   feLabview(TMFE* mfe, TMFeEquipment* eq):feTCP(mfe, eq) // ctor
   {
      fMfe = mfe;
      fEq  = eq;
      fEventSize = 0;
      fEventBuf  = NULL;
   }

   ~feLabview() // dtor
   {
      if (fEventBuf) {
         free(fEventBuf);
         fEventBuf = NULL;
      }
   }

   void Init()
   {
      fEventSize = 100;
      fEq->fOdbEqSettings->RI("event_size", &fEventSize, true);
      if (fEventBuf) {
         free(fEventBuf);
      }
      fEventBuf = (char*)malloc(fEventSize);

      fEq->fOdbEqSettings->RSA("LabV_monitored_variables", &settings.LabVar, true, NCH);
      fEq->fOdbEqSettings->RB("Dynamic_mode", &settings.dynamic, true);
      fEq->fOdbEqSettings->RF("Rotation_position", &settings.Rot_position, true);
      fEq->fOdbEqSettings->RF("Translation_position", &settings.Trans_position, true);
      fEq->fOdbEqVariables->RDA("LabV_var_val", &variables, true, NCH);
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


private:
   INT read_event(char *pevent, INT off);
   typedef struct {
      string      hostname;
      string    port;
      bool      dynamic;

      float    Rot_position;
      float     Trans_position;
      vector<string>     LabVar;
      vector<string>     names;
   } ldsettings;
   ldsettings settings;

   vector<double> variables;
};

INT feLabview::read_event(char *pevent, INT off)
{
   /* init bank structure */



   int  size;
   float value;

   if(settings.dynamic==true)
      {
         /*Update variables to monitor and destination in ODB*/
         // char monitored_var_str[80];
         // sprintf(monitored_var_str, "/Equipment/%s/Settings/LabV_monitored_variables", fMfe->fFrontendName);
         // db_find_key(hDB, 0, monitored_var_str, &hVar);
         // size=sizeof(settings.LabVar);
         // db_get_record(hDB, hVar, &settings.LabVar, &size, 0);
         fEq->fOdbEqSettings->RSA("LabV_monitored_variables", &settings.LabVar, true, NCH);
      }

   /*Read variables and write their value in ODB*/
   for(int a=0; a<settings.LabVar.size(); a++)
      {
         if(settings.LabVar[a].size())
            {
               //printf("Reading variable : %s \n", LabDriver_settings.LabVar[a]);
               string resp=Exchange(string("read_")+settings.LabVar[a]+"\r\n");
               try{
                  value = std::stof(resp);
               }
               catch (const std::invalid_argument& ia){
                  value=-99999;
                  printf("Received empty message from client. \n");
               }

               printf("Value returned: %f \n", value);

               // sprintf(destination_var_str, "/Equipment/%s/Variables/LabV_var_val", fMfe->fFrontendName);
               // db_find_key(hDB, 0, destination_var_str, &hVar);
               // size=sizeof(variables);
               // db_get_record(hDB, hVar, &variables, &size, 0);
               variables[a]=value;
               // db_set_value(hDB, 0, destination_var_str, variables, sizeof(variables), NCH, TID_FLOAT);

            }
      }
   fEq->fOdbEqVariables->WDA("LabV_var_val", variables);

   // // /* Write data bank*/
   // bk_init(pevent);
   // float *pfdata;
   // bk_create(pevent, "VLAB", TID_FLOAT, (void **)&pfdata);
   // for (int i=0; i<NCH; i++) {
   //    *pfdata++ = (float) (variables[i]);
   // }
   // bk_close(pevent, pfdata);

   // return bk_size(pevent);
   return 0;
}

static void usage()
{
   fprintf(stderr, "Usage: LabViewDriver_tmfe.exe <name> ...\n");
   exit(1);
}

int main(int argc, char* argv[])
{
   setbuf(stdout, NULL);
   setbuf(stderr, NULL);

   signal(SIGPIPE, SIG_IGN);

   std::string name = "";

   if (argc == 2) {
      name = argv[1];
   } else {
      usage(); // DOES NOT RETURN
   }

   TMFE* mfe = TMFE::Instance();

   TMFeError err = mfe->Connect("LabViewDriver_tmfe", __FILE__);
   if (err.error) {
      printf("Cannot connect, bye.\n");
      return 1;
   }

   //mfe->SetWatchdogSec(0);

   TMFeCommon *common = new TMFeCommon();
   common->EventID = 1;
   common->LogHistory = 1;
   //common->Buffer = "SYSTEM";

   TMFeEquipment* eq = new TMFeEquipment(mfe, "LabViewDriver_tmfe", common);
   eq->Init();
   eq->SetStatus("Starting...", "white");
   eq->ZeroStatistics();
   eq->WriteStatistics();

   mfe->RegisterEquipment(eq);

   feLabview* myfe = new feLabview(mfe, eq);

   mfe->RegisterRpcHandler(myfe);

   //mfe->SetTransitionSequenceStart(910);
   //mfe->SetTransitionSequenceStop(90);
   //mfe->DeregisterTransitionPause();
   //mfe->DeregisterTransitionResume();

   myfe->Init();

   mfe->RegisterPeriodicHandler(eq, myfe);

   eq->SetStatus("Started...", "white");

   while (!mfe->fShutdownRequested) {
      mfe->PollMidas(10);
   }

   mfe->Disconnect();

   return 0;
}

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
